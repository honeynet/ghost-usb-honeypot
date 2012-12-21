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

#include <ntddstor.h>
#include <ntdddisk.h>


DRIVER_UNLOAD RoDriverUnload;
DRIVER_ADD_DEVICE RoAddDevice;

DRIVER_DISPATCH RoDispatchPnp;
DRIVER_DISPATCH RoDispatchDeviceControl;
DRIVER_DISPATCH RoDispatchSkip;

IO_COMPLETION_ROUTINE RoCompleteSetEvent;


/*
 * This is the first routine that the system calls.
 *
 * Inform the system of the other entry points.
 */
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
	//DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = RoDispatchDeviceControl;
	
	return STATUS_SUCCESS;
}


/*
 * This is the last routine that the system calls before unload.
 *
 * Just log.
 */
VOID RoDriverUnload(struct _DRIVER_OBJECT *DriverObject)
{
	KdPrint((__FUNCTION__": Driver unload\n"));
}


/*
 * The system calls this routine when we are supposed to control a new device.
 * 
 * Create a device object and attach it to the device stack. Copy all properties from the
 * underlying device object.
 */
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
	
	//KdPrint((__FUNCTION__": PDO type: 0x%x\n", PhysicalDeviceObject->DeviceType));
	//KdPrint((__FUNCTION__": PDO flags: 0x%x\n", PhysicalDeviceObject->Flags));
	//KdPrint((__FUNCTION__": PDO characteristics: 0x%x\n", PhysicalDeviceObject->Characteristics));
	
	// Use the same flags as the underlying device
	DeviceObject->Flags |= PhysicalDeviceObject->Flags;
	
	// Store the PDO
	Extension = DeviceObject->DeviceExtension;
	Extension->PDO = PhysicalDeviceObject;
	
	// Attach the device object to the stack
	Extension->LowerDO = IoAttachDeviceToDeviceStack(DeviceObject, PhysicalDeviceObject);
	
	//KdPrint((__FUNCTION__": LowerDO type: 0x%x\n", Extension->LowerDO->DeviceType));
	//KdPrint((__FUNCTION__": LowerDO flags: 0x%x\n", Extension->LowerDO->Flags));
	//KdPrint((__FUNCTION__": LowerDO characteristics: 0x%x\n", Extension->LowerDO->Characteristics));
	
	// Finish initialization
	DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
	
	return STATUS_SUCCESS;
}


/*
 * This is the default IRP handler.
 *
 * Just forward the IRP to the next-lower driver.
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
NTSTATUS RoCompleteSetEvent(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	PKEVENT SyncEvent = Context;
	KeSetEvent(SyncEvent, IO_NO_INCREMENT, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS RoWaitForIrpCompletion(PIRP Irp, PDEVICE_OBJECT LowerDO) {
	KEVENT SyncEvent;
	NTSTATUS status;
	
	// We can't skip the stack location because we register a completion routine
	IoCopyCurrentIrpStackLocationToNext(Irp);
	KeInitializeEvent(&SyncEvent, NotificationEvent, FALSE);
	
	// Wait for the IRP to be completed by all lower drivers
	IoSetCompletionRoutine(Irp, RoCompleteSetEvent, &SyncEvent, TRUE, TRUE, TRUE);
	status = IoCallDriver(LowerDO, Irp);
	if (status == STATUS_PENDING) {
		KeWaitForSingleObject(&SyncEvent, Executive, KernelMode, FALSE, NULL);
	}
	
	return Irp->IoStatus.Status;
}


/*
 * This routine handles all PNP IRPs.
 *
 * We only react to start and stop requests with device object initialization
 * and deletion, respectively.
 */
NTSTATUS RoDispatchPnp(struct _DEVICE_OBJECT *DeviceObject, struct _IRP *Irp)
{
	NTSTATUS status;
	PIO_STACK_LOCATION StackLocation = IoGetCurrentIrpStackLocation(Irp);
	PRO_DEV_EXTENSION Extension = DeviceObject->DeviceExtension;
	
	switch (StackLocation->MinorFunction) {
		case IRP_MN_START_DEVICE:
			KdPrint((__FUNCTION__": Start device\n"));
		
			// We have to handle this request on its way back up the stack
			RoWaitForIrpCompletion(Irp, Extension->LowerDO);
			
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


NTSTATUS RoDispatchDeviceControl(struct _DEVICE_OBJECT *DeviceObject, struct _IRP *Irp)
{
	NTSTATUS status;
	PIO_STACK_LOCATION StackLocation = IoGetCurrentIrpStackLocation(Irp);
	PRO_DEV_EXTENSION Extension = DeviceObject->DeviceExtension;
	
	switch (StackLocation->Parameters.DeviceIoControl.IoControlCode) {
		case IOCTL_STORAGE_QUERY_PROPERTY: {
			PSTORAGE_PROPERTY_QUERY Query = Irp->AssociatedIrp.SystemBuffer;
			
			if (StackLocation->Parameters.DeviceIoControl.InputBufferLength >= sizeof(STORAGE_PROPERTY_QUERY)
				&& Query->PropertyId == StorageDeviceProperty
				&& Query->QueryType == PropertyStandardQuery
				&& StackLocation->Parameters.DeviceIoControl.OutputBufferLength > 0)
			{
				status = RoWaitForIrpCompletion(Irp, Extension->LowerDO);
				KdPrint((__FUNCTION__": Query for device properties\n"));
				IoCompleteRequest(Irp, IO_NO_INCREMENT);
				return status;
			}
			else {
				return RoDispatchSkip(DeviceObject, Irp);
			}
		}
		
		case IOCTL_DISK_IS_WRITABLE:
			status = RoWaitForIrpCompletion(Irp, Extension->LowerDO);
			KdPrint((__FUNCTION__": Device is%s writable\n", NT_SUCCESS(status) ? "" : " not"));
			Irp->IoStatus.Status = STATUS_MEDIA_WRITE_PROTECTED;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			//return status;
			return STATUS_MEDIA_WRITE_PROTECTED;
		
		default:
			return RoDispatchSkip(DeviceObject, Irp);
	}
}
