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

#include "code_origin.h"

/*
 * This routine is called whenever a process loads an executable image.
 * We use it to check whether the image originates from removable storage.
 * If so, we terminate the process.
 */
VOID ImageLoadNotification(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo) {
	NTSTATUS status;
	PIMAGE_INFO_EX ExtendedInfo;
	PDEVICE_OBJECT DiskObject;
	
	if (FullImageName == NULL) {
		// We can't get the image name - exit for now
		KdPrint((__FUNCTION__": Name of new image not given\n"));
		goto out;
	}
	
	KdPrint((__FUNCTION__": Image loaded: %wZ\n", FullImageName));
	
	if (!ImageInfo->ExtendedInfoPresent) {
		KdPrint((__FUNCTION__": No extended info available\n"));
		goto out;
	}
	
	ExtendedInfo = CONTAINING_RECORD(ImageInfo, IMAGE_INFO_EX, ImageInfo);
	DiskObject = ExtendedInfo->FileObject->DeviceObject;
	
	if (!(DiskObject->Characteristics & FILE_REMOVABLE_MEDIA)) {
		// Device is not removable - this is not interesting for us
		goto out;
	}
	
	KdPrint((__FUNCTION__": Attempt to load image from removable device!\n"));
	
	if (ProcessId == 0) {
		KdPrint((__FUNCTION__": Process ID is 0 - driver?\n"));
		goto out;
	}
	
	status = ZwTerminateProcess(ProcessId, STATUS_UNSUCCESSFUL);
	if (!NT_SUCCESS(status)) {
		KdPrint((__FUNCTION__": Failed to terminate the offending process: 0x%lx\n", status));
		goto out;
	}
	
out:
	return;
}

/*
 * Initialize the system that checks whether newly loaded code resides
 * on a removable device.
 */
NTSTATUS GhostCodeOriginInit() {
	NTSTATUS status;
	
	status = PsSetLoadImageNotifyRoutine(ImageLoadNotification);
	if (!NT_SUCCESS(status)) {
		KdPrint((__FUNCTION__": Failed to register the load notification\n"));
		goto out;
	}
		
out:
	return status;
}

/*
 * Call this routine to clean up before the driver is unloaded.
 */
NTSTATUS GhostCodeOriginFree() {
	NTSTATUS status;
	
	status =PsRemoveLoadImageNotifyRoutine(ImageLoadNotification);
	if (!NT_SUCCESS(status)) {
		KdPrint((__FUNCTION__": Failed to unregister the load notification\n"));
		goto out;
	}
		
out:
	return status;
}