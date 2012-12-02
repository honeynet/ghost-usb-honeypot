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

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <newdev.h>
#include <cfgmgr32.h>
#include <regstr.h>

#include <version.h>
#include <initguid.h>
#include <portctl.h>

#define BUS_INF_KEY "SYSTEM\\CurrentControlSet\\Services\\ghostport\\Parameters"
#define INF_FILE_VALUE "InfFile"


/*
 * Find out if a device with the given hardware ID is currently present.
 */
BOOL IsDevicePresent() {
	HDEVINFO DevInfo;
	SP_DEVICE_INTERFACE_DATA DevInterfaceData;
	
	// Get all devices that support the interface
	DevInfo = SetupDiGetClassDevsEx(&GUID_DEVINTERFACE_GHOST, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT, NULL, NULL, NULL);
	if (DevInfo == INVALID_HANDLE_VALUE) {
		return FALSE;
	}
	
	// Enumerate the device instances
	DevInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	if (!SetupDiEnumDeviceInterfaces(DevInfo, NULL, &GUID_DEVINTERFACE_GHOST, 0, &DevInterfaceData)) {
		SetupDiDestroyDeviceInfoList(DevInfo);
		return FALSE;
	}
	
	SetupDiDestroyDeviceInfoList(DevInfo);
	return TRUE;
}



int __cdecl main(int argc, char *argv[]) {
	char BusInfFile[MAX_PATH];
	char DestinationInf[MAX_PATH], *DestinationInfFile = NULL;
	char OldInf[MAX_PATH];
	char ValueName[MAX_PATH];
	DWORD OldInfLength, Index, ValueNameLength;
	DWORD res;
	BOOL NeedRestart = FALSE;
	HKEY BusInfKey;
	LONG Error;
	char Class[MAX_PATH];
	GUID ClassGuid;
	HDEVINFO DevInfoSet;
	SP_DEVINFO_DATA DevInfo;
	char HardwareID[MAX_PATH];
	
	printf("Ghost version %s installer\n", GHOST_VERSION);
	
	// Obtain full names for the INF file
	res = GetFullPathName(TEXT("ghostport.inf"), MAX_PATH, BusInfFile, NULL);
	if (res >= MAX_PATH || res == 0) {
		printf("Error: INF file path too long\n");
		return -1;
	}
	
	printf("Uninstalling the old bus driver...\n");
	Error = RegCreateKeyEx(HKEY_LOCAL_MACHINE, BUS_INF_KEY, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &BusInfKey, NULL);
	if (Error != ERROR_SUCCESS) {
		printf("Error: Could not open the bus driver's registry key (0x%x)\n", Error);
		return -1;
	}

	Index = 0;
	while (1) {
		ValueNameLength = MAX_PATH;
		OldInfLength = MAX_PATH;
		Error = RegEnumValue(BusInfKey, Index, ValueName, &ValueNameLength, NULL, NULL, OldInf, &OldInfLength);
		if (Error == ERROR_NO_MORE_ITEMS) {
			break;
		}
		if (Error != ERROR_SUCCESS) {
			printf("Error: Could not read registry value (0x%x)\n", Error);
			return -1;
		}

		if (strncmp(ValueName, INF_FILE_VALUE, MAX_PATH) == 0) {
			if (!SetupUninstallOEMInf(OldInf, SUOI_FORCEDELETE, NULL)) {
				printf("Error: Could not uninstall the old driver (0x%x)\n", GetLastError());
			}
			break;
		}

		Index++;
	}
	
	printf("Installing the new bus driver...\n");
	if (!IsDevicePresent()) {
		printf("No bus device present - creating one...\n");
		
		// Read the device class from the INF file (should be System)
		if (!SetupDiGetINFClass(BusInfFile, &ClassGuid, Class, MAX_PATH, NULL)) {
			printf("Error: Could not read the device class from the bus INF file (0x%x)\n", GetLastError());
			return -1;
		}
		
		// Create an empty device list
		DevInfoSet = SetupDiCreateDeviceInfoList(&ClassGuid, NULL);
		if (DevInfoSet == INVALID_HANDLE_VALUE) {
			printf("Error: Could not create a device info set (0x%x)\n", GetLastError());
			return -1;
		}
		
		// Create the new device
		DevInfo.cbSize = sizeof(SP_DEVINFO_DATA);
		if (!SetupDiCreateDeviceInfo(DevInfoSet, Class, &ClassGuid, NULL, NULL, DICD_GENERATE_ID, &DevInfo)) {
			printf("Error: Could not create the new device (0x%x)\n", GetLastError());
			return -1;
		}
		
		// Set the new device's hardware ID
		ZeroMemory(HardwareID, MAX_PATH);
		strncpy(HardwareID, "root\\ghostport", MAX_PATH);
		if (!SetupDiSetDeviceRegistryProperty(DevInfoSet, &DevInfo, SPDRP_HARDWAREID, HardwareID, strlen(HardwareID) + 2)) {
			printf("Error: Could not set the new device's hardware ID (0x%x)\n", GetLastError());
			return -1;
		}
		
		// Call the class installer to register the new device
		if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, DevInfoSet, &DevInfo)) {
			printf("Error: Could not register the new device (0x%x)\n", GetLastError());
			return -1;
		}
		
		SetupDiDestroyDeviceInfoList(DevInfoSet);
	}
	
	// Copy the new INF file to the driver store
	if (!SetupCopyOEMInf(BusInfFile, NULL, SPOST_PATH, 0, DestinationInf, MAX_PATH, NULL, &DestinationInfFile)) {
		printf("Error: Could not install the bus driver (0x%x)\n", GetLastError());
		return -1;
	}

	// Write the bus driver's INF file name to the registry
	Error = RegSetValueEx(BusInfKey, INF_FILE_VALUE, 0, REG_SZ, DestinationInfFile, strlen(DestinationInfFile) + 1);
	if (Error != ERROR_SUCCESS) {
		printf("Error: Could not write the INF file name to the registry (0x%x)\n", Error);
		return -1;
	}

	RegCloseKey(BusInfKey);
	
	// Update the bus driver
	if (UpdateDriverForPlugAndPlayDevices(NULL, TEXT("root\\ghostport"), BusInfFile, 0, &NeedRestart) == TRUE) {
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
	
	printf("Done\n");
	
	if (NeedRestart) {
		printf("You need to restart the machine!\n");
	}
	
	return 0;
}
