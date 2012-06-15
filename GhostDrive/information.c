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
	
	// If the code is not called at passive level, we might execute in any process's context
	if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
		KdPrint(("CollectProcessData called at IRQL > passive\n"));
		return NULL;
	}
	
	// "System" is not a real process...
	if (PsGetCurrentProcessId() == (HANDLE) 4) {
		KdPrint(("CollectProcessData called by SYSTEM\n"));
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

	KdPrint(("Process ID: %d\n", ProcessInfo->ProcessId));
	KdPrint(("Thread ID: %d\n", ProcessInfo->ThreadId));
	
	// Iterate through the list and copy the interesting information
	ModuleInfo = (PLDR_DATA_TABLE_ENTRY) LoadedModules->Flink;
	while ((PVOID) ModuleInfo != (PVOID) LoadedModules) {
		PGHOST_INFO_STRING_LIST List;
		PCHAR Buffer;
		USHORT BufferLength;
		
		// Copy the DLL's name
		BufferLength = ModuleInfo->FullDllName.Length + 1;
		List = ExAllocatePoolWithTag(PagedPool, sizeof(GHOST_INFO_STRING_LIST), TAG);
		Buffer = ExAllocatePoolWithTag(PagedPool, BufferLength, TAG);
		Buffer[BufferLength - 1] = '\0';
		RtlInitEmptyUnicodeString(&List->String, (PWCHAR) Buffer, BufferLength);
		RtlCopyUnicodeString(&List->String, &ModuleInfo->FullDllName);
		
		// Append it to the list
		List->Next = ProcessInfo->ModuleNames;
		ProcessInfo->ModuleNames = List;
		
		//KdPrint(("Base address: %p\n", ModuleInfo->ImageBase));
		//KdPrint(("Entry point: %p\n", ModuleInfo->EntryPoint));
		//KdPrint(("Full name: %ws\n", List->String.Buffer));
		//KdPrint(("Base name: %ws\n", ModuleInfo->BaseDllName.Buffer));
		
		// Move to the next DLL
		ModuleInfo = (PLDR_DATA_TABLE_ENTRY) ModuleInfo->Links.Flink;
	}
	
	/*// Allocate local memory
	KdPrint(("%d strings with %d bytes in total\n", StringCount, ByteLength));
	ByteLength += sizeof(GHOST_DRIVE_PROCESS_INFO) + (StringCount - 1) * sizeof(UNICODE_STRING);
	ProcessInfo = ExAllocatePoolWithTag(PagedPool, ByteLength, TAG);
	if (ProcessInfo == NULL) {
		KdPrint(("Memory allocation failed\n"));
		return;
	}
	
	// Second iteration: copy data to local memory
	ProcessInfo->ProcessId = PsGetCurrentProcessId();
	ProcessInfo->ThreadId = PsGetCurrentThreadId();
	ProcessInfo->StringCount = StringCount;
	ProcessInfo->ByteLength = ByteLength;
	Buffer = ((PCHAR) ProcessInfo->ModuleNames) + StringCount * sizeof(UNICODE_STRING);
	ModuleInfo = (PLDR_DATA_TABLE_ENTRY) LoadedModules->Flink;
	for (i = 0; i < StringCount; i++) {
		USHORT Length;
		
		Length = ModuleInfo->FullDllName.Length;
		ProcessInfo->ModuleNames[i].Length = Length;
		ProcessInfo->ModuleNames[i].MaximumLength = Length;
		ProcessInfo->ModuleNames[i].Buffer = (PWCH) Buffer;
		RtlCopyUnicodeString(&ProcessInfo->ModuleNames[i], &ModuleInfo->FullDllName);
		ProcessInfo->ModuleNames[i].Buffer[Length] = '\0';
		
		Buffer += Length + 1; // NULL byte
		ModuleInfo = (PLDR_DATA_TABLE_ENTRY) ModuleInfo->Links.Flink;
	}*/
	
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