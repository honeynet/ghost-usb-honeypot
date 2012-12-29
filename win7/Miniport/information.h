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


#ifndef INFORMATION_H
#define INFORMATION_H

#include <ntddk.h>

#define GHOST_MAX_PROCESS_INFO 32


typedef struct _GHOST_INFO_STRING_LIST {
	UNICODE_STRING String;
	struct _GHOST_INFO_STRING_LIST *Next;
} GHOST_INFO_STRING_LIST, *PGHOST_INFO_STRING_LIST;


typedef struct _GHOST_INFO_PROCESS_DATA {
	HANDLE ProcessId;
	HANDLE ThreadId;
	USHORT ModuleNamesCount;
	PGHOST_INFO_STRING_LIST ModuleNames;
	LIST_ENTRY ListNode;
	DWORD ProcessImageBase;
} GHOST_INFO_PROCESS_DATA, *PGHOST_INFO_PROCESS_DATA;


PGHOST_INFO_PROCESS_DATA GhostInfoCollectProcessData();
void GhostInfoFreeProcessData(PGHOST_INFO_PROCESS_DATA ProcessInfo);
SIZE_T GhostInfoGetProcessDataBufferSize(PGHOST_INFO_PROCESS_DATA ProcessInfo);
BOOLEAN GhostInfoStoreProcessDataInBuffer(PGHOST_INFO_PROCESS_DATA ProcessInfo, PVOID Buffer, SIZE_T BufferSize);


/*
 * Some internal data structures that we extract information from.
 */

typedef struct _LDR_DATA_TABLE_ENTRY {
	LIST_ENTRY Links;
	PVOID SomePointers[2];
	PVOID ImageBase;
	PVOID EntryPoint;
	PVOID Something;
	UNICODE_STRING FullDllName;
	UNICODE_STRING BaseDllName;
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

typedef struct _PEB_LDR_DATA {
	CHAR Something[8];
	PVOID SomethingElse[3];
	LIST_ENTRY LoadedModules;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

struct _PEB {
	CHAR Something[4];
	PVOID SomethingElse[2];
	PPEB_LDR_DATA LoaderData;
};


/*
 * Some undocumented routines that we need to collect information.
 */

char *PsGetProcessImageFileName(PEPROCESS Process);
PPEB PsGetProcessPeb(PEPROCESS Process);


#endif
