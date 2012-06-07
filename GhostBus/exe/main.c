/*
 * Ghost - A honeypot for USB malware
 * Copyright (C) 2011 to 2012  Sebastian Poeplau (sebastian.poeplau@gmail.com)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

#include <ghostbus.h>
#include <ghostdrive_io.h>
#include <version.h>



void print_help() {
	printf("Usage: GhostTool mount <ID>\n"
		"       GhostTool umount <ID>\n"
		"\n"
		"The image file C:\\gd<ID>.img is created with a capacity of 100 MB if necessary.\n");
}


void mount_image(int ID) {
	HANDLE hDevice;
	DWORD BytesReturned;
	BOOL result;
	LONG lID = ID;
	char dosdevice[] = "\\\\.\\GhostDrive0";

	dosdevice[14] = ID + 0x30;

	printf("Opening bus device...\n");

	hDevice = CreateFile("\\\\.\\GhostBus",
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Error: Could not open bus device\n");
		return;
	}

	printf("Activating GhostDrive...\n");

	result = DeviceIoControl(hDevice,
		IOCTL_GHOST_BUS_ENABLE_DRIVE,
		&lID,
		sizeof(LONG),
		NULL,
		0,
		&BytesReturned,
		NULL);

	if (result != TRUE) {
		printf("Error: Coult not send IOCTL code: %d\n", GetLastError());
		return;
	}

	CloseHandle(hDevice);

	printf("Finished\n");
}


void umount_image(int ID) {
	HANDLE hDevice;
	DWORD BytesReturned;
	BOOL result;
	LONG lID;
	char dosdevice[] = "\\\\.\\GhostDrive0";
	BOOLEAN Written = FALSE;

	dosdevice[14] = ID + 0x30;
	lID = ID;



	printf("Opening GhostDrive...\n");

	hDevice = CreateFile(dosdevice,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hDevice != INVALID_HANDLE_VALUE) {
		printf("Sending umount command...\n");

		result = DeviceIoControl(hDevice,
			IOCTL_GHOST_DRIVE_UMOUNT_IMAGE,
			NULL,
			0,
			&Written,
			sizeof(Written),
			&BytesReturned,
			NULL);

		if (result == FALSE) {
			printf("Error: IOCTL code failed: %d\n", GetLastError());
			return;
		}

		CloseHandle(hDevice);

		if (BytesReturned > 0) {
			if (Written == TRUE) {
				printf("Image has been written to!\n");
			}
			else {
				printf("No data has been written to the image\n");
			}
		}
		else {
		// This should not happen
			printf("The driver did not provide write state information\n");
		}
	}
	else {
		printf("Error: Could not open %s\n", dosdevice);
	}


	printf("Opening bus device...\n");

	hDevice = CreateFile("\\\\.\\GhostBus",
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Error: Could not open bus device\n");
		return;
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

	if (Written == TRUE) {
		MessageBox(NULL, "Data has been written to the image. This might indicate an infection!", "GhostDrive", MB_OK | MB_ICONEXCLAMATION);
	}

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