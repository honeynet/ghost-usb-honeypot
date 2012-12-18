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

#include "extension.h"


DRIVER_UNLOAD RoDriverUnload;
DRIVER_ADD_DEVICE RoAddDevice;

DRIVER_DISPATCH RoDispatchPnp;
DRIVER_DISPATCH RoDispatchSkip;

IO_COMPLETION_ROUTINE RoCompleteStartDevice;


NTSTATUS DriverEntry(struct _DRIVER_OBJECT *DriverObject, PUNICODE_STRING RegistryPath)
{
	int i;
	
	KdPrint((__FUNCTION__": Filter entry\n"));
	
	// Set the other entry points
	DriverObject->DriverExtension->AddDevice = RoAddDevice;
	DriverObject->DriverUnload = RoDriverUnload;
	
	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
		DriverObject->MajorFunction[i] = RoDispatchSkip;
	}
	DriverObject->MajorFunction[IRP_MJ_PNP] = RoDispatchPnp;
	
	return STATUS_SUCCESS;
}


VOID RoDriverUnload(struct _DRIVER_OBJECT *DriverObject)
{
	KdPrint((__FUNCTION__": Driver unload\n"));
}


NTSTATUS RoAddDevice(struct _DRIVER_OBJECT *DriverObject, struct _DEVICE_OBJECT *PhysicalDeviceObject)
{
	NTSTATUS status;
	PDEVICE_OBJECT DeviceObject;
	PRO_DEV_EXTENSION Extension;
	
	KdPrint((__FUNCTION__": New device\n"));
	
	// Create a device
	status = IoCreateDevice(DriverObject, sizeof(RO_DEV_EXTENSION), NULL,
		PhysicalDeviceObject->DeviceType, PhysicalDeviceObject->Characteristics, FALSE, &DeviceObject);
	if (!NT_SUCCESS(status)) {
		KdPrint((__FUNCTION__": Device creation failed\n"));
		return status;
	}
	
	// Store the PDO
	Extension = DeviceObject->DeviceExtension;
	Extension->PDO = PhysicalDeviceObject;
	
	// Attach the device object to the stack
	Extension->LowerDO = IoAttachDeviceToDeviceStack(DeviceObject, PhysicalDeviceObject);
	
	// Finish initialization
	DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
	
	return STATUS_SUCCESS;
}


/*
 * Just forward an IRP to the next-lower driver.
 */
NTSTATUS RoDispatchSkip(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	PRO_DEV_EXTENSION Extension = DeviceObject->DeviceExtension;
	
	// Skip the stack location because we don't use it
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(Extension->LowerDO, Irp);
}


/*
 * Pause completion and signal that all lower drivers have completed the IRP.
 */
NTSTATUS RoCompleteStartDevice(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	PKEVENT SyncEvent = Context;
	KeSetEvent(SyncEvent, IO_NO_INCREMENT, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS RoDispatchPnp(struct _DEVICE_OBJECT *DeviceObject, struct _IRP *Irp)
{
	NTSTATUS status;
	PIO_STACK_LOCATION StackLocation = IoGetCurrentIrpStackLocation(Irp);
	PRO_DEV_EXTENSION Extension = DeviceObject->DeviceExtension;
	KEVENT SyncEvent;
	
	switch (StackLocation->MinorFunction) {
		case IRP_MN_START_DEVICE:
			KdPrint((__FUNCTION__": Start device\n"));
		
			// We can't skip the stack location because we register a completion routine
			IoCopyCurrentIrpStackLocationToNext(Irp);
			KeInitializeEvent(&SyncEvent, NotificationEvent, FALSE);
			
			// Wait for the IRP to be completed by all lower drivers
			IoSetCompletionRoutine(Irp, RoCompleteStartDevice, &SyncEvent, TRUE, TRUE, TRUE);
			status = IoCallDriver(Extension->LowerDO, Irp);
			if (status == STATUS_PENDING) {
				KeWaitForSingleObject(&SyncEvent, Executive, KernelMode, FALSE, NULL);
			}
			
			// Initialize if no error was reported
			if (!NT_SUCCESS(Irp->IoStatus.Status)) {
				KdPrint((__FUNCTION__": Someone failed the start request\n"));
			}
			else {
				// TODO: Initialize
				Irp->IoStatus.Status = STATUS_SUCCESS;
				status = STATUS_SUCCESS;
			}
			
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			return status;
			
		case IRP_MN_REMOVE_DEVICE:
			// Forward the request to lower drivers
			IoSkipCurrentIrpStackLocation(Irp);
			status = IoCallDriver(Extension->LowerDO, Irp);
			if (!NT_SUCCESS(status)) {
				KdPrint((__FUNCTION__": Drivers must not fail the removal request!\n"));
			}
			
			// Detach the device from the driver stack
			IoDetachDevice(Extension->LowerDO);
			
			// Delete the device object
			IoDeleteDevice(DeviceObject);
			
			// Return the result of IoCallDriver
			return status;
		
		default:
			return RoDispatchSkip(DeviceObject, Irp);
	}
}
