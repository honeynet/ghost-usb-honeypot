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


#include <ntifs.h>

#include "device_control.h"
#include "file_io.h"
#include "ghostdrive.h"
#include "ghostdrive_io.h"
#include "information.h"


/*
 * Mount the given image file. If the file does not yet exist, it will be created. If another image
 * is mounted already, the function returns an error.
 */
NTSTATUS GhostFileIoMountImage(WDFDEVICE Device, PUNICODE_STRING ImageName, PLARGE_INTEGER ImageSize)
{
	PGHOST_DRIVE_CONTEXT Context;
	OBJECT_ATTRIBUTES ImageAttr;
	IO_STATUS_BLOCK ImageStatus;
	FILE_END_OF_FILE_INFORMATION EofInfo;
	FILE_STANDARD_INFORMATION StandardInfo;
	NTSTATUS status;

	// The device context holds information about a possibly mounted image.
	Context = GhostDriveGetContext(Device);

	// If another image has been mounted already, return an error
	if (Context->ImageMounted == TRUE) {
		KdPrint(("Another image has been mounted already\n"));
		return STATUS_UNSUCCESSFUL;
	}

	// Name the file
	InitializeObjectAttributes(&ImageAttr,
							ImageName,
							OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
							NULL,
							NULL);

	// Open the file
	status = ZwCreateFile(&Context->ImageFile,
						GENERIC_READ | GENERIC_WRITE,
						&ImageAttr,
						&ImageStatus,
						ImageSize,
						FILE_ATTRIBUTE_NORMAL,
						0,
						FILE_OPEN_IF,
						FILE_NON_DIRECTORY_FILE | FILE_RANDOM_ACCESS | FILE_NO_INTERMEDIATE_BUFFERING | FILE_SYNCHRONOUS_IO_NONALERT,
						NULL,
						0);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not create or open the image file %wZ", &ImageName));
		return status;
	}

	if (ImageStatus.Information == FILE_CREATED) {
		KdPrint(("File created\n"));

		// Make the file sparse
		status = ZwFsControlFile(Context->ImageFile,
						NULL,
						NULL,
						NULL,
						&ImageStatus,
						FSCTL_SET_SPARSE,
						NULL,
						0,
						NULL,
						0);

		if (!NT_SUCCESS(status)) {
			KdPrint(("Could not make the file sparse\n"));
			ZwClose(Context->ImageFile);
			return status;
		}
		else {
			KdPrint(("Sparse file\n"));
		}

		// Set the file size
		EofInfo.EndOfFile.QuadPart = ImageSize->QuadPart;
		status = ZwSetInformationFile(Context->ImageFile,
						&ImageStatus,
						&EofInfo,
						sizeof(EofInfo),
						FileEndOfFileInformation);

		if (!NT_SUCCESS(status)) {
			KdPrint(("Could not set the new file's size: 0x%lx\n", status));
			ZwClose(Context->ImageFile);
			return status;
		}

		Context->ImageSize.QuadPart = ImageSize->QuadPart;
	}
	else {
		// Retrieve the file size
		status = ZwQueryInformationFile(Context->ImageFile,
						&ImageStatus,
						&StandardInfo,
						sizeof(StandardInfo),
						FileStandardInformation);

		if (!NT_SUCCESS(status)) {
			KdPrint(("Could not read the file size\n"));
			ZwClose(Context->ImageFile);
			return status;
		}

		Context->ImageSize.QuadPart = StandardInfo.EndOfFile.QuadPart;
	}

	// Store information about the image in the device context
	Context->ImageMounted = TRUE;
	Context->ChangeCount++;
	return STATUS_SUCCESS;
}


/*
 * Detach the image file from the device. After that, I/O is not possible anymore.
 */
NTSTATUS GhostFileIoUmountImage(WDFDEVICE Device)
{
	PGHOST_DRIVE_CONTEXT Context;
	NTSTATUS status;

	// The image handle is stored in the device context
	Context = GhostDriveGetContext(Device);
	if (Context->ImageMounted == FALSE) {
		KdPrint(("No image mounted\n"));
		return STATUS_SUCCESS;
	}

	// Close the image
	status = ZwClose(Context->ImageFile);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not close handle\n"));
		return status;
	}

	// Record in the context that no image is mounted
	Context->ImageMounted = FALSE;
	return STATUS_SUCCESS;
}


/*
 * The framework calls this function whenever a read request is received.
 * We use the image file to service the request.
 */
VOID GhostFileIoRead(WDFQUEUE Queue, WDFREQUEST Request, size_t Length)
{
	NTSTATUS status;
	PGHOST_DRIVE_CONTEXT Context;
	WDF_REQUEST_PARAMETERS Params;
	LARGE_INTEGER Offset;
	IO_STATUS_BLOCK StatusBlock;
	WDFMEMORY OutputMemory;
	PVOID Buffer;

	KdPrint(("Read called\n"));

	// Get the device context
	Context = GhostDriveGetContext(WdfIoQueueGetDevice(Queue));

	// Check if an image is mounted at all
	if (Context->ImageMounted == FALSE) {
		KdPrint(("No image mounted\n"));
		WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
		return;
	}

	// Get additional information on the request (especially the offset)
	WDF_REQUEST_PARAMETERS_INIT(&Params);
	WdfRequestGetParameters(Request, &Params);
	Offset.QuadPart = Params.Parameters.Read.DeviceOffset;

	// Check if the data is within the bounds
	if (Offset.QuadPart + Length > Context->ImageSize.QuadPart) {
		KdPrint(("Out of bounds\n"));
		WdfRequestCompleteWithInformation(Request, STATUS_INVALID_PARAMETER, 0);
		return;
	}

	// Retrieve the output memory
	status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	// Allocate the local buffer
	Buffer = ExAllocatePoolWithTag(NonPagedPool, Length, TAG);
	if (Buffer == NULL) {
		KdPrint(("Could not allocate a local buffer\n"));
		WdfRequestComplete(Request, STATUS_INTERNAL_ERROR);
		return;
	}

	// Read the data
	status = ZwReadFile(Context->ImageFile,
						NULL,
						NULL,
						NULL,
						&StatusBlock,
						Buffer,
						(ULONG)Length,
						&Offset,
						NULL);

	if (!NT_SUCCESS(status)) {
		KdPrint(("ZwReadFile failed with status 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	// Copy the data to the output buffer
	status = WdfMemoryCopyFromBuffer(OutputMemory, 0, Buffer, Length);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not copy data to the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	// Free the local buffer
	ExFreePoolWithTag(Buffer, TAG);

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, StatusBlock.Information);
}


/*
 * The Windows Driver Framework calls this function whenever the driver receives a write request.
 * We write the data to the image file.
 */
VOID GhostFileIoWrite(WDFQUEUE Queue, WDFREQUEST Request, size_t Length)
{
	NTSTATUS status;
	PGHOST_DRIVE_CONTEXT Context;
	WDF_REQUEST_PARAMETERS Params;
	LARGE_INTEGER Offset;
	IO_STATUS_BLOCK StatusBlock;
	WDFMEMORY InputMemory;
	PVOID Buffer;
	PGHOST_INFO_PROCESS_DATA WriterInfo;
	HANDLE Pid;

	KdPrint(("Write called\n"));

	// Get the device context
	Context = GhostDriveGetContext(WdfIoQueueGetDevice(Queue));
	
	// Collect information about the caller
	if (Context->WriterInfoCount < MAX_NUM_WRITER_INFO) {
		// Do we know the caller already?
		Pid = PsGetCurrentProcessId();
		WriterInfo = Context->WriterInfo;
		while (WriterInfo != NULL) {
			if (WriterInfo->ProcessId == Pid) {
				break;
			}
			WriterInfo = WriterInfo->Next;
		}
		
		if (WriterInfo == NULL) {
			// Caller unknown yet - collect information
			WriterInfo = GhostInfoCollectProcessData(Request);
			if (WriterInfo != NULL) {
				WriterInfo->Next = Context->WriterInfo;
				Context->WriterInfo = WriterInfo;
				Context->WriterInfoCount++;
				GhostDeviceControlProcessPendingWriterInfoRequests(Context);
			}
		}
		else {
			KdPrint(("Caller known already\n"));
		}
	}
	else {
		KdPrint(("Maximum number of writer info structs reached\n"));
	}

	// Check if an image is mounted at all
	if (Context->ImageMounted == FALSE) {
		KdPrint(("No image mounted\n"));
		WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
		return;
	}

	// Get additional information on the request (especially the offset)
	WDF_REQUEST_PARAMETERS_INIT(&Params);
	WdfRequestGetParameters(Request, &Params);
	Offset.QuadPart = Params.Parameters.Write.DeviceOffset;

	// Check if the data is within the bounds
	if (Offset.QuadPart + Length > Context->ImageSize.QuadPart) {
		KdPrint(("Out of bounds\n"));
		WdfRequestCompleteWithInformation(Request, STATUS_INVALID_PARAMETER, 0);
		return;
	}

	// Retrieve the input memory
	status = WdfRequestRetrieveInputMemory(Request, &InputMemory);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not retrieve the input buffer\n"));
		WdfRequestComplete(Request, status);
		return;
	}

	// Allocate the local buffer
	Buffer = ExAllocatePoolWithTag(NonPagedPool, Length, TAG);
	if (Buffer == NULL) {
		KdPrint(("Could not allocate a local buffer\n"));
		WdfRequestComplete(Request, STATUS_INTERNAL_ERROR);
		return;
	}

	// Copy data to the buffer
	status = WdfMemoryCopyToBuffer(InputMemory, 0, Buffer, Length);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not copy data to the local buffer: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	// Write the data
	status = ZwWriteFile(Context->ImageFile,
						NULL,
						NULL,
						NULL,
						&StatusBlock,
						Buffer,
						Length,
						&Offset,
						NULL);

	if (!NT_SUCCESS(status)) {
		KdPrint(("ZwWriteFile failed\n"));
		WdfRequestComplete(Request, status);
		return;
	}

	// Free the local buffer
	ExFreePoolWithTag(Buffer, TAG);

	// set the flag indicating that data has been written to the device
	Context->ImageWritten = TRUE;

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, StatusBlock.Information);
}