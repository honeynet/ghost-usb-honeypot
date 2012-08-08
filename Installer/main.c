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
#include <windows.h>
#include <newdev.h>
#include <ghostlib.h>


int __cdecl main(int argc, char *argv[]) {
	TCHAR BusInfFile[MAX_PATH];
	TCHAR DriveInfFile[MAX_PATH];
	DWORD res;
	BOOL NeedRestart = FALSE, TempBool;
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
	// TODO: remove INSTALLFLAG_FORCE
	if (UpdateDriverForPlugAndPlayDevices(NULL, TEXT("root\\ghostbus"), BusInfFile, INSTALLFLAG_FORCE, &TempBool) == TRUE) {
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
				printf("No bus device present - creating one...\n");
				// TODO
				break;
			case ERROR_NO_MORE_ITEMS:
				printf("Error: Your driver is newer than this one\n");
				return -1;
				break;
			default:
				printf("Error: Could not update the bus driver\n");
				return -1;
				break;
		}
	}
	
	NeedRestart |= TempBool;
	
	printf("Updating the function driver\n");
	
	// TODO: find out whether first install and handle
	
	// TODO: remove INSTALLFLAG_FORCE
	if (UpdateDriverForPlugAndPlayDevices(NULL, TEXT("ghostbus\\ghostdrive"), DriveInfFile, INSTALLFLAG_FORCE, &TempBool) == TRUE) {
		printf("Function driver updated\n");
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
				printf("No device mounted at the moment - mounting one...\n");
				GhostID = GhostMountDevice(NULL, NULL);
				if (UpdateDriverForPlugAndPlayDevices(NULL, TEXT("ghostbus\\ghostdrive"), DriveInfFile, INSTALLFLAG_FORCE, &TempBool) == FALSE) {
					printf("Error: Could not update the function driver\n");
					return -1;
				}
				GhostUmountDevice(GhostID);
				printf("Function driver updated\n");
				break;
			case ERROR_NO_MORE_ITEMS:
				printf("Error: Your driver is newer than this one\n");
				return -1;
				break;
			default:
				printf("Error: Could not update the function driver\n");
				return -1;
				break;
		}
	}
	
	NeedRestart |= TempBool;
	
	if (NeedRestart) {
		printf("You need to restart the machine!\n");
	}
	
	return 0;
}
