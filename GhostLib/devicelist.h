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

#ifndef DEVICELIST_H
#define DEVICELIST_H


#include <windows.h>
#include <ghostdrive_io.h>

#include "ghostlib.h"


typedef struct _GHOST_INCIDENT {
	int IncidentID;
	PGHOST_DRIVE_WRITER_INFO_RESPONSE WriterInfo;
	struct _GHOST_INCIDENT *Next;
} GHOST_INCIDENT, *PGHOST_INCIDENT;


typedef struct _GHOST_DEVICE {
	int DeviceID;
	GhostIncidentCallback Callback;
	void *Context;
	HANDLE StopEvent;
	HANDLE InfoThread;
	PGHOST_INCIDENT Incidents;
	struct _GHOST_DEVICE *Next;
} GHOST_DEVICE, *PGHOST_DEVICE;


void DeviceListInit();
void DeviceListDestroy();
PGHOST_DEVICE DeviceListCreateDevice(int DeviceID);
void DeviceListAdd(PGHOST_DEVICE Device);
PGHOST_DEVICE DeviceListGet(int DeviceID);
void DeviceListRemove(int DeviceID);


#endif
