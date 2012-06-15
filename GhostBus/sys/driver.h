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

#ifndef DRIVER_H
#define DRIVER_H

#define INITGUID

#include <ntddk.h>
#include <wdf.h>


/*
 * Bus GUID
 *
 * This is the GUID used by the virtual bus as bus type.
 */

// {B546EADE-E2AA-4F97-9A70-9091DCE34F85}
DEFINE_GUID(GUID_BUS_TYPE_GHOST, 0xb546eade, 0xe2aa, 0x4f97, 0x9a, 0x70, 0x90, 0x91, 0xdc, 0xe3, 0x4f, 0x85);


/*
 * Child identification description structure
 *
 * This struct is used during device enumeration to describe instances
 * of the virtual device. The only feature of each instance is its ID.
 */
typedef struct _GHOST_DRIVE_IDENTIFICATION {
	WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER Header;
	LONG Id;
} GHOST_DRIVE_IDENTIFICATION, *PGHOST_DRIVE_IDENTIFICATION;


/*
 * Ghost drive device ID and other information
 */
#define GHOST_DRIVE_DEVICE_ID L"ghostbus\\ghostdrive"
#define GHOST_DRIVE_DESCRIPTION L"GhostDrive"
#define GHOST_DRIVE_LOCATION L"GhostBus virtual bus"



#endif
