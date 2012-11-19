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

#ifndef EXTENSIONS_H
#define EXTENSIONS_H

#include <ntddk.h>
#include <storport.h>

#include "portctl.h"

#define GHOST_PORT_TAG 'oPhG'
#define GHOST_MAX_TARGETS 10

typedef struct _GHOST_DRIVE_PDO_CONTEXT {

	BOOLEAN ImageMounted;
	BOOLEAN ImageWritten;
	HANDLE ImageFile;
	LARGE_INTEGER ImageSize;
	ULONG ID;
	USHORT WriterInfoCount;
	//PGHOST_INFO_PROCESS_DATA WriterInfo;   // paged memory
	//WDFQUEUE WriterInfoQueue;

} GHOST_DRIVE_PDO_CONTEXT, *PGHOST_DRIVE_PDO_CONTEXT;


typedef enum {
	GhostDriveEnabled,
	GhostDriveDisabled,
	GhostDriveInitializing
} GhostDriveState;


typedef struct _IO_WORK_ITEM {
	LIST_ENTRY ListNode;
	PSCSI_REQUEST_BLOCK Srb;
	PGHOST_DRIVE_PDO_CONTEXT DriveContext;
} IO_WORK_ITEM, *PIO_WORK_ITEM;


typedef struct _GHOST_PORT_EXTENSION {

	UNICODE_STRING DeviceInterface;
	GhostDriveState DriveStates[GHOST_MAX_TARGETS];
	PREQUEST_PARAMETERS MountParameters[GHOST_MAX_TARGETS];	// non-paged memory
	LIST_ENTRY IoWorkItems;
	KSPIN_LOCK IoWorkItemsLock;
	KEVENT IoWorkAvailable;
	PKTHREAD IoThread;
	KEVENT IoThreadTerminate;
	
} GHOST_PORT_EXTENSION, *PGHOST_PORT_EXTENSION;

#endif
