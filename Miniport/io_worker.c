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

#include "extensions.h"
#include "file_io.h"


BOOLEAN InitializeDrive(PGHOST_PORT_EXTENSION PortExtension, PGHOST_DRIVE_PDO_CONTEXT Context, ULONG DeviceID) {
	NTSTATUS status;
	SIZE_T NameBufferSize;
	PVOID NameBuffer;
	UNICODE_STRING ImageName;
	PREQUEST_PARAMETERS Parameters = PortExtension->MountParameters[DeviceID];
	
	KdPrint((__FUNCTION__": Initializing device %d\n", DeviceID));
	
	// Make sure that we have mount parameters
	if (Parameters == NULL) {
		// This should never happen
		KdPrint((__FUNCTION__": No mount parameters available\n"));
		PortExtension->DriveStates[DeviceID] = GhostDriveDisabled;
		return FALSE;
	}
	
	InitGhostDrivePdoContext(Context, DeviceID);
	
	// Mount the image
	NameBufferSize = (Parameters->MountInfo.ImageNameLength + 1) * sizeof(WCHAR);
	NameBuffer = ExAllocatePoolWithTag(PagedPool, NameBufferSize, GHOST_PORT_TAG);
	RtlZeroMemory(NameBuffer, NameBufferSize);
	RtlCopyMemory(NameBuffer, Parameters->MountInfo.ImageName, Parameters->MountInfo.ImageNameLength * sizeof(WCHAR));
	RtlInitUnicodeString(&ImageName, NameBuffer);
	
	KdPrint((__FUNCTION__": Using image %wZ\n", &ImageName));
	status = GhostFileIoMountImage(Context, &ImageName, &Parameters->MountInfo.ImageSize);
	if (!NT_SUCCESS(status)) {
		KdPrint((__FUNCTION__": Mount failed (0x%lx)\n", status));
		ExFreePoolWithTag(NameBuffer, GHOST_PORT_TAG);
		ExFreePoolWithTag(PortExtension->MountParameters[DeviceID], GHOST_PORT_TAG);
		PortExtension->MountParameters[DeviceID] = NULL;
		SetDriveState(PortExtension, DeviceID, GhostDriveDisabled, FALSE);
		StorPortNotification(BusChangeDetected, PortExtension, 0);
		return FALSE;
	}
	
	// Clean up
	ExFreePoolWithTag(NameBuffer, GHOST_PORT_TAG);
	ExFreePoolWithTag(PortExtension->MountParameters[DeviceID], GHOST_PORT_TAG);
	PortExtension->MountParameters[DeviceID] = NULL;
	
	// Finish initialization
	SetDriveState(PortExtension, DeviceID, GhostDriveEnabled, FALSE);
	return TRUE;
}


VOID RemoveDrive(PGHOST_PORT_EXTENSION PortExtension, PGHOST_DRIVE_PDO_CONTEXT Context) {
	NTSTATUS status;
	ULONG DeviceID = Context->ID;
	
	KdPrint((__FUNCTION__": Removing device %d\n", Context->ID));
	
	status = GhostFileIoUmountImage(Context);
	if (!NT_SUCCESS(status)) {
		KdPrint((__FUNCTION__": Umount failed (0x%lx)\n", status));
		SetDriveState(PortExtension, Context->ID, GhostDriveEnabled, FALSE);
		return;
	}
	DeleteGhostDrivePdoContext(Context);
	SetDriveState(PortExtension, DeviceID, GhostDriveDisabled, FALSE);
	StorPortNotification(BusChangeDetected, PortExtension, 0);
}


UCHAR ProcessIoWorkItem(PGHOST_PORT_EXTENSION PortExtension, PIO_WORK_ITEM WorkItem) {	
	NTSTATUS status;
	ULONG BlockOffset;
	USHORT BlockCount;
	PVOID Buffer;
	ULONG ByteLength;
	LARGE_INTEGER ByteOffset;
	PGHOST_DRIVE_PDO_CONTEXT Context = WorkItem->DriveContext;
	PCDB Cdb = (PCDB)WorkItem->Srb->Cdb;
	
	if (Cdb->CDB6GENERIC.OperationCode != SCSIOP_READ && Cdb->CDB6GENERIC.OperationCode != SCSIOP_WRITE) {
		KdPrint((__FUNCTION__": Invalid operation code\n"));
		return SRB_STATUS_INTERNAL_ERROR;
	}
	
	/*if (!Context->ImageMounted) {
		KdPrint((__FUNCTION__": No image mounted\n"));
		return SRB_STATUS_PENDING;
	}*/
	
	REVERSE_BYTES(&BlockOffset, &Cdb->CDB10.LogicalBlockByte0);
	REVERSE_BYTES_SHORT(&BlockCount, &Cdb->CDB10.TransferBlocksMsb);
	
	KdPrint((__FUNCTION__": %s %d blocks at offset %d\n", (Cdb->CDB6GENERIC.OperationCode == SCSIOP_READ) ? "Reading" : "Writing",
		BlockCount, BlockOffset));
	
	ByteLength = BlockCount * GHOST_BLOCK_SIZE;
	if (WorkItem->Srb->DataTransferLength < ByteLength) {
		KdPrint((__FUNCTION__": Buffer too small\n"));
		return SRB_STATUS_ERROR;
	}
	
	if (!NT_SUCCESS(StorPortGetSystemAddress(PortExtension, WorkItem->Srb, &Buffer))) {
		KdPrint((__FUNCTION__": No system address\n"));
		return SRB_STATUS_ERROR;
	}
	
	ByteOffset.QuadPart = BlockOffset * GHOST_BLOCK_SIZE;
	
	if (Cdb->CDB6GENERIC.OperationCode == SCSIOP_READ) {
		RtlZeroMemory(Buffer, WorkItem->Srb->DataTransferLength);
		status = GhostFileIoRead(Context, Buffer, ByteOffset, ByteLength);
	}
	else {
		status = GhostFileIoWrite(Context, Buffer, ByteOffset, ByteLength);
	}
	
	if (!NT_SUCCESS(status)) {
		KdPrint((__FUNCTION__": I/O failed (0x%lx)\n", status));
		return SRB_STATUS_ERROR;
	}
	
	return SRB_STATUS_SUCCESS;
}


/*
 * This is the main routine of the I/O worker thread, which
 * handles I/O requests that our I/O processing routine can't
 * process due to its high IRQL.
 */
VOID IoWorkerThread(PVOID ExecutionContext) {
	PGHOST_PORT_EXTENSION PortExtension = ExecutionContext;
	PKEVENT Events[2] = {&PortExtension->IoThreadTerminate, &PortExtension->IoWorkAvailable};
	
	while (TRUE) {
		PLIST_ENTRY CurrentEntry;
		PIO_WORK_ITEM WorkItem;
		
		CurrentEntry = ExInterlockedRemoveHeadList(&PortExtension->IoWorkItems, &PortExtension->IoWorkItemsLock);
		// If no work is available, wait for new work or a termination request
		if (CurrentEntry == NULL) {
			KdPrint((__FUNCTION__": Waiting for more work...\n"));
			
			if (KeWaitForMultipleObjects(2, Events, WaitAny, Executive, KernelMode, FALSE, NULL, NULL) == STATUS_WAIT_0) {
				KdPrint((__FUNCTION__": I/O thread terminating\n"));
				PsTerminateSystemThread(STATUS_SUCCESS);
				return;	// never reached
			}
			
			// New work available, so start over
			continue;
		}
		
		WorkItem = CONTAINING_RECORD(CurrentEntry, IO_WORK_ITEM, ListNode);
		
		// Process the item
		switch (WorkItem->Type) {
			case WorkItemIo:
				WorkItem->Srb->SrbStatus = ProcessIoWorkItem(PortExtension, WorkItem);
				StorPortNotification(RequestComplete, PortExtension, WorkItem->Srb);
				break;
				
			case WorkItemInitialize:
				if (!InitializeDrive(PortExtension, WorkItem->DriveContext, WorkItem->DeviceID)) {
					KdPrint((__FUNCTION__": Device initialization failed\n"));
				}
				break;
				
			case WorkItemRemove: {
				RemoveDrive(PortExtension, WorkItem->DriveContext);
				break;
			}
		}
		
		ExFreePoolWithTag(WorkItem, GHOST_PORT_TAG);
	}
}
