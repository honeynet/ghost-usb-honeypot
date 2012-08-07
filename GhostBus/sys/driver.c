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

#include "driver.h"
#include "ghostbus.h"
#include <version.h>

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
 * GhostBusDeviceControl handles I/O request packages with device control codes,
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

	default:

		break;

	}

	WdfRequestCompleteWithInformation(Request, status, info);
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
	WDFDEVICE ChildDevice;
	WDF_DEVICE_PNP_CAPABILITIES PnpCaps;
	PGHOST_DRIVE_IDENTIFICATION Identification = (PGHOST_DRIVE_IDENTIFICATION)IdentificationDescription;
	NTSTATUS status;

	KdPrint(("ChildListCreateDevice called\n"));

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

	// Create the device
	status = WdfDeviceCreate(&ChildInit, WDF_NO_OBJECT_ATTRIBUTES, &ChildDevice);
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

	return STATUS_SUCCESS;
}
