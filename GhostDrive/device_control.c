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


#include "device_control.h"
#include "ghostdrive.h"
#include "ghostdrive_io.h"
#include "file_io.h"

#include <ntdddisk.h>
#include <mountmgr.h>
#include <mountdev.h>


/*
 * Internal functions
 */
void GhostDeviceControlGetHotplugInfo(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context);
void GhostDeviceControlGetDeviceNumber(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context);
void GhostDeviceControlQueryProperty(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context);
void GhostDeviceControlGetPartitionInfo(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context);
void GhostDeviceControlCheckVerify(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context, size_t OutputBufferLength);
void GhostDeviceControlGetDriveGeometry(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context);
void GhostDeviceControlGetLengthInfo(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context);
void GhostDeviceControlMountImage(WDFREQUEST Request, WDFDEVICE Device);
void GhostDeviceControlUmountImage(WDFREQUEST Request, WDFDEVICE Device, PGHOST_DRIVE_CONTEXT Context);
void GhostDeviceControlQueryDeviceName(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context);
void GhostDeviceControlQueryUniqueId(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context);
void GhostDeviceControlHandleWriterInfoRequest(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context);
void GhostDeviceControlGetWriterInfo(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context);


/*
 * The framework calls this routine whenever Device Control IRPs have to be handled.
 * TODO: Document control codes.
 */
VOID GhostDeviceControlDispatch(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferLength,
			size_t InputBufferLength, ULONG IoControlCode)
{
	WDFDEVICE Device = WdfIoQueueGetDevice(Queue);
	PGHOST_DRIVE_CONTEXT Context = GhostDriveGetContext(Device);

	switch (IoControlCode) {
		case IOCTL_STORAGE_GET_HOTPLUG_INFO:
			GhostDeviceControlGetHotplugInfo(Request, Context);
			break;

		case IOCTL_DISK_GET_PARTITION_INFO:
			GhostDeviceControlGetPartitionInfo(Request, Context);
			break;

		case IOCTL_DISK_GET_PARTITION_INFO_EX:
			KdPrint(("Invalid - GetPartitionInfoEx\n"));
			WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
			break;

		case IOCTL_DISK_SET_PARTITION_INFO:
			KdPrint(("Invalid - SetPartitionInfo\n"));
			WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
			break;

		case IOCTL_DISK_IS_WRITABLE:
			KdPrint(("IsWritable\n"));
			WdfRequestComplete(Request, STATUS_SUCCESS);
			break;

		case IOCTL_DISK_VERIFY:
			KdPrint(("Verify\n"));
			WdfRequestComplete(Request, STATUS_SUCCESS);
			break;

		case IOCTL_STORAGE_CHECK_VERIFY:
		case IOCTL_STORAGE_CHECK_VERIFY2:
		case IOCTL_DISK_CHECK_VERIFY:
			GhostDeviceControlCheckVerify(Request, Context, OutputBufferLength);
			break;

		case IOCTL_DISK_GET_DRIVE_GEOMETRY:
			GhostDeviceControlGetDriveGeometry(Request, Context);
			break;

		case IOCTL_DISK_GET_DRIVE_GEOMETRY_EX:
			KdPrint(("Invalid - GetDriveGeometryEx\n"));
			WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
			break;

		case IOCTL_DISK_GET_DRIVE_LAYOUT:
		case IOCTL_DISK_GET_DRIVE_LAYOUT_EX:
			KdPrint(("Invalid - GetDriveLayout\n"));
			WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
			break;

		case IOCTL_DISK_GET_LENGTH_INFO:
			GhostDeviceControlGetLengthInfo(Request, Context);
			break;

		case IOCTL_GHOST_DRIVE_MOUNT_IMAGE:
			GhostDeviceControlMountImage(Request, Device);
			break;

		case IOCTL_GHOST_DRIVE_UMOUNT_IMAGE:
			GhostDeviceControlUmountImage(Request, Device, Context);
			break;

		case IOCTL_MOUNTDEV_QUERY_DEVICE_NAME:
			GhostDeviceControlQueryDeviceName(Request, Context);
			break;

		case IOCTL_MOUNTDEV_QUERY_UNIQUE_ID:
			GhostDeviceControlQueryUniqueId(Request, Context);
			break;
		
		case IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME:
			KdPrint(("Invalid - MountManager\n"));
			WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
			break;

		case IOCTL_STORAGE_QUERY_PROPERTY:
			GhostDeviceControlQueryProperty(Request, Context);
			break;

		case IOCTL_STORAGE_GET_DEVICE_NUMBER:
			GhostDeviceControlGetDeviceNumber(Request, Context);
			break;

		case IOCTL_STORAGE_GET_MEDIA_SERIAL_NUMBER:
			KdPrint(("Invalid - GetMediaSerialNumber\n"));
			WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
			break;

		case IOCTL_STORAGE_LOAD_MEDIA:
			KdPrint(("Invalid - LoadMedia\n"));
			WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
			break;
			
		case IOCTL_GHOST_DRIVE_GET_WRITER_INFO:
			GhostDeviceControlHandleWriterInfoRequest(Request, Context);
			break;

		default:
			KdPrint(("Invalid - Unknown code\n"));
			WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
			break;
	}
}


void GhostDeviceControlGetHotplugInfo(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context) {
	STORAGE_HOTPLUG_INFO HotplugInfo;
	WDFMEMORY OutputMemory;
	NTSTATUS status;

	KdPrint(("GetHotplugInfo\n"));

	// Retrieve the output memory
	status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	// Copy initial values to the local data structure
	status = WdfMemoryCopyToBuffer(OutputMemory, 0, &HotplugInfo, sizeof(STORAGE_HOTPLUG_INFO));
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not initialize data structure from memory: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	HotplugInfo.MediaRemovable = TRUE;
	HotplugInfo.MediaHotplug = TRUE;
	HotplugInfo.DeviceHotplug = TRUE;
	//HotplugInfo.WriteCacheEnableOverride = TRUE;

	// Copy the data to the output buffer
	status = WdfMemoryCopyFromBuffer(OutputMemory, 0, &HotplugInfo, sizeof(STORAGE_HOTPLUG_INFO));
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not copy data to the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(STORAGE_HOTPLUG_INFO));
}


void GhostDeviceControlGetPartitionInfo(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context) {
	PARTITION_INFORMATION PartitionInfo;
	WDFMEMORY OutputMemory;
	NTSTATUS status;

	KdPrint(("PartitionInfo\n"));

	PartitionInfo.StartingOffset.QuadPart = 0;
	PartitionInfo.PartitionLength.QuadPart = Context->ImageSize.QuadPart;
	PartitionInfo.HiddenSectors = 1;
	PartitionInfo.PartitionNumber = 0;
	PartitionInfo.PartitionType = PARTITION_ENTRY_UNUSED;
	PartitionInfo.BootIndicator = FALSE;
	PartitionInfo.RecognizedPartition = FALSE;
	PartitionInfo.RewritePartition = FALSE;

	// Retrieve the output memory
	status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	// Copy the data to the output buffer
	status = WdfMemoryCopyFromBuffer(OutputMemory, 0, &PartitionInfo, sizeof(PARTITION_INFORMATION));
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not copy data to the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(PARTITION_INFORMATION));
}


void GhostDeviceControlCheckVerify(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context, size_t OutputBufferLength) {
	WDFMEMORY OutputMemory;
	NTSTATUS status;
	ULONG_PTR info;

	KdPrint(("CheckVerify\n"));
	info = 0;

	// Optionally, we get a buffer for the change counter
	if (OutputBufferLength > 0) {
		// Retrieve the output memory
		status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
			WdfRequestComplete(Request, status);
			return;
		}

		// Copy the data to the output buffer
		status = WdfMemoryCopyFromBuffer(OutputMemory, 0, &Context->ChangeCount, sizeof(ULONG));
		if (!NT_SUCCESS(status)) {
			KdPrint(("Could not copy data to the output memory: 0x%lx\n", status));
			WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
			return;
		}

		info = sizeof(ULONG);
	}
	
	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, info);
}


void GhostDeviceControlGetDriveGeometry(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context) {
	DISK_GEOMETRY Geometry;
	WDFMEMORY OutputMemory;
	NTSTATUS status;

	KdPrint(("GetDriveGeometry\n"));

	// 32 KB per cylinder
	Geometry.BytesPerSector = 512;
	Geometry.SectorsPerTrack = 32;
	Geometry.TracksPerCylinder = 2;

	Geometry.Cylinders.QuadPart = Context->ImageSize.QuadPart / 2 / 32 / 512;

	//Geometry.MediaType = RemovableMedia;
	Geometry.MediaType = FixedMedia;

	// Retrieve the output memory
	status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	// Copy the data to the output buffer
	status = WdfMemoryCopyFromBuffer(OutputMemory, 0, &Geometry, sizeof(DISK_GEOMETRY));
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not copy data to the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
		return;
	}

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(DISK_GEOMETRY));
}


void GhostDeviceControlGetLengthInfo(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context) {
	GET_LENGTH_INFORMATION LengthInfo;
	WDFMEMORY OutputMemory;
	NTSTATUS status;

	KdPrint(("GetLengthInfo\n"));

	LengthInfo.Length.QuadPart = Context->ImageSize.QuadPart;

	// Retrieve the output memory
	status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	// Copy the data to the output buffer
	status = WdfMemoryCopyFromBuffer(OutputMemory, 0, &LengthInfo, sizeof(GET_LENGTH_INFORMATION));
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not copy data to the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
		return;
	}

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(GET_LENGTH_INFORMATION));
}


void GhostDeviceControlQueryProperty(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context) {
	STORAGE_PROPERTY_QUERY Query;
	STORAGE_DEVICE_DESCRIPTOR DeviceDescriptor;
	WDFMEMORY InputMemory;
	WDFMEMORY OutputMemory;
	NTSTATUS status;

	KdPrint(("QueryProperty\n"));

	// Retrieve the input memory
	status = WdfRequestRetrieveInputMemory(Request, &InputMemory);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	// Copy the input values to the local data structure
	status = WdfMemoryCopyToBuffer(InputMemory, 0, &Query, sizeof(STORAGE_PROPERTY_QUERY));
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not read input data structure from memory: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	// Check if the caller is only querying our support of a certain property descriptor
	if (Query.QueryType == PropertyExistsQuery) {
		// We only support StorageDeviceProperty
		if (Query.PropertyId == StorageDeviceProperty) {
			status = STATUS_SUCCESS;
		}
		else {
			status = STATUS_NOT_SUPPORTED;
		}
	}

	if (Query.QueryType != PropertyStandardQuery) {
		KdPrint(("QueryType invalid\n"));
		WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
		return;
	}

	if (Query.PropertyId != StorageDeviceProperty) {
		KdPrint(("PropertyId not supported\n"));
		WdfRequestComplete(Request, STATUS_NOT_SUPPORTED);
		return;
	}

	// Retrieve the output memory
	status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	// Copy initial values to the local data structure
	status = WdfMemoryCopyToBuffer(OutputMemory, 0, &DeviceDescriptor, sizeof(STORAGE_DEVICE_DESCRIPTOR));
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not initialize output data structure from memory: 0x%lx\n", status));
		// the system just tries to find out whether we support the query, but does not want real data
		WdfRequestComplete(Request, STATUS_SUCCESS);
		return;
	}

	// Set values describing the device
	DeviceDescriptor.DeviceType = 0;
	DeviceDescriptor.DeviceTypeModifier = 0;
	DeviceDescriptor.RemovableMedia = TRUE;
	DeviceDescriptor.CommandQueueing = FALSE;
	DeviceDescriptor.VendorIdOffset = 0;
	DeviceDescriptor.ProductIdOffset = 0;
	DeviceDescriptor.ProductRevisionOffset = 0;
	DeviceDescriptor.SerialNumberOffset = -1;
	DeviceDescriptor.BusType = BusTypeUsb;		// This is the important one
	DeviceDescriptor.RawPropertiesLength = 0;

	// Copy the data to the output buffer
	status = WdfMemoryCopyFromBuffer(OutputMemory, 0, &DeviceDescriptor, sizeof(STORAGE_DEVICE_DESCRIPTOR));
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not copy the STORAGE_DEVICE_DESCRIPTOR structure to the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
		return;
	}

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(STORAGE_DEVICE_DESCRIPTOR));
}


void GhostDeviceControlGetDeviceNumber(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context) {
	STORAGE_DEVICE_NUMBER DeviceNumber;
	WDFMEMORY OutputMemory;
	NTSTATUS status;

	KdPrint(("GetDeviceNumber\n"));

	DeviceNumber.DeviceType = FILE_DEVICE_DISK;
	DeviceNumber.DeviceNumber = Context->ID + 10;
	DeviceNumber.PartitionNumber = 0;

	// Retrieve the output memory
	status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	// Copy the data to the output buffer
	status = WdfMemoryCopyFromBuffer(OutputMemory, 0, &DeviceNumber, sizeof(STORAGE_DEVICE_NUMBER));
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not copy the STORAGE_DEVICE_NUMBER structure to the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
		return;
	}

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(STORAGE_DEVICE_NUMBER));
}


void GhostDeviceControlMountImage(WDFREQUEST Request, WDFDEVICE Device) {
	PGHOST_DRIVE_MOUNT_PARAMETERS Parameters;
	UNICODE_STRING ImageName;
	NTSTATUS status;

	KdPrint(("Mount\n"));

	// Get information on the file to be mounted
	status = WdfRequestRetrieveInputBuffer(Request, sizeof(GHOST_DRIVE_MOUNT_PARAMETERS), &Parameters, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Not enough information\n"));
		WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
		return;
	}

	//KdPrint(("%ws\n", Parameters->ImageName));

	RtlInitUnicodeString(&ImageName, Parameters->ImageName);
	status = GhostFileIoMountImage(Device, &ImageName, &Parameters->RequestedSize);
	WdfRequestComplete(Request, status);
}


void GhostDeviceControlUmountImage(WDFREQUEST Request, WDFDEVICE Device, PGHOST_DRIVE_CONTEXT Context) {
	PBOOLEAN Written = NULL;
	ULONG_PTR info = 0;
	NTSTATUS status;

	KdPrint(("Umount\n"));

	status = WdfRequestRetrieveOutputBuffer(Request, sizeof(BOOLEAN), &Written, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Buffer too small for write detection results\n"));
		// continue anyway
	}

    // We don't unmount here anymore, since this is now done in GhostDriveQueryRemove
	//status = GhostFileIoUmountImage(Device);

	if (Written != NULL) {
		*Written = Context->ImageWritten;
		info = sizeof(BOOLEAN);
	}
	
	WdfRequestCompleteWithInformation(Request, status, info);
}


void GhostDeviceControlQueryDeviceName(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context) {
	USHORT NameLength;
	WDFMEMORY OutputMemory;
	NTSTATUS status;

	KdPrint(("QueryDeviceName\n"));

	// Retrieve the output memory
	status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	NameLength = Context->DeviceNameLength - sizeof(WCHAR);

	// Copy the length to the output buffer
	status = WdfMemoryCopyFromBuffer(OutputMemory, FIELD_OFFSET(MOUNTDEV_NAME, NameLength), &NameLength, sizeof(USHORT));
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not copy the name's length to the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
		return;
	}

	// Copy the data to the output buffer
	status = WdfMemoryCopyFromBuffer(OutputMemory, FIELD_OFFSET(MOUNTDEV_NAME, Name), Context->DeviceName, Context->DeviceNameLength);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not copy the name to the output memory: 0x%lx\n", status));
		WdfRequestCompleteWithInformation(Request, STATUS_BUFFER_OVERFLOW, sizeof(MOUNTDEV_NAME));
		return;
	}

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Context->DeviceNameLength);
}


void GhostDeviceControlQueryUniqueId(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context) {
	WDFMEMORY OutputMemory;
	USHORT Length;
	UCHAR UID;
	NTSTATUS status;

	KdPrint(("QueryUniqueID\n"));

	/*if (BufferLength < sizeof(MOUNTDEV_UNIQUE_ID) + Context->DeviceNameLength - sizeof(UCHAR)) {
		KdPrint(("Buffer too small: %u\n", BufferLength));
		info = sizeof(MOUNTDEV_UNIQUE_ID) + Context->DeviceNameLength - sizeof(UCHAR);
		status = STATUS_BUFFER_TOO_SMALL;
		break;
	}

	UID->UniqueIdLength = Context->DeviceNameLength;
	RtlCopyMemory(UID->UniqueId, Context->DeviceName, Context->DeviceNameLength);*/

	// Retrieve the output memory
	status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	Length = 1;
	UID = (UCHAR)Context->ID + 0x30;

	// Copy the data to the output buffer
	status = WdfMemoryCopyFromBuffer(OutputMemory, FIELD_OFFSET(MOUNTDEV_UNIQUE_ID, UniqueIdLength), &Length, sizeof(USHORT));
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not copy the ID's length to the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
		return;
	}

	status = WdfMemoryCopyFromBuffer(OutputMemory, FIELD_OFFSET(MOUNTDEV_UNIQUE_ID, UniqueId), &UID, sizeof(UCHAR));
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not copy the ID's length to the output memory: 0x%lx\n", status));
		WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
		return;
	}

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length + sizeof(USHORT));
}


void GhostDeviceControlHandleWriterInfoRequest(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context) {
	NTSTATUS status;
	PGHOST_DRIVE_WRITER_INFO_PARAMETERS WriterInfoParams;
	USHORT Index;
	
	KdPrint(("HandleWriterInfoRequest\n"));
	
	status = WdfRequestRetrieveInputBuffer(Request, sizeof(GHOST_DRIVE_WRITER_INFO_PARAMETERS), &WriterInfoParams, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not retrieve input parameters: 0x%lx\n", status));
		WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
		return;
	}
	
	KdPrint(("Block: %d\n", WriterInfoParams->Block));
	KdPrint(("Index: %d\n", WriterInfoParams->Index));
	
	Index = WriterInfoParams->Index;
	if (Index >= Context->WriterInfoCount) {
		KdPrint(("Index out of bounds\n"));
		
		// If this is a blocking request, forward it to the writer info queue
		if (WriterInfoParams->Block) {
			status = WdfRequestForwardToIoQueue(Request, Context->WriterInfoQueue);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Forwarding to writer info queue failed: 0x%lx\n"));
				WdfRequestComplete(Request, STATUS_INTERNAL_ERROR);
				return;
			}
			
			KdPrint(("Forwarded to writer info queue\n"));
		}
		else {
			WdfRequestComplete(Request, STATUS_BUFFER_OVERFLOW);
		}
		
		return;
	}
	
	GhostDeviceControlGetWriterInfo(Request, Context);
}


void GhostDeviceControlProcessPendingWriterInfoRequests(PGHOST_DRIVE_CONTEXT Context) {
	WDFREQUEST PreviousRequest, Request, OwnedRequest;
	WDF_REQUEST_PARAMETERS RequestParams;
	NTSTATUS status;
	PGHOST_DRIVE_WRITER_INFO_PARAMETERS WriterInfoParams;
	
	KdPrint(("Processing pending writer info requests\n"));
	
	PreviousRequest = NULL;
	Request = NULL;
	
	do {
		WDF_REQUEST_PARAMETERS_INIT(&RequestParams);
		status = WdfIoQueueFindRequest(Context->WriterInfoQueue, PreviousRequest, NULL, &RequestParams, &Request);
		if (PreviousRequest != NULL) {
			WdfObjectDereference(PreviousRequest);
		}
		
		if (status == STATUS_NO_MORE_ENTRIES) {
			// End of the queue
			return;
		}
		else if (!NT_SUCCESS(status)) {
			KdPrint(("Could not search writer info queue: 0x%lx\n", status));
			return;
		}
		
		// Look at the request's input parameters
		status = WdfRequestRetrieveInputBuffer(Request, sizeof(GHOST_DRIVE_WRITER_INFO_PARAMETERS), &WriterInfoParams, NULL);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Could not retrieve input parameters: 0x%lx\n", status));
			continue;
		}
		
		// Can we process the request now?
		if (WriterInfoParams->Index < Context->WriterInfoCount) {
			status = WdfIoQueueRetrieveFoundRequest(Context->WriterInfoQueue, Request, &OwnedRequest);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not obtain ownership of the found request: 0x%lx\n", status));
				continue;
			}
			
			GhostDeviceControlGetWriterInfo(OwnedRequest, Context);
		}
		
		PreviousRequest = Request;
	} while (TRUE);
}

	
void GhostDeviceControlGetWriterInfo(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context) {
	NTSTATUS status;
	PGHOST_DRIVE_WRITER_INFO_PARAMETERS WriterInfoParams;
	USHORT Index, i;
	PGHOST_INFO_PROCESS_DATA ProcessInfo;
	PVOID Buffer;
	SIZE_T BufferSize, RequiredSize;
	
	KdPrint(("Processing writer info request\n"));
	
	status = WdfRequestRetrieveInputBuffer(Request, sizeof(GHOST_DRIVE_WRITER_INFO_PARAMETERS), &WriterInfoParams, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not retrieve input parameters: 0x%lx\n", status));
		WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
		return;
	}

	Index = WriterInfoParams->Index;
	if (Index >= Context->WriterInfoCount) {
		// This must not happen
		KdPrint(("Index out of bounds, but should not be\n"));
		WdfRequestComplete(Request, STATUS_INTERNAL_ERROR);
		return;
	}
	
	ProcessInfo = Context->WriterInfo;
	for (i = 0; i < (Context->WriterInfoCount - Index - 1); i++) {
		if (ProcessInfo == NULL) {
			// This does not happen as long as WriterInfoCount is correct
			// Still check as a safeguard
			WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
			return;
		}
		
		ProcessInfo = ProcessInfo->Next;
	}
	
	// Retrieve the output buffer
	status = WdfRequestRetrieveOutputBuffer(Request, sizeof(SIZE_T), &Buffer, &BufferSize);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not obtain output buffer: 0x%lx\n", status));
		WdfRequestComplete(Request, status);
		return;
	}
	
	KdPrint(("Output buffer size is %d\n", BufferSize));
	RequiredSize = GhostInfoGetProcessDataBufferSize(ProcessInfo);
	// If the output buffer is too small, just return its required size
	if (BufferSize < RequiredSize) {
		KdPrint(("Output buffer is too small - need %d bytes\n", RequiredSize));
		*((SIZE_T*) Buffer) = RequiredSize;
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(SIZE_T));
		return;
	}
	
	// Return the requested information
	if (GhostInfoStoreProcessDataInBuffer(ProcessInfo, Buffer, BufferSize) == FALSE) {
		WdfRequestComplete(Request, STATUS_INTERNAL_ERROR);
		return;
	}
	
	KdPrint(("WriterInfo copied\n"));
	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, RequiredSize);
}
