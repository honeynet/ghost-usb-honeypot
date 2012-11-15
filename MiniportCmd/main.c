/*
 * Ghost - A honeypot for USB malware
 * Copyright (C) 2011-2012  Sebastian Poeplau (sebastian.poeplau@gmail.com)
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

#include <shlobj.h>
#include <stdio.h>
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>

#include <ghostbus.h>
#include <version.h>
#include <ntddscsi.h>

#include <initguid.h>
#include <portctl.h>



void print_help() {
	printf("Usage: GhostTool mount <ID>\n"
		"       GhostTool umount <ID>\n"
		"\n"
		"The image file C:\\gd<ID>.img is created with a capacity of 100 MB if necessary.\n");
}


void PrintWriterInfo(PGHOST_DRIVE_WRITER_INFO_RESPONSE WriterInfo) {
	int i;
	
	printf("\n");
	printf("PID: %d\n", WriterInfo->ProcessId);
	printf("TID: %d\n", WriterInfo->ThreadId);
	printf("Loaded modules:\n");
	for (i = 0; i < WriterInfo->ModuleNamesCount; i++) {
		printf("%S\n", (PWCHAR) ((PCHAR) WriterInfo + WriterInfo->ModuleNameOffsets[i]));
	}
	printf("\n");
}


PGHOST_DRIVE_WRITER_INFO_RESPONSE GetWriterInfo(HANDLE Device, USHORT Index, BOOLEAN Block, ULONG DeviceID) {
	GHOST_DRIVE_WRITER_INFO_PARAMETERS WriterInfoParams;
	BOOL result;
	SIZE_T RequiredSize;
	DWORD BytesReturned;
	PGHOST_DRIVE_WRITER_INFO_RESPONSE WriterInfo;
	
	WriterInfoParams.DeviceID = DeviceID;
	WriterInfoParams.Block = Block;
	WriterInfoParams.Index = Index;
	
	result = DeviceIoControl(Device,
		IOCTL_GHOST_DRIVE_GET_WRITER_INFO,
		&WriterInfoParams,
		sizeof(GHOST_DRIVE_WRITER_INFO_PARAMETERS),
		&RequiredSize,
		sizeof(SIZE_T),
		&BytesReturned,
		NULL);

	if (result == FALSE) {
		// Probably no more data available
		printf("No more data\n");
		return NULL;
	}
	
	//printf("%d bytes returned\n", BytesReturned);
	//printf("Need %d bytes\n", RequiredSize);
	
	WriterInfo = malloc(RequiredSize);
	//printf("Retrieving actual data...\n");
	
	result = DeviceIoControl(Device,
		IOCTL_GHOST_DRIVE_GET_WRITER_INFO,
		&WriterInfoParams,
		sizeof(GHOST_DRIVE_WRITER_INFO_PARAMETERS),
		WriterInfo,
		RequiredSize,
		&BytesReturned,
		NULL);

	if (result == FALSE) {
		printf("Error: IOCTL code failed: %d\n", GetLastError());
		free(WriterInfo);
		return NULL;
	}
	
	//printf("Returned %d bytes\n", BytesReturned);
	return WriterInfo;
}


void mount_image(int ID) {
	HANDLE hDevice;
	DWORD BytesReturned;
	BOOL result;
	PGHOST_DRIVE_WRITER_INFO_RESPONSE WriterInfo;
	HDEVINFO DevInfo;
	SP_DEVICE_INTERFACE_DATA DevInterfaceData;
	DWORD RequiredSize;
	PSP_DEVICE_INTERFACE_DETAIL_DATA DevInterfaceDetailData;
	PREQUEST_PARAMETERS Parameters;
	
	printf("Searching bus device...\n");
	
	// Get all devices that support the interface
	DevInfo = SetupDiGetClassDevsEx(&GUID_DEVINTERFACE_GHOST, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT, NULL, NULL, NULL);
	if (DevInfo == INVALID_HANDLE_VALUE) {
		printf("Error: Unable to obtain candidates (0x%x)\n", GetLastError());
		return;
	}
	
	// Enumerate the device instances
	DevInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	if (!SetupDiEnumDeviceInterfaces(DevInfo, NULL, &GUID_DEVINTERFACE_GHOST, 0, &DevInterfaceData)) {
		printf("Error: Unable to find an instance of the interface (0x%x)\n", GetLastError());
		return;
	}
	
	// Obtain details
	SetupDiGetDeviceInterfaceDetail(DevInfo, &DevInterfaceData, NULL, 0, &RequiredSize, NULL);
	DevInterfaceDetailData = malloc(RequiredSize);
	DevInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
	if (!SetupDiGetDeviceInterfaceDetail(DevInfo, &DevInterfaceData, DevInterfaceDetailData, RequiredSize, NULL, NULL)) {
		printf("Error: Unable to obtain details of the interface instance\n");
		free(DevInterfaceDetailData);
		return;
	}
	
	SetupDiDestroyDeviceInfoList(DevInfo);

	printf("Opening bus device at %s...\n", DevInterfaceDetailData->DevicePath);

	hDevice = CreateFile(DevInterfaceDetailData->DevicePath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
		
	free(DevInterfaceDetailData);

	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Error: Could not open bus device\n");
		return;
	}

	printf("Activating GhostDrive...\n");
	
	Parameters = malloc(sizeof(REQUEST_PARAMETERS) + strlen("\\DosDevices\\C:\\gd7.img") * sizeof(WCHAR));
	Parameters->MagicNumber = GHOST_MAGIC_NUMBER;
	Parameters->Opcode = OpcodeEnable;
	Parameters->DeviceID = 7;
	wcscpy(Parameters->ImageName, L"\\DosDevices\\C:\\gd7.img");
	Parameters->ImageNameLength = strlen("\\DosDevices\\C:\\gd7.img");

	result = DeviceIoControl(hDevice,
		IOCTL_MINIPORT_PROCESS_SERVICE_IRP,
		Parameters,
		sizeof(REQUEST_PARAMETERS) + strlen("\\DosDevices\\C:\\gd7.img") * sizeof(WCHAR),
		NULL,
		0,
		&BytesReturned,
		NULL);

	if (result != TRUE) {
		printf("Error: Coult not send IOCTL code: %d\n", GetLastError());
		return;
	}

	CloseHandle(hDevice);
	
	/*printf("Opening GhostDrive...\n");

	hDevice = CreateFile(dosdevice,
		0,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Error: Could not open GhostDrive: %d\n", GetLastError());
		return;
	}
	
	WriterInfo = GetWriterInfo(hDevice, 0, TRUE);
	if (WriterInfo != NULL) {
		PrintWriterInfo(WriterInfo);
		free(WriterInfo);
	}
	else {
		printf("Did not receive writer info\n");
	}*/

	printf("Finished\n");
}


void umount_image(int ID) {
	HANDLE hDevice;
	DWORD BytesReturned;
	BOOL result;
	ULONG lID = ID;
	BOOLEAN Written = FALSE;
	PGHOST_DRIVE_WRITER_INFO_RESPONSE WriterInfo;
	USHORT i;
		
	printf("Opening bus device...\n");

	hDevice = CreateFile("\\\\.\\GhostBus",
		0,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Error: Could not open bus device\n");
		return;
	}
	
	printf("Querying writer info...\n");
	
	for (i = 0; i < MAX_NUM_WRITER_INFO; i++) {
		WriterInfo = GetWriterInfo(hDevice, i, FALSE, lID);
		if (WriterInfo != NULL) {
			PrintWriterInfo(WriterInfo);
			free(WriterInfo);
		}
		else {
			break;
		}
	}

	printf("Deactivating GhostDrive...\n");

	result = DeviceIoControl(hDevice,
		IOCTL_GHOST_BUS_DISABLE_DRIVE,
		&lID,
		sizeof(LONG),
		NULL,
		0,
		&BytesReturned,
		NULL);

	if (result == FALSE) {
		printf("Error: IOCTL code failed: %d\n", GetLastError());
		return;
	}

	CloseHandle(hDevice);

	// if (Written == TRUE) {
	// 		MessageBox(NULL, "Data has been written to the image. This might indicate an infection!", "GhostDrive", MB_OK | MB_ICONEXCLAMATION);
	// 	}

	printf("Finished\n");
}


int __cdecl main(int argc, char *argv[])
{
	int ID;
	char *image;
	
	printf("GhostTool version %s\n", GHOST_VERSION);

	if (argc < 3) {
		print_help();
		return 0;
	}

	ID = (int)argv[2][0] - 0x30;
	if (ID < 0 || ID > 9) {
		printf("Error: ID must be between 0 and 9\n");
		return -1;
	}

	if (!strcmp(argv[1], "mount")) {
		/* we are going to mount an image */
		mount_image(ID);
	}
	else if (!strcmp(argv[1], "umount")) {
		/* we have to umount an image */
		umount_image(ID);
	}
	else {
		print_help();
	}

	return 0;
}