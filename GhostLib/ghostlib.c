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
#include "devicelist.h"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winioctl.h>

#include <ghostbus.h>
#include <ghostdrive_io.h>
#include <version.h>


static int LastError = 0;
static const char *ErrorDescriptions[] = {
	"No error", // 0
	"Could not open the bus device", // 1
	"Could not plug in the virtual USB device", // 2
	"Could not unplug the virtual USB device", // 3
	"Could not start the info thread", // 4
	"The supplied device ID is invalid", // 5
	"Could not initialize an event object", // 6
	"Invalid device ID", // 7
	"Could not set an event object" // 8
};


BOOL WINAPI DllMain(HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			DeviceListInit();
			break;
			
		case DLL_PROCESS_DETACH:
			DeviceListDestroy();
			break;
	}
	
	return TRUE;
}


/*void _PrintWriterInfo(PGHOST_DRIVE_WRITER_INFO_RESPONSE WriterInfo) {
	int i;
	
	printf("\n");
	printf("PID: %d\n", WriterInfo->ProcessId);
	printf("TID: %d\n", WriterInfo->ThreadId);
	printf("Loaded modules:\n");
	for (i = 0; i < WriterInfo->ModuleNamesCount; i++) {
		printf("%S\n", (PWCHAR) ((PCHAR) WriterInfo + WriterInfo->ModuleNameOffsets[i]));
	}
	printf("\n");
}*/


DWORD WINAPI InfoThread(LPVOID Parameter) {
	PGHOST_DEVICE GhostDevice;
	char dosdevice[] = "\\\\.\\GhostDrive0";
	HANDLE hDevice;
	PGHOST_DRIVE_WRITER_INFO_RESPONSE WriterInfo;
	USHORT i = 0;
	PGHOST_INCIDENT Incident;
	OVERLAPPED Overlapped;
	HANDLE Events[2];
	
	if (Parameter == NULL) {
		return -1;
	}

	ZeroMemory(&Overlapped, sizeof(OVERLAPPED));
	Overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (Overlapped.hEvent == NULL) {
		return -1;
	}
	
	GhostDevice = (PGHOST_DEVICE) Parameter;
	dosdevice[14] = GhostDevice->DeviceID + 0x30;
	
	Events[0] = GhostDevice->StopEvent;
	Events[1] = Overlapped.hEvent;
	
	if (GhostDevice->Callback == NULL) {
		return -1;
	}
	
	// Open the device
	hDevice = CreateFile(dosdevice,
		0,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
		NULL);

	if (hDevice == INVALID_HANDLE_VALUE) {
		return -1;
	}
	
	// Wait for writer information
	while (1) {
		GHOST_DRIVE_WRITER_INFO_PARAMETERS WriterInfoParams;
		SIZE_T RequiredSize;
		DWORD BytesReturned;
		PGHOST_DRIVE_WRITER_INFO_RESPONSE WriterInfo;
		
		WriterInfoParams.Block = TRUE;
		WriterInfoParams.Index = i;

		DeviceIoControl(hDevice,
			IOCTL_GHOST_DRIVE_GET_WRITER_INFO,
			&WriterInfoParams,
			sizeof(GHOST_DRIVE_WRITER_INFO_PARAMETERS),
			&RequiredSize,
			sizeof(SIZE_T),
			&BytesReturned,
			&Overlapped);
		
		// Wait until either we've received information or we're told to terminate
		if (WaitForMultipleObjects(2, Events, FALSE, INFINITE) == WAIT_OBJECT_0) {
			break;
		}

		if (Overlapped.Internal != 0) {
			// Probably no more data available
			//printf("No more data\n");
			break;
		}
		
		ResetEvent(Overlapped.hEvent);

		//printf("%d bytes returned\n", BytesReturned);
		//printf("Need %d bytes\n", RequiredSize);

		WriterInfo = malloc(RequiredSize);
		ZeroMemory(WriterInfo, RequiredSize);
		//printf("Retrieving actual data...\n");

		DeviceIoControl(hDevice,
			IOCTL_GHOST_DRIVE_GET_WRITER_INFO,
			&WriterInfoParams,
			sizeof(GHOST_DRIVE_WRITER_INFO_PARAMETERS),
			WriterInfo,
			RequiredSize,
			&BytesReturned,
			&Overlapped);
			
		// Wait until either we've received information or we're told to terminate
		if (WaitForMultipleObjects(2, Events, FALSE, INFINITE) == WAIT_OBJECT_0) {
			break;
		}

		if (Overlapped.Internal != 0) {
			//printf("Error: IOCTL code failed: %d\n", GetLastError());
			free(WriterInfo);
			break;
		}
		
		ResetEvent(Overlapped.hEvent);
		
		// Are we supposed to terminate?
		/*if (WaitForSingleObject(GhostDevice->StopEvent, 0) == WAIT_OBJECT_0) {
			break;
		}*/
		
		Incident = malloc(sizeof(GHOST_INCIDENT));
		Incident->IncidentID = i;
		Incident->WriterInfo = WriterInfo;
		Incident->Next = GhostDevice->Incidents;
		GhostDevice->Incidents = Incident;
		
		// Invoke the callback function
		GhostDevice->Callback(GhostDevice->DeviceID, Incident->IncidentID, GhostDevice->Context);
		
		i++;
	}
	
	CancelIo(hDevice);
	CloseHandle(hDevice);
	CloseHandle(Overlapped.hEvent);
	return 0;
}


int DLLCALL GhostMountDevice(GhostIncidentCallback Callback, void *Context) {
	return GhostMountDeviceWithID(-1, Callback, Context);
}


int DLLCALL GhostMountDeviceWithID(int DeviceID, GhostIncidentCallback Callback, void *Context) {
	PGHOST_DEVICE GhostDevice;
	
	GhostDevice = (PGHOST_DEVICE) GhostMountDeviceWithIDNoThread(DeviceID, Callback, Context);
	if (GhostDevice == NULL) {
		return -1;
	}
	
	// Start a thread that waits for incidents
	if (Callback != NULL) {
		GhostDevice->InfoThread = CreateThread(NULL, 0, InfoThread, GhostDevice, 0, NULL);
		if (GhostDevice->InfoThread == NULL) {
			LastError = 4;
			return -1;
		}
	}
	else {
		GhostDevice->StopEvent = NULL;
		GhostDevice->InfoThread = NULL;
	}
	
	return GhostDevice->DeviceID;
}


void * DLLCALL GhostMountDeviceWithIDNoThread(int DeviceID, GhostIncidentCallback Callback, void *Context) {
	HANDLE hDevice;
	DWORD BytesReturned;
	BOOL result;
	LONG OutDeviceID;
	PGHOST_DEVICE GhostDevice;
	
	// Check the device ID
	if (DeviceID < -1 || DeviceID > 9) {
		LastError = 5;
		return NULL;
	}
	
	// Open the bus device
	hDevice = CreateFile("\\\\.\\GhostBus",
		0,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hDevice == INVALID_HANDLE_VALUE) {
		LastError = 1;
		return NULL;
	}

	// "Plug in" the virtual USB device
	result = DeviceIoControl(hDevice,
		IOCTL_GHOST_BUS_ENABLE_DRIVE,
		&DeviceID,
		sizeof(LONG),
		&OutDeviceID,
		sizeof(LONG),
		&BytesReturned,
		NULL);

	if (result != TRUE || BytesReturned != sizeof(LONG)) {
		LastError = 2;
		return NULL;
	}

	CloseHandle(hDevice);
	
	GhostDevice = DeviceListCreateDevice(OutDeviceID);
	GhostDevice->Callback = Callback;
	GhostDevice->Context = Context;
	GhostDevice->Incidents = NULL;
	GhostDevice->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (GhostDevice->StopEvent == NULL) {
		LastError = 6;
		return NULL;
	}
	DeviceListAdd(GhostDevice);

	return GhostDevice;
}

int DLLCALL GhostUmountDevice(int DeviceID) {
	HANDLE hDevice;
	DWORD BytesReturned;
	BOOL result;
	LONG lID = DeviceID;
	BOOLEAN Written = FALSE;
	PGHOST_DRIVE_WRITER_INFO_RESPONSE WriterInfo;
	USHORT i;
	PGHOST_DEVICE Device;
	
	// Find context information about the device
	Device = DeviceListGet(DeviceID);
	if (Device == NULL) {
		LastError = 7;
		return -1;
	}

	// Open the bus device
	hDevice = CreateFile("\\\\.\\GhostBus",
		0,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hDevice == INVALID_HANDLE_VALUE) {
		LastError = 1;
		return -1;
	}

	// "Unplug" the emulated device
	result = DeviceIoControl(hDevice,
		IOCTL_GHOST_BUS_DISABLE_DRIVE,
		&lID,
		sizeof(LONG),
		NULL,
		0,
		&BytesReturned,
		NULL);

	if (result == FALSE) {
		LastError = 3;
		return -1;
	}

	CloseHandle(hDevice);
	
	// Signal the info thread to terminate
	if (Device->InfoThread != NULL) {
		if (!SetEvent(Device->StopEvent)) {
			LastError = 8;
			return -1;
		}
		
		// Wait until the thread has terminated
		WaitForSingleObject(Device->InfoThread, INFINITE);
	}
	if (Device->StopEvent != NULL) {
		CloseHandle(Device->StopEvent);
	}
	DeviceListRemove(DeviceID);
	return 0;
}

int DLLCALL GhostGetLastError() {
	return LastError;
}

const char * DLLCALL GhostGetErrorDescription(int Error) {
	if (Error < 0 || Error >= (sizeof(ErrorDescriptions) / sizeof(char*))) {
		return NULL;
	}
	
	return ErrorDescriptions[Error];
}

PGHOST_INCIDENT _GetIncident(int DeviceID, int IncidentID) {
	PGHOST_DEVICE Device;
	PGHOST_INCIDENT Incident;
	
	Device = DeviceListGet(DeviceID);
	if (Device == NULL) {
		return NULL;
	}
	
	Incident = Device->Incidents;
	while (Incident != NULL) {
		if (Incident->IncidentID == IncidentID) {
			return Incident;
		}
		
		Incident = Incident->Next;
	}
	
	return NULL;
}

int DLLCALL GhostGetProcessID(int DeviceID, int IncidentID) {
	PGHOST_INCIDENT Incident;
	
	Incident = _GetIncident(DeviceID, IncidentID);
	if (Incident == NULL) {
		return -1;
	}
	
	return (int) Incident->WriterInfo->ProcessId;
}

int DLLCALL GhostGetThreadID(int DeviceID, int IncidentID) {
	PGHOST_INCIDENT Incident;
	
	Incident = _GetIncident(DeviceID, IncidentID);
	if (Incident == NULL) {
		return -1;
	}
	
	return (int) Incident->WriterInfo->ThreadId;
}

int DLLCALL GhostGetNumModules(int DeviceID, int IncidentID) {
	PGHOST_INCIDENT Incident;
	
	Incident = _GetIncident(DeviceID, IncidentID);
	if (Incident == NULL) {
		return -1;
	}
	
	return Incident->WriterInfo->ModuleNamesCount;
}

int DLLCALL GhostGetModuleName(int DeviceID, int IncidentID, int ModuleIndex, wchar_t *Buffer, size_t BufferLength) {
	PGHOST_INCIDENT Incident;
	wchar_t *Name;
	size_t Length;
	/*char Debug[512];
	
	_snprintf(Debug, 512, "Buffer: %p, Length: %d\n", Buffer, BufferLength);
	OutputDebugString(Debug);*/
	
	Incident = _GetIncident(DeviceID, IncidentID);
	if (Incident == NULL) {
		return -1;
	}
	
	Name = (PWCHAR) ((PCHAR) Incident->WriterInfo + Incident->WriterInfo->ModuleNameOffsets[ModuleIndex]);
	Length = wcslen(Name);
	
	if (Buffer == NULL || BufferLength < Length + 1) {
		return -1;
	}
	
	wcsncpy(Buffer, Name, BufferLength);
	return Length + 1;
}
