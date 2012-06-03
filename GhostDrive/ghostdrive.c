/*
 * Ghost - A honeypot for USB malware
 * Copyright (C) 2011 to 2012  Sebastian Poeplau (sebastian.poeplau@gmail.com)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

#include "ghostdrive.h"
#include "ghostdrive_io.h"

#include <initguid.h>
#include <ntdddisk.h>
#include <mountmgr.h>
#include <mountdev.h>



/* Prototypes */

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD GhostDriveDeviceAdd;
EVT_WDF_DRIVER_UNLOAD GhostDriveUnload;
EVT_WDF_IO_QUEUE_IO_READ GhostDriveRead;
EVT_WDF_IO_QUEUE_IO_WRITE GhostDriveWrite;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL GhostDriveDeviceControl;
EVT_WDF_OBJECT_CONTEXT_CLEANUP GhostDriveDeviceCleanup;

NTSTATUS GhostDriveMountImage(WDFDEVICE Device, PUNICODE_STRING ImageName, PLARGE_INTEGER ImageSize);


/*
 * This is the entry function for the driver. We just use it to initialize the
 * Windows Driver Framework.
 */
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	WDF_DRIVER_CONFIG config;
	WDFDRIVER Driver;
	NTSTATUS status;

	KdPrint(("GhostDrive built %s %s\n", __DATE__, __TIME__));

	WDF_DRIVER_CONFIG_INIT(&config, GhostDriveDeviceAdd);
	config.EvtDriverUnload = GhostDriveUnload;
	status = WdfDriverCreate(DriverObject, RegistryPath, NULL, &config, &Driver);

	if (!NT_SUCCESS(status)) {
		KdPrint(("Unable to create WDF driver\n"));
		return status;
	}

	return status;
}


/*
 * DeviceAdd is called whenever GhostBus reports a new device.
 * It creates a device object for the device and sets up I/O handling.
 */
NTSTATUS GhostDriveDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit)
{
	NTSTATUS status;
	WDFDEVICE Device;
	WDF_OBJECT_ATTRIBUTES DeviceAttr;
	WCHAR devname[] = DRIVE_DEVICE_NAME;
	WCHAR linkname[] = DRIVE_LINK_NAME;
	WCHAR imagename[] = IMAGE_NAME;
	DECLARE_CONST_UNICODE_STRING(DeviceSDDL, DEVICE_SDDL_STRING);
	UNICODE_STRING DeviceName;
	UNICODE_STRING LinkName;
	UNICODE_STRING FileToMount;
	PGHOST_DRIVE_CONTEXT Context;
	WDF_IO_QUEUE_CONFIG QueueConfig;
	ULONG ID, Length;
	const GUID GUID_DEVINTERFACE_USBSTOR = {0xA5DCBF10L, 0x6530, 0x11D2, {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}};
	LARGE_INTEGER FileSize;

	KdPrint(("DeviceAdd called\n"));

	// Define the context for the new device
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&DeviceAttr, GHOST_DRIVE_CONTEXT);
	DeviceAttr.EvtCleanupCallback = GhostDriveDeviceCleanup;
	DeviceAttr.ExecutionLevel = WdfExecutionLevelPassive;

	// Create the device's name based on its ID
	status = WdfFdoInitQueryProperty(DeviceInit, DevicePropertyUINumber, sizeof(ULONG), &ID, &Length);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not retrieve drive ID\n"));
		return status;
	}

	devname[18] = (WCHAR)(ID + 0x30);
	RtlInitUnicodeString(&DeviceName, devname);

	// Name the new device
	status = WdfDeviceInitAssignName(DeviceInit, &DeviceName);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not assign a name\n"));
		return status;
	}

	// Set device properties
	WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_DISK);
	WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoDirect);
	WdfDeviceInitSetExclusive(DeviceInit, FALSE);
	status = WdfDeviceInitAssignSDDLString(DeviceInit, &DeviceSDDL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not assign an SDDL string\n"));
		return status;
	}

	// Create the device
	status = WdfDeviceCreate(&DeviceInit, &DeviceAttr, &Device);
	if (!NT_SUCCESS(status)) {
		KdPrint(("GhostDrive: Could not create a device: 0x%lx", status));
		return status;
	}

	// Initialize the device extension
	Context = GhostDriveGetContext(Device);
	Context->ImageMounted = FALSE;
	Context->ImageWritten = FALSE;
	Context->ID = ID;
	Context->ChangeCount = 0;

	// Copy the name to the device extension
	RtlCopyMemory(Context->DeviceName, DeviceName.Buffer, DeviceName.Length + sizeof(WCHAR));
	Context->DeviceNameLength = DeviceName.Length + sizeof(WCHAR);

	// Set up the device's I/O queue to handle I/O requests
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&QueueConfig, WdfIoQueueDispatchSequential);
	QueueConfig.EvtIoRead = GhostDriveRead;
	QueueConfig.EvtIoWrite = GhostDriveWrite;
	QueueConfig.EvtIoDeviceControl = GhostDriveDeviceControl;

	status = WdfIoQueueCreate(Device, &QueueConfig, WDF_NO_OBJECT_ATTRIBUTES, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not create the I/O queue\n"));
		return status;
	}

	// Create symbolic link for volume
	linkname[22] = (WCHAR)(ID + 0x30);
	RtlInitUnicodeString(&LinkName, linkname);

	status = WdfDeviceCreateSymbolicLink(Device, &LinkName);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not create symbolic link\n"));
		return status;
	}

	// Mount the image
	imagename[17] = (WCHAR)(ID + 0x30);
	RtlInitUnicodeString(&FileToMount, imagename);
	FileSize.QuadPart = 100 * 1024 * 1024;
	status = GhostDriveMountImage(Device, &FileToMount, &FileSize);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Mount failed\n"));
		return status;
	}

	WdfDeviceSetCharacteristics(Device, FILE_REMOVABLE_MEDIA | FILE_DEVICE_SECURE_OPEN);

	// Register device interfaces
	status = WdfDeviceCreateDeviceInterface(Device, &GUID_DEVINTERFACE_USBSTOR, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("GhostDrive: Could not register for device interface USBSTOR\n"));
		return status;
	}
	status = WdfDeviceCreateDeviceInterface(Device, &GUID_DEVINTERFACE_DISK, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("GhostDrive: Could not register for device interface DISK\n"));
		return status;
	}
	status = WdfDeviceCreateDeviceInterface(Device, &MOUNTDEV_MOUNTED_DEVICE_GUID, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("GhostDrive: Could not register for mount manager device interface\n"));
		return status;
	}

	KdPrint(("DeviceAdd finished\n"));
	return status;
}


/*
 * Detach the image file from the device. After that, I/O is not possible anymore.
 */
NTSTATUS GhostDriveUmountImage(WDFDEVICE Device)
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
 * Mount the given image file. If the file does not yet exist, it will be created. If another image
 * is mounted already, the function returns an error.
 */
NTSTATUS GhostDriveMountImage(WDFDEVICE Device, PUNICODE_STRING ImageName, PLARGE_INTEGER ImageSize)
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
		KdPrint(("Could not create or open the image file %ws", ImageName->Buffer));
		return status;
	}

	if (ImageStatus.Information == FILE_CREATED) {
		KdPrint(("File created\n"));

		// Make the file sparse
		// TODO: No effect?
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
			KdPrint(("Could not set the new file's size\n"));
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
 * This routine is called by the framework when the driver is about to be unloaded.
 */
VOID GhostDriveUnload(WDFDRIVER Driver)
{
	KdPrint(("Unload called\n"));
}


/*
 * The framework calls this function whenever a read request is received.
 * We use the image file to service the request.
 */
VOID GhostDriveRead(WDFQUEUE Queue, WDFREQUEST Request, size_t Length)
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
VOID GhostDriveWrite(WDFQUEUE Queue, WDFREQUEST Request, size_t Length)
{
	NTSTATUS status;
	PGHOST_DRIVE_CONTEXT Context;
	WDF_REQUEST_PARAMETERS Params;
	LARGE_INTEGER Offset;
	IO_STATUS_BLOCK StatusBlock;
	WDFMEMORY InputMemory;
	PVOID Buffer;

	KdPrint(("Write called\n"));

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


/*
 * The framework calls this routine whenever Device Control IRPs have to be handled.
 * TODO: Dispatch to other functions.
 * TODO: Document control codes.
 */
VOID GhostDriveDeviceControl(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferLength,
			size_t InputBufferLength, ULONG IoControlCode)
{
	NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
	ULONG_PTR info = 0;

	switch (IoControlCode) {

	case IOCTL_STORAGE_GET_HOTPLUG_INFO:
		{
			PGHOST_DRIVE_CONTEXT Context;
			STORAGE_HOTPLUG_INFO HotplugInfo;
			WDFMEMORY OutputMemory;

			KdPrint(("GetHotplugInfo\n"));

			Context = GhostDriveGetContext(WdfIoQueueGetDevice(Queue));

			// Retrieve the output memory
			status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
				break;
			}

			// Copy initial values to the local data structure
			status = WdfMemoryCopyToBuffer(OutputMemory, 0, &HotplugInfo, sizeof(STORAGE_HOTPLUG_INFO));
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not initialize data structure from memory: 0x%lx\n", status));
				break;
			}

			HotplugInfo.MediaRemovable = TRUE;
			HotplugInfo.MediaHotplug = TRUE;
			HotplugInfo.DeviceHotplug = TRUE;
			//HotplugInfo.WriteCacheEnableOverride = TRUE;

			// Copy the data to the output buffer
			status = WdfMemoryCopyFromBuffer(OutputMemory, 0, &HotplugInfo, sizeof(STORAGE_HOTPLUG_INFO));
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not copy data to the output memory: 0x%lx\n", status));
				break;
			}

			info = sizeof(STORAGE_HOTPLUG_INFO);
			status = STATUS_SUCCESS;
			break;
		}

	case IOCTL_DISK_GET_PARTITION_INFO:
		{
			PGHOST_DRIVE_CONTEXT Context;
			PARTITION_INFORMATION PartitionInfo;
			WDFMEMORY OutputMemory;

			KdPrint(("PartitionInfo\n"));

			Context = GhostDriveGetContext(WdfIoQueueGetDevice(Queue));

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
				break;
			}

			// Copy the data to the output buffer
			status = WdfMemoryCopyFromBuffer(OutputMemory, 0, &PartitionInfo, sizeof(PARTITION_INFORMATION));
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not copy data to the output memory: 0x%lx\n", status));
				break;
			}

			info = sizeof(PARTITION_INFORMATION);
			status = STATUS_SUCCESS;
			break;
		}

	case IOCTL_DISK_GET_PARTITION_INFO_EX:
		KdPrint(("GetPartitionInfoEx\n"));
		break;

	case IOCTL_DISK_SET_PARTITION_INFO:
		KdPrint(("SetPartitionInfo\n"));
		break;

	case IOCTL_DISK_IS_WRITABLE:
		KdPrint(("IsWritable\n"));
		status = STATUS_SUCCESS;
		break;

	case IOCTL_DISK_VERIFY:
		KdPrint(("Verify\n"));
		status = STATUS_SUCCESS;
		break;

	case IOCTL_STORAGE_CHECK_VERIFY:
	case IOCTL_STORAGE_CHECK_VERIFY2:
	case IOCTL_DISK_CHECK_VERIFY:
		{
			PGHOST_DRIVE_CONTEXT Context;
			WDFMEMORY OutputMemory;
		
			KdPrint(("CheckVerify\n"));

			Context = GhostDriveGetContext(WdfIoQueueGetDevice(Queue));

			// Optionally, we get a buffer for the change counter
			if (OutputBufferLength > 0) {
				// Retrieve the output memory
				status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
				if (!NT_SUCCESS(status)) {
					KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
					break;
				}

				// Copy the data to the output buffer
				status = WdfMemoryCopyFromBuffer(OutputMemory, 0, &Context->ChangeCount, sizeof(ULONG));
				if (!NT_SUCCESS(status)) {
					KdPrint(("Could not copy data to the output memory: 0x%lx\n", status));
					status = STATUS_BUFFER_TOO_SMALL;
					break;
				}

				info = sizeof(ULONG);
			}
			
			status = STATUS_SUCCESS;
			break;
		}

	case IOCTL_DISK_GET_DRIVE_GEOMETRY:
		{
			DISK_GEOMETRY Geometry;
			PGHOST_DRIVE_CONTEXT Context;
			WDFMEMORY OutputMemory;

			KdPrint(("GetDriveGeometry\n"));

			Context = GhostDriveGetContext(WdfIoQueueGetDevice(Queue));

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
				break;
			}

			// Copy the data to the output buffer
			status = WdfMemoryCopyFromBuffer(OutputMemory, 0, &Geometry, sizeof(DISK_GEOMETRY));
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not copy data to the output memory: 0x%lx\n", status));
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			info = sizeof(DISK_GEOMETRY);
			status = STATUS_SUCCESS;
			break;
		}

	case IOCTL_DISK_GET_DRIVE_GEOMETRY_EX:
		KdPrint(("GetDriveGeometryEx\n"));
		break;

	case IOCTL_DISK_GET_DRIVE_LAYOUT:
	case IOCTL_DISK_GET_DRIVE_LAYOUT_EX:
		KdPrint(("GetDriveLayout\n"));
		break;

	case IOCTL_DISK_GET_LENGTH_INFO:
		{
			GET_LENGTH_INFORMATION LengthInfo;
			PGHOST_DRIVE_CONTEXT Context;
			WDFMEMORY OutputMemory;

			KdPrint(("GetLengthInfo\n"));

			Context = GhostDriveGetContext(WdfIoQueueGetDevice(Queue));

			LengthInfo.Length.QuadPart = Context->ImageSize.QuadPart;

			// Retrieve the output memory
			status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
				break;
			}

			// Copy the data to the output buffer
			status = WdfMemoryCopyFromBuffer(OutputMemory, 0, &LengthInfo, sizeof(GET_LENGTH_INFORMATION));
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not copy data to the output memory: 0x%lx\n", status));
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			info = sizeof(GET_LENGTH_INFORMATION);
			status = STATUS_SUCCESS;
			break;
		}

	case IOCTL_GHOST_DRIVE_MOUNT_IMAGE:
		{
			WDFDEVICE Device;
			PGHOST_DRIVE_MOUNT_PARAMETERS Parameters;
			UNICODE_STRING ImageName;

			KdPrint(("Mount\n"));

			Device = WdfIoQueueGetDevice(Queue);

			// Get information on the file to be mounted
			status = WdfRequestRetrieveInputBuffer(Request, sizeof(GHOST_DRIVE_MOUNT_PARAMETERS), &Parameters, NULL);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Not enough information\n"));
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			KdPrint(("%ws\n", Parameters->ImageName));

			RtlInitUnicodeString(&ImageName, Parameters->ImageName);
			status = GhostDriveMountImage(Device, &ImageName, &Parameters->RequestedSize);
			break;
		}

	case IOCTL_GHOST_DRIVE_UMOUNT_IMAGE:
		{
			WDFDEVICE Device;
			PGHOST_DRIVE_CONTEXT Context;
			PBOOLEAN Written = NULL;

			KdPrint(("Umount\n"));

			status = WdfRequestRetrieveOutputBuffer(Request, sizeof(BOOLEAN), &Written, NULL);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Buffer too small for write detection results\n"));
				// continue anyway
			}

			Device = WdfIoQueueGetDevice(Queue);
			status = GhostDriveUmountImage(Device);

			if (Written != NULL) {
				Context = GhostDriveGetContext(WdfIoQueueGetDevice(Queue));
				*Written = Context->ImageWritten;
				info = sizeof(BOOLEAN);
			}

			break;
		}

	case IOCTL_MOUNTDEV_QUERY_DEVICE_NAME:
		{
			PGHOST_DRIVE_CONTEXT Context;
			USHORT NameLength;
			WDFMEMORY OutputMemory;

			KdPrint(("QueryDeviceName\n"));

			Context = GhostDriveGetContext(WdfIoQueueGetDevice(Queue));

			// Retrieve the output memory
			status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
				break;
			}

			NameLength = Context->DeviceNameLength - sizeof(WCHAR);

			// Copy the length to the output buffer
			status = WdfMemoryCopyFromBuffer(OutputMemory, FIELD_OFFSET(MOUNTDEV_NAME, NameLength), &NameLength, sizeof(USHORT));
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not copy the name's length to the output memory: 0x%lx\n", status));
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			// Copy the data to the output buffer
			status = WdfMemoryCopyFromBuffer(OutputMemory, FIELD_OFFSET(MOUNTDEV_NAME, Name), Context->DeviceName, Context->DeviceNameLength);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not copy the name to the output memory: 0x%lx\n", status));
				info = sizeof(MOUNTDEV_NAME);
				status = STATUS_BUFFER_OVERFLOW;
				break;
			}

			info = Context->DeviceNameLength;
			status = STATUS_SUCCESS;
			break;
		}

	case IOCTL_MOUNTDEV_QUERY_UNIQUE_ID:
		{
			PGHOST_DRIVE_CONTEXT Context;
			WDFMEMORY OutputMemory;
			USHORT Length;
			UCHAR UID;

			KdPrint(("QueryUniqueID\n"));

			Context = GhostDriveGetContext(WdfIoQueueGetDevice(Queue));

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
				break;
			}

			Length = 1;
			UID = (UCHAR)Context->ID + 0x30;

			// Copy the data to the output buffer
			status = WdfMemoryCopyFromBuffer(OutputMemory, FIELD_OFFSET(MOUNTDEV_UNIQUE_ID, UniqueIdLength), &Length, sizeof(USHORT));
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not copy the ID's length to the output memory: 0x%lx\n", status));
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			status = WdfMemoryCopyFromBuffer(OutputMemory, FIELD_OFFSET(MOUNTDEV_UNIQUE_ID, UniqueId), &UID, sizeof(UCHAR));
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not copy the ID's length to the output memory: 0x%lx\n", status));
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			info = Length + sizeof(USHORT);
			status = STATUS_SUCCESS;
			break;
		}

	case IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME:
		KdPrint(("MountManager\n"));
		break;

	case IOCTL_STORAGE_QUERY_PROPERTY:
		{
			STORAGE_PROPERTY_QUERY Query;
			STORAGE_DEVICE_DESCRIPTOR DeviceDescriptor;
			WDFMEMORY InputMemory;
			WDFMEMORY OutputMemory;

			KdPrint(("QueryProperty\n"));

			// Retrieve the input memory
			status = WdfRequestRetrieveInputMemory(Request, &InputMemory);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
				break;
			}

			// Copy the input values to the local data structure
			status = WdfMemoryCopyToBuffer(InputMemory, 0, &Query, sizeof(STORAGE_PROPERTY_QUERY));
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not read input data structure from memory: 0x%lx\n", status));
				break;
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
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			if (Query.PropertyId != StorageDeviceProperty) {
				KdPrint(("PropertyId not supported\n"));
				status = STATUS_NOT_SUPPORTED;
				break;
			}

			// Retrieve the output memory
			status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
				break;
			}

			// Copy initial values to the local data structure
			status = WdfMemoryCopyToBuffer(OutputMemory, 0, &DeviceDescriptor, sizeof(STORAGE_DEVICE_DESCRIPTOR));
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not initialize output data structure from memory: 0x%lx\n", status));
				// the system just tries to find out whether we support the query, but does not want real data
				status = STATUS_SUCCESS;
				break;
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
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			info = sizeof(STORAGE_DEVICE_DESCRIPTOR);
			status = STATUS_SUCCESS;
			break;
		}

	case IOCTL_STORAGE_GET_DEVICE_NUMBER:
		{
			STORAGE_DEVICE_NUMBER DeviceNumber;
			PGHOST_DRIVE_CONTEXT Context;
			WDFMEMORY OutputMemory;

			KdPrint(("GetDeviceNumber\n"));

			Context = GhostDriveGetContext(WdfIoQueueGetDevice(Queue));

			DeviceNumber.DeviceType = FILE_DEVICE_DISK;
			DeviceNumber.DeviceNumber = Context->ID + 10;
			DeviceNumber.PartitionNumber = 0;

			// Retrieve the output memory
			status = WdfRequestRetrieveOutputMemory(Request, &OutputMemory);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not retrieve the output memory: 0x%lx\n", status));
				break;
			}

			// Copy the data to the output buffer
			status = WdfMemoryCopyFromBuffer(OutputMemory, 0, &DeviceNumber, sizeof(STORAGE_DEVICE_NUMBER));
			if (!NT_SUCCESS(status)) {
				KdPrint(("Could not copy the STORAGE_DEVICE_NUMBER structure to the output memory: 0x%lx\n", status));
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			info = sizeof(STORAGE_DEVICE_NUMBER);
			status = STATUS_SUCCESS;
			break;
		}

	case IOCTL_STORAGE_GET_MEDIA_SERIAL_NUMBER:
		KdPrint(("GetMediaSerialNumber"));
		break;

	case IOCTL_STORAGE_LOAD_MEDIA:
		KdPrint(("LoadMedia"));
		break;

	default:
		KdPrint(("Unknown code\n"));
		break;

	}

	WdfRequestCompleteWithInformation(Request, status, info);
}


/*
 * This function is called just before the device object is destroyed. We use it
 * to unmount the image file in order to ensure that all data is written to disk
 * successfully before the file handle is discarded.
 */
VOID GhostDriveDeviceCleanup(WDFOBJECT Object)
{
	NTSTATUS status;
	PGHOST_DRIVE_CONTEXT Context;

	Context = GhostDriveGetContext(Object);
	KdPrint(("Image has been written to: %u\n", Context->ImageWritten));
	if (Context->ImageMounted)
		GhostDriveUmountImage(Object);
}
