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
	//Context->WriterInfoCount = 0;
	InitializeListHead(&Context->WriterInfoList);
	KeInitializeSpinLock(&Context->WriterInfoListLock);
}


VOID DeleteGhostDrivePdoContext(PGHOST_DRIVE_PDO_CONTEXT Context) {
	// TODO: Delete all process info structs
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
