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

#include <wdm.h>
#include <storport.h>

#define GHOST_MAX_TARGETS 10


/*
 * Forward declarations
 */
DRIVER_INITIALIZE DriverEntry;


/*
 * Storport calls this routine immediately after driver initialization.
 * We have to supply information about the bus.
 */
ULONG GhostVirtualHwStorFindAdapter(
  IN PVOID DeviceExtension,
  IN PVOID HwContext,
  IN PVOID BusInformation,
  IN PVOID LowerDevice,
  IN PCHAR ArgumentString,
  IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
  OUT PBOOLEAN Again
)
{
	KdPrint((__FUNCTION__": Setting adapter properties\n"));
	
	/*
	 * Set the virtual adapter's properties
	 */
	if (ConfigInfo->NumberOfPhysicalBreaks == SP_UNINITIALIZED_VALUE) {
		KdPrint((__FUNCTION__": Number of physical breaks is uninitialized\n"));
		ConfigInfo->NumberOfPhysicalBreaks = 0;
	}
	
	ConfigInfo->NumberOfBuses = 1;
	ConfigInfo->ScatterGather = TRUE;
	ConfigInfo->Master = TRUE;
	ConfigInfo->MapBuffers = STOR_MAP_NON_READ_WRITE_BUFFERS; // RW buffer mapping is only supported starting with Windows 8
	ConfigInfo->MaximumNumberOfTargets = GHOST_MAX_TARGETS;
	
	if (ConfigInfo->Dma64BitAddresses == SCSI_DMA64_SYSTEM_SUPPORTED) {
		KdPrint((__FUNCTION__": Indicating support for 64-bit addresses\n"));
		ConfigInfo->Dma64BitAddresses = SCSI_DMA64_MINIPORT_SUPPORTED;
	}
	
	ConfigInfo->MaximumNumberOfLogicalUnits = 1;
	ConfigInfo->SynchronizationModel = StorSynchronizeFullDuplex;
	ConfigInfo->VirtualDevice = TRUE;
	
	return SP_RETURN_FOUND;
}


/*
 * Storport calls this routine to initialize the hardware after device detection
 * and reset.
 *
 * Caution: DIRQL (probably not for virtual devices, but still...)!
 * (-> PassiveInitialization)
 */
BOOLEAN GhostHwStorInitialize(
  IN PVOID DeviceExtension
)
{
	KdPrint((__FUNCTION__": Initializing the bus\n"));
	
	return TRUE;
}


/*
 * Storport calls this routine to reset the bus.
 *
 * Caution: DISPATCH_LEVEL!
 */
BOOLEAN GhostHwStorResetBus(
  IN PVOID DeviceExtension,
  IN ULONG PathId
)
{
	KdPrint((__FUNCTION__": Resetting the bus\n"));
	
	return TRUE;
}


/*
 * This is the last callback that Storport calls before deleting the device.
 */
VOID GhostHwStorFreeAdapterResources(
  IN PVOID DeviceExtension
)
{
	KdPrint((__FUNCTION__": Freeing resources\n"));
}


/*
 * This is the routine that handles I/O for the bus and all its children.
 *
 * Caution: DISPATCH_LEVEL!
 */
BOOLEAN GhostHwStorStartIo(
  IN PVOID DeviceExtension,
  IN PSCSI_REQUEST_BLOCK Srb
)
{
	KdPrint((__FUNCTION__": I/O request\n"));
	
	Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
	
	return FALSE;
}


/*
 * This routine is called to start or stop the adapter.
 *
 * Caution: DIRQL (probably not for virtual devices, but still...)!
 */
SCSI_ADAPTER_CONTROL_STATUS GhostHwStorAdapterControl(
  IN  PVOID DeviceExtension,
  IN  SCSI_ADAPTER_CONTROL_TYPE ControlType,
  IN  PVOID Parameters
)
{
	KdPrint((__FUNCTION__": Processing control request\n"));
	
	switch (ControlType) {
		
	case ScsiQuerySupportedControlTypes:
	{
		PSCSI_SUPPORTED_CONTROL_TYPE_LIST SupportedControl = (PSCSI_SUPPORTED_CONTROL_TYPE_LIST) Parameters;
		ULONG i;
		
		for (i = 0; i < SupportedControl->MaxControlType - 1; i++) {
			if (i == ScsiStopAdapter || i == ScsiRestartAdapter)
				SupportedControl->SupportedTypeList[i] = TRUE;
		}
		break;
	}
	
	case ScsiStopAdapter:
		KdPrint((__FUNCTION__": Stopping the adapter\n"));
		break;
		
	case ScsiRestartAdapter:
		KdPrint((__FUNCTION__": Restarting the adapter\n"));
		break;
		
	default:
		break;
	}
	
	return ScsiAdapterControlSuccess;
}


NTSTATUS DriverEntry(struct _DRIVER_OBJECT *DriverObject, PUNICODE_STRING RegistryPath) {
	VIRTUAL_HW_INITIALIZATION_DATA InitData;
	NTSTATUS status;
	
	KdPrint((__FUNCTION__": Miniport entry\n"));
	
	/*
	 * Initialize Storport, providing all required callbacks
	 */
	RtlZeroMemory(&InitData, sizeof(VIRTUAL_HW_INITIALIZATION_DATA));
	InitData.HwInitializationDataSize = sizeof(VIRTUAL_HW_INITIALIZATION_DATA);
	
	InitData.AdapterInterfaceType = Internal;
	
	InitData.HwInitialize = GhostHwStorInitialize;
	InitData.HwStartIo = GhostHwStorStartIo;
	InitData.HwFindAdapter = GhostVirtualHwStorFindAdapter;
	InitData.HwResetBus = GhostHwStorResetBus;
	
	InitData.HwAdapterState = NULL;
	
	InitData.DeviceExtensionSize = 0;
	InitData.SpecificLuExtensionSize = 0;
	InitData.SrbExtensionSize = 0;
	
	InitData.TaggedQueuing = TRUE;
	InitData.AutoRequestSense = TRUE;
	InitData.MultipleRequestPerLu = TRUE;
	InitData.ReceiveEvent = TRUE;
	
	InitData.HwAdapterControl = GhostHwStorAdapterControl;
	InitData.HwFreeAdapterResources = GhostHwStorFreeAdapterResources;
	
	status = StorPortInitialize(DriverObject, RegistryPath, (PHW_INITIALIZATION_DATA) &InitData, NULL);
	if (status != STATUS_SUCCESS) {
		KdPrint((__FUNCTION__": StorPortInitialize failed: 0x%lx\n", status));
	}
	
	return status;
}

