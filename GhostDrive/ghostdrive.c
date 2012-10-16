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


#include "ghostdrive.h"
#include "version.h"

#include <initguid.h>
#include <ntdddisk.h>
#include <ntstrsafe.h>
#include <mountmgr.h>
#include <mountdev.h>


/* Prototypes */

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD GhostDriveDeviceAdd;
EVT_WDF_DRIVER_UNLOAD GhostDriveUnload;
EVT_WDF_OBJECT_CONTEXT_CLEANUP GhostDriveDeviceCleanup;
EVT_WDF_DEVICE_QUERY_REMOVE GhostDriveQueryRemove;

NTSTATUS GhostDriveInitImage(WDFDEVICE Device, ULONG ID);
void GhostDeviceControlLinkCreated(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context);
void GhostDeviceControlQueryUniqueId(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context);
void GhostDeviceControlQueryDeviceName(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context);


VOID GhostFileIoReadWrite(WDFQUEUE Queue, WDFREQUEST Request, size_t Length)
{
	WDFIOTARGET IoTarget;
	WDF_REQUEST_SEND_OPTIONS Options;
	
	IoTarget = WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue));
	if (IoTarget == NULL) {
		KdPrint(("Drive: I/O target is NULL\n"));
		return;
	}
	
	WDF_REQUEST_SEND_OPTIONS_INIT(&Options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
	WdfRequestFormatRequestUsingCurrentType(Request);
	if (!WdfRequestSend(Request, IoTarget, &Options)) {
		KdPrint(("Drive: Send failed\n"));
	}
}


VOID GhostDeviceControlDispatch(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferLength,
			size_t InputBufferLength, ULONG IoControlCode)
{
	WDFDEVICE Device = WdfIoQueueGetDevice(Queue);
	PGHOST_DRIVE_CONTEXT Context = GhostDriveGetContext(Device);

	switch (IoControlCode) {

		case IOCTL_MOUNTDEV_QUERY_DEVICE_NAME:
			GhostDeviceControlQueryDeviceName(Request, Context);
			break;

		case IOCTL_MOUNTDEV_QUERY_UNIQUE_ID:
			GhostDeviceControlQueryUniqueId(Request, Context);
			break;
		
		case IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME:
			KdPrint(("Invalid - Suggested link name\n"));
			WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
			break;
			
		case IOCTL_MOUNTDEV_LINK_CREATED:
            GhostDeviceControlLinkCreated(Request, Context);
            break;

		default:
		{
			WDFIOTARGET IoTarget;
			WDF_REQUEST_SEND_OPTIONS Options;
			
			KdPrint(("Drive: Forwarding to bus\n"));

			IoTarget = WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue));
			if (IoTarget == NULL) {
				KdPrint(("Drive: I/O target is NULL\n"));
				return;
			}

			WDF_REQUEST_SEND_OPTIONS_INIT(&Options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
			WdfRequestFormatRequestUsingCurrentType(Request);
			if (!WdfRequestSend(Request, IoTarget, &Options)) {
				KdPrint(("Drive: Send failed\n"));
			}
			break;
		}
	}
}


void GhostDeviceControlLinkCreated(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context) {
    NTSTATUS status;
    PMOUNTDEV_NAME MountdevName;

    KdPrint(("Mount manager reports link creation\n"));

    status = WdfRequestRetrieveInputBuffer(Request, sizeof(MOUNTDEV_NAME), &MountdevName, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not retrieve input parameters: 0x%lx\n", status));
		WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
		return;
	}

    KdPrint(("Link: %ws\n", MountdevName->Name));
    WdfRequestComplete(Request, STATUS_SUCCESS);
}


void GhostDeviceControlQueryUniqueId(WDFREQUEST Request, PGHOST_DRIVE_CONTEXT Context) {
	WDFMEMORY OutputMemory;
	USHORT Length;
	UCHAR UID;
	NTSTATUS status;

	KdPrint(("QueryUniqueID\n"));

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


/*
 * This is the entry function for the driver. We just use it to initialize the
 * Windows Driver Framework.
 */
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	WDF_DRIVER_CONFIG config;
	WDFDRIVER Driver;
	NTSTATUS status;

	KdPrint(("GhostDrive version %s built %s %s\n", GHOST_VERSION, __DATE__, __TIME__));

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
	WDF_OBJECT_ATTRIBUTES DeviceAttr, QueueAttr;
	WCHAR devname[] = DRIVE_DEVICE_NAME;
	WCHAR linkname[] = DRIVE_LINK_NAME;
	DECLARE_CONST_UNICODE_STRING(DeviceSDDL, DEVICE_SDDL_STRING);
	UNICODE_STRING DeviceName;
	UNICODE_STRING LinkName;
	WDF_IO_QUEUE_CONFIG QueueConfig;
	ULONG ID, Length;
	PGHOST_DRIVE_CONTEXT Context;

	KdPrint(("DeviceAdd called\n"));
	
	// Define the context for the new device
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&DeviceAttr, GHOST_DRIVE_CONTEXT);
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
	
	// Copy the name to the device extension
	Context = GhostDriveGetContext(Device);
	RtlCopyMemory(Context->DeviceName, DeviceName.Buffer, DeviceName.Length + sizeof(WCHAR));
	Context->DeviceNameLength = DeviceName.Length + sizeof(WCHAR);

	// Set up the device's I/O queue to handle I/O requests
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&QueueConfig, WdfIoQueueDispatchSequential);
	QueueConfig.EvtIoRead = GhostFileIoReadWrite;
	QueueConfig.EvtIoWrite = GhostFileIoReadWrite;
	QueueConfig.EvtIoDeviceControl = GhostDeviceControlDispatch;
	
	WDF_OBJECT_ATTRIBUTES_INIT(&QueueAttr);
	QueueAttr.ExecutionLevel = WdfExecutionLevelPassive;	// necessary to collect writer information

	status = WdfIoQueueCreate(Device, &QueueConfig, &QueueAttr, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not create the default I/O queue\n"));
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

	WdfDeviceSetCharacteristics(Device, FILE_REMOVABLE_MEDIA | FILE_DEVICE_SECURE_OPEN);

	// Register device interfaces
	status = WdfDeviceCreateDeviceInterface(Device, &GUID_DEVINTERFACE_DISK, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("GhostDrive: Could not register for device interface DISK\n"));
		return status;
	}
#if (NTDDI_VERSION < NTDDI_VISTA)
	status = WdfDeviceCreateDeviceInterface(Device, &MOUNTDEV_MOUNTED_DEVICE_GUID, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("GhostDrive: Could not register for mount manager device interface\n"));
		return status;
	}
#endif

	KdPrint(("DeviceAdd finished\n"));
	return status;
}


/*
 * This routine is called by the framework when the driver is about to be unloaded.
 */
VOID GhostDriveUnload(WDFDRIVER Driver)
{
	KdPrint(("Unload called\n"));
}
