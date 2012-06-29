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

#include "information.h"
#include "ghostdrive_io.h"
#include "ghostdrive.h"


/*
 * Collect information about the calling process, including process and thread IDs
 * and a list of loaded modules.
 */
PGHOST_INFO_PROCESS_DATA GhostInfoCollectProcessData(WDFREQUEST Request) {
	PEPROCESS Process;
	PPEB Peb;
	PLDR_DATA_TABLE_ENTRY ModuleInfo;
	PLIST_ENTRY LoadedModules;
	PGHOST_INFO_PROCESS_DATA ProcessInfo;
	PGHOST_INFO_STRING_LIST LastEntry = NULL;
	
	// If the code is not called at passive level, we might execute in any process's context
	if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
		KdPrint(("CollectProcessData called at IRQL > passive\n"));
		return NULL;
	}
	
	// "System" is not a real process...
	if (PsGetCurrentProcessId() == (HANDLE) 4) {
		KdPrint(("CollectProcessData called by SYSTEM\n"));
		KdPrint(("Thread ID: %d\n", PsGetCurrentThreadId()));
		return NULL;
	}
	
	// Find the list of loaded modules
	Process = PsGetCurrentProcess();
	Peb = PsGetProcessPeb(Process);
	LoadedModules = &Peb->LoaderData->LoadedModules;
	
	// Allocate memory to store the information
	ProcessInfo = ExAllocatePoolWithTag(PagedPool, sizeof(GHOST_INFO_PROCESS_DATA), TAG);
	ProcessInfo->ProcessId = PsGetCurrentProcessId();
	ProcessInfo->ThreadId = PsGetCurrentThreadId();
	ProcessInfo->ModuleNames = NULL;
	ProcessInfo->ModuleNamesCount = 0;

	KdPrint(("Process ID: %d\n", ProcessInfo->ProcessId));
	KdPrint(("Thread ID: %d\n", ProcessInfo->ThreadId));
	
	// Iterate through the list and copy the interesting information
	ModuleInfo = (PLDR_DATA_TABLE_ENTRY) LoadedModules->Flink;
	while ((PVOID) ModuleInfo != (PVOID) LoadedModules) {
		PGHOST_INFO_STRING_LIST List;
		PCHAR Buffer;
		USHORT BufferLength;
		
		// Copy the DLL's name
		BufferLength = ModuleInfo->FullDllName.Length + 2;
		List = ExAllocatePoolWithTag(PagedPool, sizeof(GHOST_INFO_STRING_LIST), TAG);
		Buffer = ExAllocatePoolWithTag(PagedPool, BufferLength, TAG);
		Buffer[BufferLength - 2] = L'\0';
		RtlInitEmptyUnicodeString(&List->String, (PWCHAR) Buffer, BufferLength);
		RtlCopyUnicodeString(&List->String, &ModuleInfo->FullDllName);
		
		// Append it to the list
		List->Next = NULL;
		if (LastEntry != NULL) {
			LastEntry->Next = List;
			LastEntry = List;
		}
		else {
			ProcessInfo->ModuleNames = List;
			LastEntry = List;
		}
		ProcessInfo->ModuleNamesCount++;
		
		//KdPrint(("Base address: %p\n", ModuleInfo->ImageBase));
		//KdPrint(("Entry point: %p\n", ModuleInfo->EntryPoint));
		//KdPrint(("Full name: %ws\n", List->String.Buffer));
		//KdPrint(("Base name: %ws\n", ModuleInfo->BaseDllName.Buffer));
		
		// Move to the next DLL
		ModuleInfo = (PLDR_DATA_TABLE_ENTRY) ModuleInfo->Links.Flink;
	}
	
	return ProcessInfo;
}


void GhostInfoFreeProcessData(PGHOST_INFO_PROCESS_DATA ProcessInfo) {
	PGHOST_INFO_STRING_LIST List;
	
	while (ProcessInfo->ModuleNames != NULL) {
		List = ProcessInfo->ModuleNames;
		ProcessInfo->ModuleNames = List->Next;
		ExFreePoolWithTag(List, TAG);
	}
	
	ExFreePoolWithTag(ProcessInfo, TAG);
}


SIZE_T GhostInfoGetProcessDataBufferSize(PGHOST_INFO_PROCESS_DATA ProcessInfo) {
	SIZE_T BufferSize = 0;
	PGHOST_INFO_STRING_LIST ListEntry;
	
	BufferSize += sizeof(GHOST_DRIVE_WRITER_INFO_RESPONSE);
	ListEntry = ProcessInfo->ModuleNames;
	if (ListEntry != NULL) {
		BufferSize -= sizeof(SIZE_T); // will be added during the loop
	}
	while (ListEntry != NULL) {
		BufferSize += sizeof(SIZE_T) + ListEntry->String.Length + 2; // +2 for the wide null character at the end
		ListEntry = ListEntry->Next;
	}
	
	return BufferSize;
}


BOOLEAN GhostInfoStoreProcessDataInBuffer(PGHOST_INFO_PROCESS_DATA ProcessInfo, PVOID Buffer, SIZE_T BufferSize) {
	SIZE_T StringOffset;
	PCHAR StringBuffer;
	PGHOST_DRIVE_WRITER_INFO_RESPONSE Response;
	PGHOST_INFO_STRING_LIST ListEntry;
	int i;
	
	if (BufferSize < GhostInfoGetProcessDataBufferSize(ProcessInfo)) {
		KdPrint(("Buffer too small\n"));
		return FALSE;
	}
	
	// Translate the main struct
	Response = Buffer;
	Response->ProcessId = ProcessInfo->ProcessId;
	Response->ThreadId = ProcessInfo->ThreadId;
	Response->ModuleNamesCount = ProcessInfo->ModuleNamesCount;
	
	// Copy the module names
	StringOffset = sizeof(GHOST_DRIVE_WRITER_INFO_RESPONSE) - sizeof(SIZE_T) + ProcessInfo->ModuleNamesCount * sizeof(SIZE_T);
	ListEntry = ProcessInfo->ModuleNames;
	for (i = 0; i < ProcessInfo->ModuleNamesCount; i++) {
		if (ListEntry == NULL) {
			// This does not happen if ModuleNamesCount is correct
			// Still include it as a safeguard
			break;
		}
		
		StringBuffer = ((PCHAR) Buffer) + StringOffset;
		RtlCopyMemory(StringBuffer, ListEntry->String.Buffer, ListEntry->String.Length);
		StringBuffer[ListEntry->String.Length] = L'\0';
		Response->ModuleNameOffsets[i] = StringOffset;
		
		StringOffset += ListEntry->String.Length + 2;
		ListEntry = ListEntry->Next;
	}
	
	return TRUE;
}