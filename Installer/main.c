/*
 * Ghost - A honeypot for USB malware
 * Copyright (C) 2011-2012	Sebastian Poeplau (sebastian.poeplau@gmail.com)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * 
 * Additional permission under GNU GPL version 3 section 7
 *
 * If you modify this Program, or any covered work, by linking or combining
 * it with the Windows Driver Frameworks (or a modified version of that library),
 * containing parts covered by the terms of the Microsoft Software License Terms -
 * Microsoft Windows Driver Kit, the licensors of this Program grant you additional
 * permission to convey the resulting work.
 * 
 */

#include <version.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <newdev.h>
#include <cfgmgr32.h>
#include <ghostlib.h>


/*
 * Enumerate all devices and find out if a device with the given
 * hardware ID is currently present. If WasPresent is not NULL,
 * then also find out whether such a device has been present before
 * and write the result to WasPresent.
 */
BOOL IsDevicePresent(const char *HardwareID, BOOL *WasPresent) {
	HDEVINFO DevInfoSet;
	SP_DEVINFO_DATA DevInfoData;
	DWORD DeviceIndex;
	BOOL Present = FALSE, PresentBefore = FALSE;
	
	// Create a list of all devices
	DevInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES);
	if (DevInfoSet == INVALID_HANDLE_VALUE) {
		printf("Error: Could not enumerate devices (0x%x)\n", GetLastError());
		exit(-1);
	}
	
	// Iterate through the list
	ZeroMemory(&DevInfoData, sizeof(SP_DEVINFO_DATA));
	DevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	DeviceIndex = 0;
	while (SetupDiEnumDeviceInfo(DevInfoSet, DeviceIndex, &DevInfoData)) {
		DWORD RequiredBufferSize;
		char *Buffer;
		ULONG Status, ProblemNumber;
		
		DeviceIndex++;
		
		// Find the device's hardware ID
		if (!SetupDiGetDeviceRegistryProperty(DevInfoSet, &DevInfoData, SPDRP_HARDWAREID, NULL, NULL, 0, &RequiredBufferSize)) {
			if (GetLastError() == ERROR_INVALID_DATA) {
				continue;
			}
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
				printf("Error: Could not retrieve a device's hardware ID (0x%x)\n", GetLastError());
				exit(-1);
			}
		}
		
		Buffer = malloc(RequiredBufferSize);
		if (!SetupDiGetDeviceRegistryProperty(DevInfoSet, &DevInfoData, SPDRP_HARDWAREID, NULL, Buffer, RequiredBufferSize, NULL)) {
			printf("Error: Could not retrieve a device's hardware ID (0x%x)\n", GetLastError());
			exit(-1);
		}
		
		// Buffer contains multiple strings, but the device we're looking for
		// has only one ID, so we just compare the first string.
		if (strncmp(HardwareID, Buffer, strlen(HardwareID)) != 0) {
			continue;
		}
		
		PresentBefore = TRUE;
		
		// Check whether the device is still present
		if (CM_Get_DevNode_Status(&Status, &ProblemNumber, DevInfoData.DevInst, 0) != CR_NO_SUCH_DEVINST) {
			Present = TRUE;
			break;
		}
	}
	
	SetupDiDestroyDeviceInfoList(DevInfoSet);
	
	if (WasPresent != NULL) {
		*WasPresent = PresentBefore;
	}
	return Present;
}


int __cdecl main(int argc, char *argv[]) {
	TCHAR BusInfFile[MAX_PATH];
	TCHAR DriveInfFile[MAX_PATH];
	DWORD res;
	BOOL NeedRestart = FALSE, TempBool = FALSE, DeviceWasPresent, DeviceIsPresent, Error = FALSE;
	int GhostID;
	
	printf("Ghost version %s installer\n", GHOST_VERSION);
	
	// Obtain full names for the INF files
	res = GetFullPathName(TEXT("ghostbus.inf"), MAX_PATH, BusInfFile, NULL);
	if (res >= MAX_PATH || res == 0) {
		printf("Error: INF file path too long\n");
		return -1;
	}
	res = GetFullPathName(TEXT("ghostdrive.inf"), MAX_PATH, DriveInfFile, NULL);
	if (res >= MAX_PATH || res == 0) {
		printf("Error: INF file path too long\n");
		return -1;
	}
	
	// TODO: Get WDF files
	
	printf("Updating the bus driver...\n");
	if (!IsDevicePresent("root\\ghostbus", NULL)) {
		printf("No bus device present - creating one...\n");
		// TODO
		return -1;
		//
	}
	
	// Update the driver
	if (UpdateDriverForPlugAndPlayDevices(NULL, TEXT("root\\ghostbus"), BusInfFile, 0, &TempBool) == TRUE) {
		printf("Bus driver updated\n");
	}
	else {
		switch (GetLastError()) {
			case ERROR_FILE_NOT_FOUND:
				printf("Error: INF file not found\n");
				return -1;
				break;
			case ERROR_IN_WOW64:
				printf("Error: 64-bit environments are not currently supported\n");
				return -1;
				break;
			case ERROR_NO_SUCH_DEVINST:
				printf("Error: No bus device present\n");
				return -1;
				break;
			case ERROR_NO_MORE_ITEMS:
				printf("Your bus driver is newer than this one\n");
				break;
			default:
				printf("Error: Could not update the bus driver\n");
				return -1;
				break;
		}
	}
	
	NeedRestart |= TempBool;
	
	printf("Updating the function driver\n");
	
	// Find out whether the device is installed already
	DeviceIsPresent = IsDevicePresent("ghostbus\\ghostdrive", &DeviceWasPresent);
	if (DeviceWasPresent) {
		// This is an upgrade
		
		// TODO: Remove old driver
		
		// Mount a device if none is present
		if (!DeviceIsPresent) {
			printf("No device mounted at the moment - mounting one...\n");
			GhostID = GhostMountDevice(NULL, NULL);
		}
		
		// Update the driver
		if (UpdateDriverForPlugAndPlayDevices(NULL, TEXT("ghostbus\\ghostdrive"), DriveInfFile, 0, &TempBool) == TRUE) {
			printf("Function driver updated\n");
		}
		else {
			Error = TRUE;
			switch (GetLastError()) {
				case ERROR_FILE_NOT_FOUND:
					printf("Error: INF file not found\n");
					break;
				case ERROR_IN_WOW64:
					printf("Error: 64-bit environments are not currently supported\n");
					break;
				case ERROR_NO_SUCH_DEVINST:
					printf("Error: No device present\n");
					break;
				case ERROR_NO_MORE_ITEMS:
					printf("Your function driver is newer than this one\n");
					Error = FALSE;
					break;
				default:
					printf("Error: Could not update the function driver\n");
					break;
			}
		}
		
		// Unmount the device
		if (!DeviceIsPresent) {
			// A little ugly, but we have to make sure that autorun is done
			// before we can unmount the device
			Sleep(2000);
			GhostUmountDevice(GhostID);
		}
		
		if (Error) {
			return -1;
		}
	}
	else {
		// This is the first installation - just copy the driver
		if (!SetupCopyOEMInf(DriveInfFile, NULL, SPOST_PATH, 0, NULL, 0, NULL, NULL)) {
			printf("Error: Could not install the function driver (0x%x)\n", GetLastError());
		}
	}
	
	NeedRestart |= TempBool;
	
	if (NeedRestart) {
		printf("You need to restart the machine!\n");
	}
	
	return 0;
}
