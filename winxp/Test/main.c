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

#include "ghostlib.h"

#include <stdio.h>
#include <Windows.h>


void Callback(int DeviceID, int IncidentID, void *Context) {
	// Data was written to the emulated device -
	// obtain information about the write request
	printf("Process ID: %d\n", GhostGetProcessID(DeviceID, IncidentID));
	printf("Thread ID: %d\n", GhostGetThreadID(DeviceID, IncidentID));
}


int __cdecl main(int argc, char *argv[]) {
	int ID;
	
	printf("Mount the device and register a callback function\n");
	ID = GhostMountDevice(Callback, NULL);
	if (ID < 0) {
		printf("Error: %s\n", GhostGetErrorDescription(GhostGetLastError()));
		return -1;
	}
	
	// Wait for the malware to infect the device
	Sleep(10 * 1000);
	
	printf("Unmount the device\n");
	if (GhostUmountDevice(ID) < 0) {
		printf("Error: %s\n", GhostGetErrorDescription(GhostGetLastError()));
		return -1;
	}
	
	return 0;
}