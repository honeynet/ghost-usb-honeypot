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
#include <winioctl.h>

#include <ntddscsi.h>
#include <setupapi.h>
#include <initguid.h>
#include <portctl.h>

#include <version.h>

#include "ghostlib.h"
#include "devicelist.h"


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
	"Could not set an event object", // 8
	"No image name specified", // 9
	"Image name too long" // 10
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


HANDLE OpenBus() {
	HANDLE hDevice;
	HDEVINFO DevInfo;
	SP_DEVICE_INTERFACE_DATA DevInterfaceData;
	DWORD RequiredSize;
	PSP_DEVICE_INTERFACE_DETAIL_DATA DevInterfaceDetailData;
	
	// Get all devices that support the interface
	DevInfo = SetupDiGetClassDevsEx(&GUID_DEVINTERFACE_GHOST, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT, NULL, NULL, NULL);
	if (DevInfo == INVALID_HANDLE_VALUE) {
		return INVALID_HANDLE_VALUE;
	}
	
	// Enumerate the device instances
	DevInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	if (!SetupDiEnumDeviceInterfaces(DevInfo, NULL, &GUID_DEVINTERFACE_GHOST, 0, &DevInterfaceData)) {
		//printf("Error: Unable to find an instance of the interface (0x%x)\n", GetLastError());
		return INVALID_HANDLE_VALUE;
	}
	
	// Obtain details
	SetupDiGetDeviceInterfaceDetail(DevInfo, &DevInterfaceData, NULL, 0, &RequiredSize, NULL);
	DevInterfaceDetailData = malloc(RequiredSize);
	DevInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
	if (!SetupDiGetDeviceInterfaceDetail(DevInfo, &DevInterfaceData, DevInterfaceDetailData, RequiredSize, NULL, NULL)) {
		//printf("Error: Unable to obtain details of the interface instance\n");
		free(DevInterfaceDetailData);
		return INVALID_HANDLE_VALUE;
	}
	
	SetupDiDestroyDeviceInfoList(DevInfo);

	//printf("Opening bus device at %s...\n", DevInterfaceDetailData->DevicePath);

	hDevice = CreateFile(DevInterfaceDetailData->DevicePath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
		
	free(DevInterfaceDetailData);
	return hDevice;
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


PGHOST_INCIDENT FetchIncidentInfo(PGHOST_DEVICE GhostDevice, int IncidentID) {
	SIZE_T RequiredSize;
	DWORD BytesReturned;
	PGHOST_DRIVE_WRITER_INFO_RESPONSE WriterInfo;
	REQUEST_PARAMETERS RequestParams;
	HANDLE hDevice;
	PGHOST_INCIDENT Incident;
	OVERLAPPED Overlapped;
	HANDLE Events[2];
	
	ZeroMemory(&Overlapped, sizeof(OVERLAPPED));
	Overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (Overlapped.hEvent == NULL) {
		return NULL;
	}
	
	Events[0] = Overlapped.hEvent;
	Events[1] = GhostDevice->StopEvent;
	
	// Open the device
	hDevice = OpenBus();

	if (hDevice == INVALID_HANDLE_VALUE) {
		return NULL;
	}

	RequestParams.Opcode = OpcodeGetWriterInfo;
	RequestParams.MagicNumber = GHOST_MAGIC_NUMBER;
	RequestParams.DeviceID = (UCHAR)GhostDevice->DeviceID;
	RequestParams.WriterInfoParameters.BlockingCall = TRUE;
	RequestParams.WriterInfoParameters.WriterIndex = (USHORT)IncidentID;

	DeviceIoControl(hDevice,
		IOCTL_MINIPORT_PROCESS_SERVICE_IRP,
		&RequestParams,
		sizeof(REQUEST_PARAMETERS),
		&RequiredSize,
		sizeof(SIZE_T),
		&BytesReturned,
		&Overlapped);
	
	// Wait until either we've received information or we're told to terminate
	if (WaitForMultipleObjects(2, Events, FALSE, INFINITE) == WAIT_OBJECT_0 + 1) {
		return NULL;
	}

	if (Overlapped.Internal != 0) {
		// Probably no more data available
		//printf("No more data\n");
		return NULL;
	}
	
	ResetEvent(Overlapped.hEvent);

	//printf("%d bytes returned\n", BytesReturned);
	//printf("Need %d bytes\n", RequiredSize);

	WriterInfo = malloc(RequiredSize);
	ZeroMemory(WriterInfo, RequiredSize);
	//printf("Retrieving actual data...\n");

	DeviceIoControl(hDevice,
		IOCTL_MINIPORT_PROCESS_SERVICE_IRP,
		&RequestParams,
		sizeof(REQUEST_PARAMETERS),
		WriterInfo,
		RequiredSize,
		&BytesReturned,
		&Overlapped);
		
	// Wait until either we've received information or we're told to terminate
	if (WaitForMultipleObjects(2, Events, FALSE, INFINITE) == WAIT_OBJECT_0 + 1) {
		return NULL;
	}

	if (Overlapped.Internal != 0) {
		//printf("Error: IOCTL code failed: %d\n", GetLastError());
		free(WriterInfo);
		return NULL;
	}
	
	ResetEvent(Overlapped.hEvent);
	
	// Are we supposed to terminate?
	/*if (WaitForSingleObject(GhostDevice->StopEvent, 0) == WAIT_OBJECT_0) {
		break;
	}*/
	
	Incident = malloc(sizeof(GHOST_INCIDENT));
	Incident->IncidentID = IncidentID;
	Incident->WriterInfo = WriterInfo;
	Incident->Next = GhostDevice->Incidents;
	GhostDevice->Incidents = Incident;
	
	CancelIo(hDevice);
	CloseHandle(hDevice);
	CloseHandle(Overlapped.hEvent);
	
	return Incident;
}


DWORD WINAPI InfoThread(LPVOID Parameter) {
	PGHOST_DEVICE GhostDevice;
	//char dosdevice[] = "\\\\.\\GhostBus";
	USHORT i = 0;
	PGHOST_INCIDENT Incident;
	
	if (Parameter == NULL) {
		return -1;
	}
	
	GhostDevice = (PGHOST_DEVICE) Parameter;
	
	// Wait for writer information
	while (1) {
		Incident = FetchIncidentInfo(GhostDevice, i);
		if (Incident == NULL) {
			// TODO: Error code?
			return -1;
		}
		
		// Invoke the callback function
		//
		// This thread is only ever created if the callback function is non-NULL, so
		// we don't need to check again.
		GhostDevice->Callback(GhostDevice->DeviceID, Incident->IncidentID, GhostDevice->Context);
		
		i++;
	}
	
	return 0;
}


int GhostWaitForIncident(int DeviceID, int NextIncidentID) {
	PGHOST_DEVICE GhostDevice;
	PGHOST_INCIDENT Incident;
	
	GhostDevice = DeviceListGet(DeviceID);
	if (GhostDevice == NULL) {
		LastError = 5;
		return -1;
	}
	
	Incident = FetchIncidentInfo(GhostDevice, NextIncidentID);
	if (Incident == NULL) {
		// TODO: Error code?
		return -1;
	}
	
	return Incident->IncidentID;
}


/*int DLLCALL GhostMountDevice(GhostIncidentCallback Callback, void *Context, const char *ImageName) {
	return GhostMountDeviceWithID(-1, Callback, Context, ImageName);
}*/


int DLLCALL GhostMountDeviceWithID(int DeviceID, GhostIncidentCallback Callback, void *Context, const char *ImageName) {
	HANDLE hDevice;
	DWORD BytesReturned;
	BOOL result;
	LONG OutDeviceID;
	PGHOST_DEVICE GhostDevice;
	PREQUEST_PARAMETERS Parameters;
	WCHAR ActualImageName[MAX_PATH];
	char FullImageName[MAX_PATH] = "\\DosDevices\\";
	
	// Check the image name
	if (ImageName == NULL) {
		LastError = 9;
		return -1;
	}
	
	// Check the name length
	if (mbstowcs(NULL, ImageName, MAX_PATH) >= MAX_PATH - strlen(FullImageName)) {
		LastError = 10;
		return -1;
	}
	
	// Check the device ID
	if (DeviceID < -1 || DeviceID > 9) {
		LastError = 5;
		return -1;
	}
	
	// Open the bus device
	hDevice = OpenBus();

	if (hDevice == INVALID_HANDLE_VALUE) {
		LastError = 1;
		return -1;
	}

	// "Plug in" the virtual USB device
	strncat(FullImageName, ImageName, MAX_PATH - strlen(FullImageName));
	mbstowcs(ActualImageName, FullImageName, MAX_PATH);
	Parameters = malloc(sizeof(REQUEST_PARAMETERS) + wcslen(ActualImageName) * sizeof(WCHAR));
	Parameters->MagicNumber = GHOST_MAGIC_NUMBER;
	Parameters->Opcode = OpcodeEnable;
	Parameters->DeviceID = (UCHAR)DeviceID;
	wcsncpy(Parameters->MountInfo.ImageName, ActualImageName, wcslen(ActualImageName));
	Parameters->MountInfo.ImageNameLength = (USHORT)wcslen(ActualImageName);
	Parameters->MountInfo.ImageSize.QuadPart = 100 * 1024 * 1024;

	result = DeviceIoControl(hDevice,
		IOCTL_MINIPORT_PROCESS_SERVICE_IRP,
		Parameters,
		sizeof(REQUEST_PARAMETERS) + wcslen(ActualImageName) * sizeof(WCHAR),
		NULL,
		0,
		&BytesReturned,
		NULL);

	if (result != TRUE) {
		LastError = 2;
		return -1;
	}

	CloseHandle(hDevice);
	
	GhostDevice = DeviceListCreateDevice(DeviceID);
	GhostDevice->Callback = Callback;
	GhostDevice->Context = Context;
	GhostDevice->Incidents = NULL;
	GhostDevice->InfoThread = NULL;
	
	GhostDevice->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (GhostDevice->StopEvent == NULL) {
		LastError = 6;
		return -1;
	}
	
	DeviceListAdd(GhostDevice);
	
	// Start a thread that waits for incidents
	if (Callback != NULL) {
		GhostDevice->InfoThread = CreateThread(NULL, 0, InfoThread, GhostDevice, 0, NULL);
		if (GhostDevice->InfoThread == NULL) {
			LastError = 4;
			return -1;
		}
	}

	return DeviceID;
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
	REQUEST_PARAMETERS Parameters;
	
	// Find context information about the device
	Device = DeviceListGet(DeviceID);
	if (Device == NULL) {
		LastError = 7;
		return -1;
	}

	// Open the bus device
	hDevice = OpenBus();

	if (hDevice == INVALID_HANDLE_VALUE) {
		LastError = 1;
		return -1;
	}

	// "Unplug" the emulated device
	Parameters.MagicNumber = GHOST_MAGIC_NUMBER;
	Parameters.Opcode = OpcodeDisable;
	Parameters.DeviceID = (UCHAR)DeviceID;

	result = DeviceIoControl(hDevice,
		IOCTL_MINIPORT_PROCESS_SERVICE_IRP,
		&Parameters,
		sizeof(REQUEST_PARAMETERS),
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
	if (!SetEvent(Device->StopEvent)) {
		LastError = 8;
		return -1;
	}
		
	if (Device->InfoThread != NULL) {
		// Wait until the thread has terminated
		WaitForSingleObject(Device->InfoThread, INFINITE);
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

int DLLCALL GhostGetProcessImageBase(int DeviceID, int IncidentID) {
	PGHOST_INCIDENT Incident;

	Incident = _GetIncident(DeviceID, IncidentID);
	if (NULL == Incident) {
		return -1;
	}

	return Incident->WriterInfo->ProcessImageBase;
}

