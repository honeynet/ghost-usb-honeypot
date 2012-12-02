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

#include "devicelist.h"

#include <stdlib.h>


static PGHOST_DEVICE GlobalDeviceList = NULL;
static CRITICAL_SECTION CriticalSection;


static PGHOST_DEVICE _DeviceListFind(int DeviceID, int RemoveFromList);


static void _FreeDevice(PGHOST_DEVICE Device) {
	PGHOST_INCIDENT Incident, NextIncident;
	
	if (Device == NULL) {
		return;
	}
	
	Incident = Device->Incidents;
	while (Incident != NULL) {
		NextIncident = Incident->Next;
		free(Incident->WriterInfo);
		free(Incident);
		Incident = NextIncident;
	}
	
	free(Device);
}


void DeviceListInit() {
	InitializeCriticalSection(&CriticalSection);
}


void DeviceListDestroy() {
	PGHOST_DEVICE Device, NextDevice;
	PGHOST_INCIDENT Incident, NextIncident;
	
	EnterCriticalSection(&CriticalSection);
	
	Device = GlobalDeviceList;
	while (Device != NULL) {
		NextDevice = Device->Next;
		_FreeDevice(Device);
		Device = NextDevice;
	}
	
	LeaveCriticalSection(&CriticalSection);
	DeleteCriticalSection(&CriticalSection);
}


PGHOST_DEVICE DeviceListCreateDevice(int DeviceID) {
	PGHOST_DEVICE Device;
	
	Device = malloc(sizeof(GHOST_DEVICE));
	Device->DeviceID = DeviceID;
	
	return Device;
}


void DeviceListAdd(PGHOST_DEVICE Device) {
	EnterCriticalSection(&CriticalSection);
	Device->Next = GlobalDeviceList;
	GlobalDeviceList = Device;
	LeaveCriticalSection(&CriticalSection);
}


static PGHOST_DEVICE _DeviceListFind(int DeviceID, int RemoveFromList) {
	PGHOST_DEVICE Device, PreviousDevice;
	
	EnterCriticalSection(&CriticalSection);
	
	PreviousDevice = NULL;
	Device = GlobalDeviceList;
	while (Device != NULL) {
		if (Device->DeviceID == DeviceID) {
			if (RemoveFromList) {
				if (PreviousDevice != NULL) {
					PreviousDevice->Next = Device->Next;
				}
				else {
					GlobalDeviceList = Device->Next;
				}
				
				Device->Next = NULL;
			}
			
			break;
		}
		
		PreviousDevice = Device;
		Device = Device->Next;
	}
	
	LeaveCriticalSection(&CriticalSection);
	return Device;
}


PGHOST_DEVICE DeviceListGet(int DeviceID) {
	return _DeviceListFind(DeviceID, 0);
}


void DeviceListRemove(int DeviceID) {
	_FreeDevice(_DeviceListFind(DeviceID, 1));
}