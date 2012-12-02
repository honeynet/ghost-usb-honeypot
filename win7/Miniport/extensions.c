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

#include "extensions.h"


VOID InitGhostDrivePdoContext(PGHOST_DRIVE_PDO_CONTEXT Context, ULONG DeviceID) {
	RtlZeroMemory(Context, sizeof(GHOST_DRIVE_PDO_CONTEXT));
	Context->ImageMounted = FALSE;
	Context->ImageWritten = FALSE;
	Context->ImageSize.QuadPart = 0;
	Context->ID = DeviceID;
	Context->WriterInfoCount = 0;
	InitializeListHead(&Context->WriterInfoList);
	KeInitializeSpinLock(&Context->WriterInfoListLock);
	InitializeListHead(&Context->WriterInfoRequests);
	KeInitializeSpinLock(&Context->WriterInfoRequestsLock);
}


VOID DeleteGhostDrivePdoContext(PGHOST_PORT_EXTENSION PortExtension, PGHOST_DRIVE_PDO_CONTEXT Context) {
	KLOCK_QUEUE_HANDLE LockHandle;
	PLIST_ENTRY CurrentNode;

	// Delete all process info structs
	KeAcquireInStackQueuedSpinLock(&Context->WriterInfoListLock, &LockHandle);
	CurrentNode = RemoveHeadList(&Context->WriterInfoList);
	while (CurrentNode != &Context->WriterInfoList) {
		GhostInfoFreeProcessData(CONTAINING_RECORD(CurrentNode, GHOST_INFO_PROCESS_DATA, ListNode));
		CurrentNode = RemoveHeadList(&Context->WriterInfoList);
	}
	KeReleaseInStackQueuedSpinLock(&LockHandle);
	
	// Cancel all outstanding writer info requests
	KeAcquireInStackQueuedSpinLock(&Context->WriterInfoRequestsLock, &LockHandle);
	CurrentNode = RemoveHeadList(&Context->WriterInfoRequests);
	while (CurrentNode != &Context->WriterInfoRequests) {
		PWRITER_INFO_REQUEST WriterInfoRequest = CONTAINING_RECORD(CurrentNode, WRITER_INFO_REQUEST, ListEntry);
		PIRP Request = WriterInfoRequest->Irp;
		
		Request->IoStatus.Status = STATUS_CANCELLED;
		StorPortCompleteServiceIrp(PortExtension, Request);
		
		CurrentNode = RemoveHeadList(&Context->WriterInfoRequests);
	}
	KeReleaseInStackQueuedSpinLock(&LockHandle);
	
	RtlZeroMemory(Context, sizeof(GHOST_DRIVE_PDO_CONTEXT));
	return;
}


VOID EnqueueWorkItem(PGHOST_PORT_EXTENSION PortExtension, PIO_WORK_ITEM WorkItem) {
	ExInterlockedInsertTailList(&PortExtension->IoWorkItems, &WorkItem->ListNode, &PortExtension->IoWorkItemsLock);
	KeSetEvent(&PortExtension->IoWorkAvailable, IO_NO_INCREMENT, FALSE);
}


VOID InitGhostPortExtension(PGHOST_PORT_EXTENSION PortExtension) {
	int i;
	
	for (i = 0; i < GHOST_MAX_TARGETS; i++) {
		PortExtension->DriveStates[i] = GhostDriveDisabled;
		PortExtension->MountParameters[i] = NULL;
	}
	
	KeInitializeEvent(&PortExtension->IoWorkAvailable, SynchronizationEvent, FALSE);
	KeInitializeEvent(&PortExtension->IoThreadTerminate, SynchronizationEvent, FALSE);
	KeInitializeSpinLock(&PortExtension->IoWorkItemsLock);
	KeInitializeSpinLock(&PortExtension->DriveStatesLock);
	InitializeListHead(&PortExtension->IoWorkItems);
}


VOID SetDriveState(PGHOST_PORT_EXTENSION PortExtension, ULONG DeviceID, GhostDriveState State, BOOLEAN LockAcquired) {
	KLOCK_QUEUE_HANDLE LockHandle;
	
	if (!LockAcquired) {
		KeAcquireInStackQueuedSpinLock(&PortExtension->DriveStatesLock, &LockHandle);
	}
	
	PortExtension->DriveStates[DeviceID] = State;
	
	if (!LockAcquired) {
		KeReleaseInStackQueuedSpinLock(&LockHandle);
	}
}


GhostDriveState GetDriveState(PGHOST_PORT_EXTENSION PortExtension, ULONG DeviceID, BOOLEAN LockAcquired) {
	KLOCK_QUEUE_HANDLE LockHandle;
	GhostDriveState State;
	
	if (!LockAcquired) {
		KeAcquireInStackQueuedSpinLock(&PortExtension->DriveStatesLock, &LockHandle);
	}
	
	State = PortExtension->DriveStates[DeviceID];
	
	if (!LockAcquired) {
		KeReleaseInStackQueuedSpinLock(&LockHandle);
	}
	
	return State;
}


BOOLEAN IsProcessKnown(PGHOST_DRIVE_PDO_CONTEXT Context, HANDLE ProcessID) {
	KLOCK_QUEUE_HANDLE LockHandle;
	PLIST_ENTRY CurrentNode;
	PGHOST_INFO_PROCESS_DATA ProcessInfo;
	BOOLEAN Known = FALSE;
	
	KeAcquireInStackQueuedSpinLock(&Context->WriterInfoListLock, &LockHandle);
	
	// Iterate through all our process info structs
	// Move backward, because recent writers are likely to write again
	CurrentNode = Context->WriterInfoList.Blink;
	while (CurrentNode != &Context->WriterInfoList) {
		ProcessInfo = CONTAINING_RECORD(CurrentNode, GHOST_INFO_PROCESS_DATA, ListNode);
		if (ProcessInfo->ProcessId == ProcessID) {
			Known = TRUE;
			break;
		}
		CurrentNode = CurrentNode->Blink;
	}
	
	KeReleaseInStackQueuedSpinLock(&LockHandle);
	return Known;
}


VOID AddProcessInfo(PGHOST_PORT_EXTENSION PortExtension, PGHOST_DRIVE_PDO_CONTEXT Context, PGHOST_INFO_PROCESS_DATA ProcessInfo) {
	KLOCK_QUEUE_HANDLE LockHandle;
	USHORT NewIndex;
	PLIST_ENTRY CurrentNode;
	
	KeAcquireInStackQueuedSpinLock(&Context->WriterInfoListLock, &LockHandle);
	InsertTailList(&Context->WriterInfoList, &ProcessInfo->ListNode);
	Context->WriterInfoCount++;
	NewIndex = Context->WriterInfoCount - 1;
	KeReleaseInStackQueuedSpinLock(&LockHandle);
	
	// Complete any outstanding writer info requests
	KeAcquireInStackQueuedSpinLock(&Context->WriterInfoRequestsLock, &LockHandle);
	CurrentNode = Context->WriterInfoRequests.Flink;
	while (CurrentNode != &Context->WriterInfoRequests) {
		PWRITER_INFO_REQUEST WriterInfoRequest = CONTAINING_RECORD(CurrentNode, WRITER_INFO_REQUEST, ListEntry);
		
		// If this is a request for the new entry, then handle it and move to the next list entry
		if (WriterInfoRequest->RequestedIndex == NewIndex) {
			PIO_WORK_ITEM WorkItem;
			
			RemoveEntryList(CurrentNode);
			CurrentNode = CurrentNode->Flink;
			
			WorkItem = ExAllocatePoolWithTag(NonPagedPool, sizeof(IO_WORK_ITEM), GHOST_PORT_TAG);
			WorkItem->Type = WorkItemInfoRequest;
			WorkItem->DriveContext = Context;
			WorkItem->WriterInfoData.WriterInfoRequest = WriterInfoRequest;
			WorkItem->WriterInfoData.ProcessInfo = ProcessInfo;
			EnqueueWorkItem(PortExtension, WorkItem);
		}
		else {
			CurrentNode = CurrentNode->Flink;
		}
	}
	KeReleaseInStackQueuedSpinLock(&LockHandle);
}


PGHOST_INFO_PROCESS_DATA GetProcessInfo(PGHOST_DRIVE_PDO_CONTEXT Context, USHORT Index) {
	KLOCK_QUEUE_HANDLE LockHandle;
	PLIST_ENTRY CurrentNode;
	USHORT i;
	
	KeAcquireInStackQueuedSpinLock(&Context->WriterInfoListLock, &LockHandle);
	
	// Check the index
	if (Index >= Context->WriterInfoCount) {
		KeReleaseInStackQueuedSpinLock(&LockHandle);
		return NULL;
	}
	
	// Obtain the requested entry
	CurrentNode = Context->WriterInfoList.Flink;
	for (i = 0; i < Index; i++) {
		CurrentNode = CurrentNode->Flink;
	}
	
	KeReleaseInStackQueuedSpinLock(&LockHandle);
	return CONTAINING_RECORD(CurrentNode, GHOST_INFO_PROCESS_DATA, ListNode);
}


VOID AddWriterInfoRequest(PGHOST_DRIVE_PDO_CONTEXT Context, PIRP Request) {
	KLOCK_QUEUE_HANDLE LockHandle;
	PWRITER_INFO_REQUEST WriterInfoRequest;
	
	WriterInfoRequest = ExAllocatePoolWithTag(NonPagedPool, sizeof(WRITER_INFO_REQUEST), GHOST_PORT_TAG);
	WriterInfoRequest->Irp = Request;
	WriterInfoRequest->RequestedIndex = ((PREQUEST_PARAMETERS)Request->AssociatedIrp.SystemBuffer)->WriterInfoParameters.WriterIndex;
	
	KeAcquireInStackQueuedSpinLock(&Context->WriterInfoRequestsLock, &LockHandle);
	InsertTailList(&Context->WriterInfoRequests, &WriterInfoRequest->ListEntry);
	KeReleaseInStackQueuedSpinLock(&LockHandle);
}


VOID ProcessWriterInfoRequest(PIRP ServiceRequest, PGHOST_INFO_PROCESS_DATA ProcessInfo) {
	SIZE_T RequiredSize;
	ULONG OutBufferSize;
	PIO_STACK_LOCATION StackLocation = IoGetCurrentIrpStackLocation(ServiceRequest);

	// Determine the required buffer size
	RequiredSize = GhostInfoGetProcessDataBufferSize(ProcessInfo);
	OutBufferSize = StackLocation->Parameters.DeviceIoControl.OutputBufferLength;
	if (OutBufferSize < RequiredSize) {
		// Buffer too small - if there's space for a SIZE_T, then return the required size
		if (OutBufferSize >= sizeof(SIZE_T)) {
			*(PSIZE_T)ServiceRequest->AssociatedIrp.SystemBuffer = RequiredSize;
			ServiceRequest->IoStatus.Information = sizeof(SIZE_T);
			ServiceRequest->IoStatus.Status = STATUS_SUCCESS;
		}
		else {
			ServiceRequest->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
		}
		
		return;
	}
	
	// The buffer is ok, copy data
	if (!GhostInfoStoreProcessDataInBuffer(ProcessInfo, ServiceRequest->AssociatedIrp.SystemBuffer, OutBufferSize)) {
		KdPrint((__FUNCTION__": Unable to copy process info to the output buffer\n"));
		ServiceRequest->IoStatus.Status = STATUS_INTERNAL_ERROR;
		return;
	}
	
	ServiceRequest->IoStatus.Information = RequiredSize;
	ServiceRequest->IoStatus.Status = STATUS_SUCCESS;
}
