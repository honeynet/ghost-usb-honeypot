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
 

#ifndef GHOSTLIB_H
#define GHOSTLIB_H


#include <stddef.h>

#define DLLCALL __stdcall


/*
 * This is the interface of GhostLib, a library for communication
 * with the kernel-mode components of Ghost.
 *
 * Client applications should use the library to control the operation
 * of Ghost's core and to obtain results.
 */
 


/*
 * ----------------------------------------------------------------
 * Mounting and unmounting
 * ----------------------------------------------------------------
 */

/*
 * Incident callbacks receive the device ID, an incident ID that they can use to query
 * details and a caller-supplied pointer to give them context information.
 */
typedef void (DLLCALL *GhostIncidentCallback) (int, int, void*);

/*
 * Mount a virtual USB storage device. The callback function is called
 * with the caller-supplied context (may be NULL)
 * whenever data is written to the emulated device by a process that has not been
 * reported before. Its signature should
 * be of the type void MyCallback(int IncidentID, void *Context).
 *
 * On success, the function returns the new device's ID. Otherwise, it returns -1
 * and you can obtain a description of the error using the library's
 * error handling functions.
 */
//int DLLCALL GhostMountDevice(GhostIncidentCallback Callback, void *Context, const char *ImageName);
int DLLCALL GhostMountDeviceWithID(int DeviceID, GhostIncidentCallback Callback, void *Context, const char *ImageName);

/*
 * Unmount the emulated device specified by its device ID
 * as obtained from GhostMountDevice.
 *
 * On success, the function returns 0. Otherwise, it returns -1
 * and you can obtain a description of the error using the library's
 * error handling functions.
 */
int DLLCALL GhostUmountDevice(int DeviceID);


/*
 * ----------------------------------------------------------------
 * Error handling
 * ----------------------------------------------------------------
 */

/*
 * Retrieve the error number of the most recent error. Using the
 * number, you can obtain a description of the error by means of
 * GhostGetErrorDescription.
 */ 
int DLLCALL GhostGetLastError();

/*
 * Retrieve a description of the given error.
 */
const char * DLLCALL GhostGetErrorDescription(int Error);


/*
 * ----------------------------------------------------------------
 * Incident details
 * ----------------------------------------------------------------
 */

/*
 * Get the process ID of the writer or -1 on error.
 */
int DLLCALL GhostGetProcessID(int DeviceID, int IncidentID);

/*
 * Get the thread ID of the writer or -1 on error.
 */
int DLLCALL GhostGetThreadID(int DeviceID, int IncidentID);

/*
 * Get the number of modules (program binaries and DLLs) that were
 * loaded in the writing process at the time of writing. Note that
 * the list of loaded modules is not always available.
 */
int DLLCALL GhostGetNumModules(int DeviceID, int IncidentID);

/*
 * Get the name of a loaded module in the writing process. The
 * index must be between 0 and the number of loaded modules (as
 * obtained by GhostGetNumModules) minus 1.
 *
 * The function will write the name to Buffer, copying at most
 * BufferLength wide characters. If the buffer is too short or NULL,
 * it returns -1. On success, it returns the number of wide characters
 * written to the buffer.
 */
int DLLCALL GhostGetModuleName(int DeviceID, int IncidentID, int ModuleIndex, wchar_t *Buffer, size_t BufferLength);

#endif
