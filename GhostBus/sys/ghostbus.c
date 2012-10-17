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

#include "ghostbus_internal.h"
#include "ghostbus.h"
#include "file_io.h"
#include "device_control.h"
#include <version.h>

#include <ntstrsafe.h>
#include <wdmguid.h>


/*
 * Declaration of framework callback methods and DriverEntry
 */
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD GhostBusDeviceAdd;
EVT_WDF_DRIVER_UNLOAD GhostBusUnload;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL GhostBusDeviceControl;
EVT_WDF_CHILD_LIST_CREATE_DEVICE GhostBusChildListCreateDevice;


/*
 * This is the driver's main routine. It is called once when the driver is loaded.
 *
 * DriverEntry mainly creates the framework driver object and registers the AddDevice routine.
 */
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) 
{
	WDF_DRIVER_CONFIG config;
	WDFDRIVER Driver;
	NTSTATUS status;

	KdPrint(("GhostBus version %s built %s %s\n", GHOST_VERSION, __DATE__, __TIME__));

	WDF_DRIVER_CONFIG_INIT(&config, GhostBusDeviceAdd);
	config.EvtDriverUnload = GhostBusUnload;
	status = WdfDriverCreate(DriverObject, RegistryPath, NULL, &config, &Driver);

	if (!NT_SUCCESS(status)) {
		KdPrint(("Unable to create WDF driver\n"));
		return status;
	}

	return status;
}


/*
 * This function is called just before the device object is destroyed. We use it
 * to unmount the image file in order to ensure that all data is written to disk
 * successfully before the file handle is discarded.
 */
VOID GhostDrivePdoDeviceCleanup(WDFOBJECT Object)
{
	NTSTATUS status;
	PGHOST_DRIVE_PDO_CONTEXT Context;
	PGHOST_INFO_PROCESS_DATA ProcessInfo;
	int counter;

	Context = GhostDrivePdoGetContext(Object);
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


/*
 * This routine is called by the framework whenever a new device is found and needs to be installed.
 * For GhostBus, there should be no more than one device in the system.
 *
 * GhostBusDeviceAdd sets up a symbolic link for user-mode applications to communicate with the
 * device and initializes I/O and device properties.
 */
NTSTATUS GhostBusDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit)
{
	NTSTATUS status;
	WDFDEVICE Device;
	DECLARE_CONST_UNICODE_STRING(DeviceName, BUS_DEVICE_NAME);
	DECLARE_CONST_UNICODE_STRING(LinkName, BUS_LINK_NAME);
	WDF_IO_QUEUE_CONFIG QueueConfig;
	WDF_CHILD_LIST_CONFIG ChildListConfig;
	PNP_BUS_INFORMATION BusInfo;
	WDF_OBJECT_ATTRIBUTES DeviceAttr;

	KdPrint(("DeviceAdd called\n"));

	// Set the maximum execution level
	WDF_OBJECT_ATTRIBUTES_INIT(&DeviceAttr);
	DeviceAttr.ExecutionLevel = WdfExecutionLevelPassive;

	// Name the new device
	status = WdfDeviceInitAssignName(DeviceInit, &DeviceName);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not assign a name\n"));
		return status;
	}

	// Set device properties
	//WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_BUS_EXTENDER);
	WdfDeviceInitSetExclusive(DeviceInit, TRUE);
	WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoBuffered);

	// Initialize the device's child list
	WDF_CHILD_LIST_CONFIG_INIT(&ChildListConfig, sizeof(GHOST_DRIVE_IDENTIFICATION),
								GhostBusChildListCreateDevice);
	WdfFdoInitSetDefaultChildListConfig(DeviceInit, &ChildListConfig, WDF_NO_OBJECT_ATTRIBUTES);

	// Create the device
	status = WdfDeviceCreate(&DeviceInit, &DeviceAttr, &Device);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not create a device\n"));
		return status;
	}

	// Set up the device's I/O queue to handle I/O requests
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&QueueConfig, WdfIoQueueDispatchSequential);
	QueueConfig.EvtIoDeviceControl = GhostBusDeviceControl;
	status = WdfIoQueueCreate(Device, &QueueConfig, WDF_NO_OBJECT_ATTRIBUTES, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could create the I/O queue\n"));
		return status;
	}

	// Set information for child devices
	//BusInfo.BusTypeGuid = GUID_BUS_TYPE_GHOST;
	BusInfo.BusTypeGuid = GUID_BUS_TYPE_USB;
	BusInfo.LegacyBusType = PNPBus;
	BusInfo.BusNumber = 0;
	WdfDeviceSetBusInformationForChildren(Device, &BusInfo);

	// Create symbolic link in user-reachable directory
	status = WdfDeviceCreateSymbolicLink(Device, &LinkName);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not create symbolic link\n"));
		return status;
	}

	return status;
}


/*
 * This routine is called by the framework when the driver is about to be unloaded.
 */
VOID GhostBusUnload(WDFDRIVER Driver)
{
	KdPrint(("Unload called\n"));
}


/*
 * This routine initiates an image file for the given drive PDO.
 */
NTSTATUS GhostDrivePdoInitImage(WDFDEVICE Device, ULONG ID) {
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
	
	FileSize.QuadPart = 100 * 1024 * 1024;
	KdPrint(("Image file name: %wZ\n", &FileToMount));
	status = GhostFileIoMountImage(Device, &FileToMount, &FileSize);
	
#ifndef USE_FIXED_IMAGE_NAME
	ExFreePoolWithTag(FileToMount.Buffer, TAG);
#endif
	
	return status;
}


/*
 * GhostBusDeviceControl handles I/O request packages with device control codes for the bus FDO,
 * that is especially user-defined codes controlling child enumeration.
 *
 * TODO: Move request handling to separate functions.
 */
VOID GhostBusDeviceControl(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferLength,
							size_t InputBufferLength, ULONG IoControlCode)
{
	GHOST_DRIVE_IDENTIFICATION Identification;
	WDFCHILDLIST ChildList;
	PLONG pID;
	LONG ID;
	ULONG_PTR info = 0;
	NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

	KdPrint(("DeviceControl called\n"));

	switch (IoControlCode) {

	case IOCTL_GHOST_BUS_ENABLE_DRIVE:
		KdPrint(("Enabling GhostDrive\n"));

		// Retrieve the drive's ID from the request's input buffer
		status = WdfRequestRetrieveInputBuffer(Request, sizeof(LONG), &pID, NULL);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Could not retrieve ID\n"));
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		// Inform the PnP manager of a new device being plugged in
		ChildList = WdfFdoGetDefaultChildList(WdfIoQueueGetDevice(Queue));
		WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(&Identification.Header, sizeof(Identification));

		// If the requested device ID is the special value -1, then we look for a free device ID
		ID = *pID;
		if (ID == -1) {
			int i;
			WDF_CHILD_LIST_ITERATOR Iterator;
			WDF_CHILD_RETRIEVE_INFO ChildInfo;
			WDFDEVICE Device;
			
			WDF_CHILD_RETRIEVE_INFO_INIT(&ChildInfo, &Identification.Header);
			WDF_CHILD_LIST_ITERATOR_INIT(&Iterator, WdfRetrieveAllChildren);
			WdfChildListBeginIteration(ChildList, &Iterator);
			
			for (i = 0; i < GHOST_DRIVE_MAX_NUM; i++) {
				Identification.Id = i;
				
				status = WdfChildListRetrieveNextDevice(ChildList, &Iterator, &Device, &ChildInfo);
				if (status == STATUS_NO_MORE_ENTRIES) {
					KdPrint(("Device ID %d is free\n", i));
					ID = i;
					break;
				}
				else if (!NT_SUCCESS(status)) {
					KdPrint(("Could not retrieve child device\n"));
				}
			}
			
			WdfChildListEndIteration(ChildList, &Iterator);
			
			// If we didn't find a free ID, then fail
			if (ID == -1) {
				KdPrint(("No free ID\n"));
				status = STATUS_UNSUCCESSFUL;
				break;
			}
		}
		
		Identification.Id = ID;

		status = WdfChildListAddOrUpdateChildDescriptionAsPresent(ChildList, &Identification.Header, NULL);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Could not add child to child list\n"));
			status = STATUS_INTERNAL_ERROR;
			break;
		}
		
		// Write the device's ID to the output buffer
		status = WdfRequestRetrieveInputBuffer(Request, sizeof(LONG), &pID, NULL);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Could not retrieve the output buffer\n"));
			break;
		}
		
		*pID = ID;
		info = sizeof(LONG);
		
		status = STATUS_SUCCESS;
		break;

	case IOCTL_GHOST_BUS_DISABLE_DRIVE:
	{
		WDFDEVICE ChildPdo;
		WDF_CHILD_RETRIEVE_INFO ChildInfo;
		
		KdPrint(("Disabling GhostDrive\n"));

		// Retrieve the drive's ID from the request's input buffer
		status = WdfRequestRetrieveInputBuffer(Request, sizeof(LONG), &pID, NULL);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Could not retrieve ID\n"));
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		// Inform the PnP manager that a child device has been removed
		ChildList = WdfFdoGetDefaultChildList(WdfIoQueueGetDevice(Queue));

		WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(&Identification.Header, sizeof(Identification));
		Identification.Id = *pID;
		
		// Obtain the child's PDO to send it an umount command
		WDF_CHILD_RETRIEVE_INFO_INIT(&ChildInfo, &Identification.Header);
		ChildPdo = WdfChildListRetrievePdo(ChildList, &ChildInfo);
		if (ChildPdo == NULL) {
			KdPrint(("Bus: Couldn't obtain the drive PDO\n"));
		}

		/*status = WdfChildListUpdateChildDescriptionAsMissing(ChildList, &Identification.Header);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Could not remove child from child list\n"));
		}*/

		if (!WdfChildListRequestChildEject(ChildList, &Identification.Header)) {
			KdPrint(("Could not eject child device\n"));
			status = STATUS_INTERNAL_ERROR;
			break;
		}

		status = STATUS_SUCCESS;
		break;
	}
	
	case IOCTL_GHOST_DRIVE_GET_WRITER_INFO:
	{
		WDFDEVICE ChildPdo, BusDevice;
		WDF_CHILD_RETRIEVE_INFO ChildInfo;
		PGHOST_DRIVE_WRITER_INFO_PARAMETERS Params;
		WDFIOTARGET ChildIoTarget;
		WDF_IO_TARGET_OPEN_PARAMS OpenParams;
		WDFMEMORY InputMem, OutputMem;
		WDF_REQUEST_SEND_OPTIONS SendOptions;
		WDF_MEMORY_DESCRIPTOR InMemDesc, OutMemDesc;
		
		/*
		 * This is actually a request for information from one of our child devices.
		 * We'll find the corresponding drive PDO and forward the request. Sending the
		 * IRP to the bus rather than the drive has the advantage that the drive doesn't
		 * have to be visible to user-mode applications.
		 */
		KdPrint(("Bus: writer info request\n"));
		
		status = WdfRequestRetrieveInputBuffer(Request, sizeof(GHOST_DRIVE_WRITER_INFO_PARAMETERS), &Params, NULL);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Could not retrieve parameters\n"));
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		
		BusDevice = WdfIoQueueGetDevice(Queue);
		ChildList = WdfFdoGetDefaultChildList(BusDevice);
		WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(&Identification.Header, sizeof(Identification));
		Identification.Id = Params->DeviceID;
		WDF_CHILD_RETRIEVE_INFO_INIT(&ChildInfo, &Identification.Header);
		ChildPdo = WdfChildListRetrievePdo(ChildList, &ChildInfo);
		if (ChildPdo == NULL) {
			KdPrint(("Bus: couldn't obtain the drive PDO\n"));
		}
		
		status = WdfIoTargetCreate(BusDevice, WDF_NO_OBJECT_ATTRIBUTES, &ChildIoTarget);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Bus: couldn't create the I/O target: %lx\n", status));
			break;
		}
		
		WDF_IO_TARGET_OPEN_PARAMS_INIT_EXISTING_DEVICE(&OpenParams, WdfDeviceWdmGetDeviceObject(ChildPdo));
		status = WdfIoTargetOpen(ChildIoTarget, &OpenParams);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Bus: couldn't open the I/O target: %lx\n", status));
			break;
		}
		
		WdfRequestRetrieveInputMemory(Request, &InputMem);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Bus: couldn't obtain the input memory: %lx\n", status));
			break;
		}
		WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&InMemDesc, InputMem, NULL);
		
		WdfRequestRetrieveOutputMemory(Request, &OutputMem);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Bus: couldn't obtain the output memory: %lx\n", status));
			break;
		}
		WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&OutMemDesc, OutputMem, NULL);
		
		WDF_REQUEST_SEND_OPTIONS_INIT(&SendOptions, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
		
		/*
		status = WdfIoTargetFormatRequestForIoctl(ChildIoTarget, Request, IOCTL_GHOST_DRIVE_GET_WRITER_INFO, InputMem, NULL, OutputMem, NULL);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Bus: couldn't format the request: %lx\n", status));
			break;
		}
		
		status = WdfRequestSend(Request, ChildIoTarget, &SendOptions);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Bus: couldn't send the request: %lx\n", status));
			break;
		}
		*/
		
		status = WdfIoTargetSendIoctlSynchronously(ChildIoTarget, Request, IOCTL_GHOST_DRIVE_GET_WRITER_INFO, &InMemDesc, &OutMemDesc, NULL, &info);
		KdPrint(("Bus: request status: %lx", status));
		
		break;
	}

	default:

		break;

	}

	WdfRequestCompleteWithInformation(Request, status, info);
}


NTSTATUS GhostDrivePdoQueryRemove(WDFDEVICE Device) {
	PGHOST_DRIVE_PDO_CONTEXT Context;
	
	KdPrint(("QueryRemove\n"));
	
	Context = GhostDrivePdoGetContext(Device);
	KdPrint(("Draining the writer info queue...\n"));
	WdfIoQueuePurgeSynchronously(Context->WriterInfoQueue);
	
	if (!NT_SUCCESS(GhostFileIoUmountImage(Device))) {
		KdPrint(("Could not unmount the image\n"));
	}
	
	return STATUS_SUCCESS;
}


/*
 * The Windows Driver Framework calls this function whenever a new child device has to be created.
 * We create the device object and set its properties. Most importantly, we mark the device as a
 * removable one.
 */
NTSTATUS GhostBusChildListCreateDevice(WDFCHILDLIST ChildList,
					PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER IdentificationDescription,
					PWDFDEVICE_INIT ChildInit)
{
	DECLARE_CONST_UNICODE_STRING(DeviceID, GHOST_DRIVE_DEVICE_ID);
	DECLARE_CONST_UNICODE_STRING(DeviceDescription, GHOST_DRIVE_DESCRIPTION);
	DECLARE_CONST_UNICODE_STRING(DeviceLocation, GHOST_DRIVE_LOCATION);
	DECLARE_UNICODE_STRING_SIZE(InstanceID, 2 * sizeof(WCHAR));		// the ID is only one character
	DECLARE_CONST_UNICODE_STRING(DeviceSDDL, DEVICE_SDDL_STRING);
	WDFDEVICE ChildDevice;
	WDF_DEVICE_PNP_CAPABILITIES PnpCaps;
	PGHOST_DRIVE_IDENTIFICATION Identification = (PGHOST_DRIVE_IDENTIFICATION)IdentificationDescription;
	NTSTATUS status;
	WDF_OBJECT_ATTRIBUTES DeviceAttr;
	PGHOST_DRIVE_PDO_CONTEXT Context;
	ULONG ID;
	WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;
	WDF_OBJECT_ATTRIBUTES QueueAttr;
	WDF_IO_QUEUE_CONFIG QueueConfig;

	KdPrint(("ChildListCreateDevice called\n"));
	ID = ((PGHOST_DRIVE_IDENTIFICATION) IdentificationDescription)->Id;
	
	// Define the context for the new device
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&DeviceAttr, GHOST_DRIVE_PDO_CONTEXT);
	DeviceAttr.EvtCleanupCallback = GhostDrivePdoDeviceCleanup;
	DeviceAttr.ExecutionLevel = WdfExecutionLevelPassive;
	
	/*
	// begin tmp
	{
		DECLARE_CONST_UNICODE_STRING(GenDisk, L"GenDisk");
		
		// Set the device ID
		status = WdfPdoInitAssignDeviceID(ChildInit, &GenDisk);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Could not set the child device's ID\n"));
			WdfDeviceInitFree(ChildInit);
			return status;
		}

		// Set the hardware ID to the same value
		status = WdfPdoInitAddHardwareID(ChildInit, &GenDisk);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Could not set the child device's hardware ID\n"));
			WdfDeviceInitFree(ChildInit);
			return status;
		}
	}
	// end tmp
	*/
	
	// Set the device ID
	status = WdfPdoInitAssignDeviceID(ChildInit, &DeviceID);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not set the child device's ID\n"));
		WdfDeviceInitFree(ChildInit);
		return status;
	}

	// Set the hardware ID to the same value
	status = WdfPdoInitAddHardwareID(ChildInit, &DeviceID);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not set the child device's hardware ID\n"));
		WdfDeviceInitFree(ChildInit);
		return status;
	}

	// Set the instance ID
	status = RtlIntegerToUnicodeString(Identification->Id, 10, &InstanceID);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not create a string from the GhostDrive's ID\n"));
		WdfDeviceInitFree(ChildInit);
		return status;
	}

	status = WdfPdoInitAssignInstanceID(ChildInit, &InstanceID);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not assign the instance ID\n"));
		WdfDeviceInitFree(ChildInit);
		return status;
	}

	// Add a description and set default locale to United States English
	WdfPdoInitSetDefaultLocale(ChildInit, 0x409);
	status = WdfPdoInitAddDeviceText(ChildInit, &DeviceDescription, &DeviceLocation, 0x409);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not set a description for the child device\n"));
		WdfDeviceInitFree(ChildInit);
		return status;
	}
	
	// Set device properties
	WdfDeviceInitSetDeviceType(ChildInit, FILE_DEVICE_DISK);
	WdfDeviceInitSetIoType(ChildInit, WdfDeviceIoDirect);
	WdfDeviceInitSetExclusive(ChildInit, FALSE);
	status = WdfDeviceInitAssignSDDLString(ChildInit, &DeviceSDDL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not assign an SDDL string\n"));
		return status;
	}
	
	// Register callbacks
	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
	PnpPowerCallbacks.EvtDeviceQueryRemove = GhostDrivePdoQueryRemove;
	WdfDeviceInitSetPnpPowerEventCallbacks(ChildInit, &PnpPowerCallbacks);

	// Create the device
	status = WdfDeviceCreate(&ChildInit, &DeviceAttr, &ChildDevice);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not create the child device\n"));
		return status;
	}

	// Report PnP capabilities - this is the crucial part of the whole story, because here we can
	// make the device removable
	WDF_DEVICE_PNP_CAPABILITIES_INIT(&PnpCaps);
	PnpCaps.LockSupported = WdfFalse;
	PnpCaps.EjectSupported = WdfTrue;
	PnpCaps.Removable = WdfTrue;
	PnpCaps.DockDevice = WdfFalse;
	PnpCaps.UniqueID = WdfFalse;		// the ID is only unique to the bus
	PnpCaps.SilentInstall = WdfFalse;	// maybe change that later
	PnpCaps.SurpriseRemovalOK = WdfFalse;	// same here
	PnpCaps.HardwareDisabled = WdfFalse;
	PnpCaps.NoDisplayInUI = WdfFalse;
	PnpCaps.UINumber = Identification->Id;
	WdfDeviceSetPnpCapabilities(ChildDevice, &PnpCaps);
	
	// Initialize the device extension
	Context = GhostDrivePdoGetContext(ChildDevice);
	Context->ImageMounted = FALSE;
	Context->ImageWritten = FALSE;
	Context->ID = ID;
	Context->ChangeCount = 0;
	Context->WriterInfoCount = 0;
	Context->WriterInfo = NULL;
	
	// Set up the device's I/O queue to handle I/O requests
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&QueueConfig, WdfIoQueueDispatchSequential);
	QueueConfig.EvtIoRead = GhostFileIoRead;
	QueueConfig.EvtIoWrite = GhostFileIoWrite;
	QueueConfig.EvtIoDeviceControl = GhostDeviceControlDispatch;

	WDF_OBJECT_ATTRIBUTES_INIT(&QueueAttr);
	QueueAttr.ExecutionLevel = WdfExecutionLevelPassive;	// necessary to collect writer information

	status = WdfIoQueueCreate(ChildDevice, &QueueConfig, &QueueAttr, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not create the default I/O queue\n"));
		return status;
	}
	
	// Create a separate queue for blocking writer info requests
	WDF_IO_QUEUE_CONFIG_INIT(&QueueConfig, WdfIoQueueDispatchManual);

	status = WdfIoQueueCreate(ChildDevice, &QueueConfig, &QueueAttr, &Context->WriterInfoQueue);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Could not create the I/O queue for writer info requests\n"));
		return status;
	}
	
	// Mount the image
	status = GhostDrivePdoInitImage(ChildDevice, ID);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Mount failed\n"));
		return status;
	}

	return STATUS_SUCCESS;
}
