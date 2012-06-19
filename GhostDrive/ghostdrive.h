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

#ifndef GHOSTDRIVE_H
#define GHOSTDRIVE_H

#include <ntddk.h>
#include <wdf.h>

#include "information.h"


#define DRIVE_DEVICE_NAME L"\\Device\\GhostDrive0"
#define DRIVE_LINK_NAME L"\\DosDevices\\GhostDrive0"
// TODO: Refine permissions
#define DEVICE_SDDL_STRING L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;BU)"
#define IMAGE_NAME L"\\DosDevices\\C:\\gd0.img"

#define TAG 'rDhG'


/*
 * The context struct is attached to each instance of the virtual
 * device. It contains information about the device itself and the
 * image file that may be mounted.
 */
typedef struct _GHOST_DRIVE_CONTEXT {

	BOOLEAN ImageMounted;
	BOOLEAN ImageWritten;
	HANDLE ImageFile;
	LARGE_INTEGER ImageSize;
	ULONG ID;
	WCHAR DeviceName[256];
	USHORT DeviceNameLength;
	ULONG ChangeCount;
	USHORT WriterInfoCount;
	PGHOST_INFO_PROCESS_DATA WriterInfo;   // paged memory

} GHOST_DRIVE_CONTEXT, *PGHOST_DRIVE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(GHOST_DRIVE_CONTEXT, GhostDriveGetContext);


#endif
