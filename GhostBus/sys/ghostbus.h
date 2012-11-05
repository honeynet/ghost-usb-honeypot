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

#ifndef GHOSTBUS_H
#define GHOSTBUS_H


/*
 * Path definitions
 */
#define BUS_DEVICE_NAME L"\\Device\\GhostBus"
#define BUS_LINK_NAME L"\\DosDevices\\GhostBus"


/*
 * Control code definitions
 *
 * The custom control codes that we define here are used by the user-mode application to
 * make the virtual bus enable or disable virtual devices.
 */
#define FILE_DEVICE_GHOST_BUS 0x8182
#define IOCTL_GHOST_BUS_ENABLE_DRIVE CTL_CODE(FILE_DEVICE_GHOST_BUS, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GHOST_BUS_DISABLE_DRIVE CTL_CODE(FILE_DEVICE_GHOST_BUS, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)


/*
 * We define custom control codes here, so that the user-mode application can tell
 * us to mount or unmount an image file or to provide information about writers.
 */
#define FILE_DEVICE_GHOST_DRIVE 0x8183
#define IOCTL_GHOST_DRIVE_GET_WRITER_INFO CTL_CODE(FILE_DEVICE_GHOST_DRIVE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
// Deprecated codes
#define IOCTL_GHOST_DRIVE_MOUNT_IMAGE CTL_CODE(FILE_DEVICE_GHOST_DRIVE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GHOST_DRIVE_UMOUNT_IMAGE CTL_CODE(FILE_DEVICE_GHOST_DRIVE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define MAX_NUM_WRITER_INFO 32


/*
 * Parameters for control codes
 *
 * This struct is used by the mount control code to transmit information about the
 * image file that is to be loaded.
 */
typedef struct _GHOST_DRIVE_MOUNT_PARAMETERS {

	LARGE_INTEGER RequestedSize;
	WCHAR ImageName[0];

} GHOST_DRIVE_MOUNT_PARAMETERS, *PGHOST_DRIVE_MOUNT_PARAMETERS;


/*
 * This struct contains information about the writer info request.
 */ 
typedef struct _GHOST_DRIVE_WRITER_INFO_PARAMETERS {
	
	ULONG DeviceID;
	BOOLEAN Block;
	USHORT Index;
	
} GHOST_DRIVE_WRITER_INFO_PARAMETERS, *PGHOST_DRIVE_WRITER_INFO_PARAMETERS;


/*
 * This struct is returned by the writer info request, if the buffer is large enough.
 */
typedef struct _GHOST_DRIVE_WRITER_INFO_RESPONSE {
	
	HANDLE ProcessId;
	HANDLE ThreadId;
	USHORT ModuleNamesCount;
	SIZE_T ModuleNameOffsets[1];
	
} GHOST_DRIVE_WRITER_INFO_RESPONSE, *PGHOST_DRIVE_WRITER_INFO_RESPONSE;


#endif
