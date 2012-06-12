/*
 * Ghost - A honeypot for USB malware
 * Copyright (C) 2011 to 2012  Sebastian Poeplau (sebastian.poeplau@gmail.com)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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


/*
 * Collect information about the calling process, including process and thread IDs
 * and a list of loaded modules.
 */
void GhostInfoCollectProcessData(WDFREQUEST Request) {
	PEPROCESS Process;
	PPEB Peb;
	PLDR_DATA_TABLE_ENTRY ModuleInfo;
	PLIST_ENTRY LoadedModules;
	
	// If the code is not called at passive level, we might execute in any process's context
	if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
		KdPrint(("CollectProcessData called at IRQL > passive\n"));
		return;
	}
	
	// "System" is not a real process...
	if (PsGetCurrentProcessId() == (HANDLE) 4) {
		KdPrint(("CollectProcessData called by SYSTEM\n"));
		return;
	}
	
	KdPrint(("Process ID: %d\n", PsGetCurrentProcessId()));
	KdPrint(("Thread ID: %d\n", PsGetCurrentThreadId()));
	
	Process = PsGetCurrentProcess();
	KdPrint(("%s\n", PsGetProcessImageFileName(Process)));
	
	// Iterate through the list of loaded modules
	Peb = PsGetProcessPeb(Process);
	LoadedModules = &Peb->LoaderData->LoadedModules;
	ModuleInfo = (PLDR_DATA_TABLE_ENTRY) LoadedModules->Flink;
	while ((PVOID) ModuleInfo != (PVOID) LoadedModules) {
		KdPrint(("Base address: %p\n", ModuleInfo->ImageBase));
		KdPrint(("Entry point: %p\n", ModuleInfo->EntryPoint));
		KdPrint(("Full name: %ws\n", ModuleInfo->FullDllName.Buffer));
		KdPrint(("Base name: %ws\n", ModuleInfo->BaseDllName.Buffer));
		
		ModuleInfo = (PLDR_DATA_TABLE_ENTRY) ModuleInfo->Links.Flink;
	}
}