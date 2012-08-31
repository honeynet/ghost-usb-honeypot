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
#include "file_io.h"
#include "device_control.h"

#include <initguid.h>
#include <ntdddisk.h>
#include <ntstrsafe.h>
#include <mountmgr.h>


/* Prototypes */

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD GhostDriveDeviceAdd;
EVT_WDF_DRIVER_UNLOAD GhostDriveUnload;
EVT_WDF_OBJECT_CONTEXT_CLEANUP GhostDriveDeviceCleanup;
EVT_WDF_DEVICE_QUERY_REMOVE GhostDriveQueryRemove;

NTSTATUS GhostDriveInitImage(WDFDEVICE Device, ULONG ID);


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
	WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;
	WCHAR devname[] = DRIVE_DEVICE_NAME;
	WCHAR linkname[] = DRIVE_LINK_NAME;
	DECLARE_CONST_UNICODE_STRING(DeviceSDDL, DEVICE_SDDL_STRING);
	UNICODE_STRING DeviceName;
	UNICODE_STRING LinkName;
	PGHOST_DRIVE_CONTEXT Context;
	WDF_IO_QUEUE_CONFIG QueueConfig;
	ULONG ID, Length;
	const GUID GUID_DEVINTERFACE_USBSTOR = {0xA5DCBF10L, 0x6530, 0x11D2, {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}};

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
	
	// Register callbacks
	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
	PnpPowerCallbacks.EvtDeviceQueryRemove = GhostDriveQueryRemove;
	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &PnpPowerCallbacks);

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
	Context->WriterInfoCount = 0;
	Context->WriterInfo = NULL;

	// Copy the name to the device extension
	RtlCopyMemory(Context->DeviceName, DeviceName.Buffer, DeviceName.Length + sizeof(WCHAR));
	Context->DeviceNameLength = DeviceName.Length + sizeof(WCHAR);

	// Set up the device's I/O queue to handle I/O requests
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&QueueConfig, WdfIoQueueDispatchSequential);
	QueueConfig.EvtIoRead = GhostFileIoRead;
	QueueConfig.EvtIoWrite = GhostFileIoWrite;
	QueueConfig.EvtIoDeviceControl = GhostDeviceControlDispatch;
	
	WDF_OBJECT_ATTRIBUTES_INIT(&QueueAttr);
	QueueAttr.ExecutionLevel = WdfExecutionLevelPassive;	// necessary to collect writer information

	status = WdfIoQueueCreate(Device, &QueueConfig, &QueueAttr, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not create the default I/O queue\n"));
		return status;
	}
	
	// Create a separate queue for blocking writer info requests
	WDF_IO_QUEUE_CONFIG_INIT(&QueueConfig, WdfIoQueueDispatchManual);

	status = WdfIoQueueCreate(Device, &QueueConfig, &QueueAttr, &Context->WriterInfoQueue);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not create the I/O queue for writer info requests\n"));
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
	status = GhostDriveInitImage(Device, ID);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Mount failed\n"));
		return status;
	}

	WdfDeviceSetCharacteristics(Device, FILE_REMOVABLE_MEDIA | FILE_DEVICE_SECURE_OPEN);

	// Register device interfaces
	/*status = WdfDeviceCreateDeviceInterface(Device, &GUID_DEVINTERFACE_USBSTOR, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("GhostDrive: Could not register for device interface USBSTOR\n"));
		return status;
	}*/
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


/*
 * This function is called just before the device object is destroyed. We use it
 * to unmount the image file in order to ensure that all data is written to disk
 * successfully before the file handle is discarded.
 */
VOID GhostDriveDeviceCleanup(WDFOBJECT Object)
{
	NTSTATUS status;
	PGHOST_DRIVE_CONTEXT Context;
	PGHOST_INFO_PROCESS_DATA ProcessInfo;
	int counter;

	Context = GhostDriveGetContext(Object);
	KdPrint(("Image has been written to: %u\n", Context->ImageWritten));
	if (Context->ImageMounted)
		GhostFileIoUmountImage(Object);
	
	// Free the writer info structs
	counter = 0;
	while (Context->WriterInfo != NULL) {
		ProcessInfo = Context->WriterInfo;
		Context->WriterInfo = Context->WriterInfo->Next;
		GhostInfoFreeProcessData(ProcessInfo);
		counter++;
	}
	
	KdPrint(("%d info struct(s) freed\n", counter));
}


NTSTATUS GhostDriveQueryRemove(WDFDEVICE Device) {
	PGHOST_DRIVE_CONTEXT Context;
	
	KdPrint(("QueryRemove\n"));
	
	Context = GhostDriveGetContext(Device);
	KdPrint(("Draining the writer info queue...\n"));
	WdfIoQueuePurgeSynchronously(Context->WriterInfoQueue);
	
	if (!NT_SUCCESS(GhostFileIoUmountImage(Device))) {
		KdPrint(("Could not unmount the image\n"));
	}
	
	return STATUS_SUCCESS;
}


NTSTATUS GhostDriveInitImage(WDFDEVICE Device, ULONG ID) {
	WCHAR imagename[] = IMAGE_NAME;
	LARGE_INTEGER FileSize;
	UNICODE_STRING FileToMount, RegValue;
	NTSTATUS status;
	WDFKEY ParametersKey;
	DECLARE_CONST_UNICODE_STRING(ValueName, L"ImageFileName0");
	USHORT ByteLength = 0;
	PWCHAR Buffer;
	int i;
	PCWSTR Prefix = L"\\DosDevices\\";
	size_t PrefixLength;
	
#ifndef USE_FIXED_IMAGE_NAME
	KdPrint(("Reading the image file name from the registry\n"));
	
	/* Read the desired image file name from the registry */
	status = WdfDriverOpenParametersRegistryKey(WdfGetDriver(), GENERIC_READ,
		WDF_NO_OBJECT_ATTRIBUTES, &ParametersKey);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not open the driver's registry key\n"));
		return status;
	}
	
	ValueName.Buffer[13] = (WCHAR)(ID + 0x30);
	status = WdfRegistryQueryUnicodeString(ParametersKey, &ValueName, &ByteLength, NULL);
	if (status != STATUS_BUFFER_OVERFLOW && !NT_SUCCESS(status)) {
		KdPrint(("Could not obtain the length of the image file name from the registry\n"));
		return status;
	}
	
	Buffer = ExAllocatePoolWithTag(NonPagedPool, ByteLength, TAG);
	RtlInitEmptyUnicodeString(&RegValue, Buffer, ByteLength);
	
	status = WdfRegistryQueryUnicodeString(ParametersKey, &ValueName, NULL, &RegValue);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not read the image file name from the registry\n"));
		return status;
	}	 
	
	WdfRegistryClose(ParametersKey);
	
	/* Add the required "\DosDevices" prefix */
	PrefixLength = wcslen(Prefix) * sizeof(WCHAR);
	Buffer = ExAllocatePoolWithTag(NonPagedPool, ByteLength + PrefixLength, TAG);
	RtlInitEmptyUnicodeString(&FileToMount, Buffer, ByteLength + PrefixLength);
	status = RtlUnicodeStringCopyString(&FileToMount, Prefix);
	if (!NT_SUCCESS(status)) {
		KdPrint(("String copy operation failed\n"));
		return status;
	}
	status = RtlUnicodeStringCat(&FileToMount, &RegValue);
	if (!NT_SUCCESS(status)) {
		KdPrint(("String copy operation failed\n"));
		return status;
	}
	
	ExFreePoolWithTag(RegValue.Buffer, TAG);
#else
	KdPrint(("Using the image file name that is compiled into the driver\n"));
	
	/* Use the fixed image file name */
	RtlInitUnicodeString(&FileToMount, imagename);
#endif	
	
	/* Set the correct ID in the file name */
	/*for (i = 0; i < FileToMount.Length; i++) {
		if (FileToMount.Buffer[i] == L'0') {
			KdPrint(("Inserting ID into device name\n"));
			FileToMount.Buffer[i] = (WCHAR)(ID + 0x30);
			break;
		}
	}*/
	
	FileSize.QuadPart = 100 * 1024 * 1024;
	KdPrint(("Image file name: %wZ\n", &FileToMount));
	status = GhostFileIoMountImage(Device, &FileToMount, &FileSize);
	
#ifndef USE_FIXED_IMAGE_NAME
	ExFreePoolWithTag(FileToMount.Buffer, TAG);
#endif
	
	return status;
}
