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

#include <ntddk.h>
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
	UNREFERENCED_PARAMETER(Again);
	UNREFERENCED_PARAMETER(ArgumentString);
	UNREFERENCED_PARAMETER(LowerDevice);
	UNREFERENCED_PARAMETER(BusInformation);
	UNREFERENCED_PARAMETER(HwContext);
	UNREFERENCED_PARAMETER(DeviceExtension);

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
	UNREFERENCED_PARAMETER(DeviceExtension);

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
	UNREFERENCED_PARAMETER(DeviceExtension);
	UNREFERENCED_PARAMETER(PathId);

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
	UNREFERENCED_PARAMETER(DeviceExtension);

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
	UCHAR SrbStatus = SRB_STATUS_INVALID_REQUEST;
	UNREFERENCED_PARAMETER(DeviceExtension);

	KdPrint((__FUNCTION__": I/O request (0x%x)\n", Srb->Function));
	
	switch (Srb->Function) {
		case SRB_FUNCTION_EXECUTE_SCSI: {
			PCDB Cdb = (PCDB)Srb->Cdb;
			KdPrint((__FUNCTION__": Executing SCSI command 0x%x\n", Cdb->CDB6GENERIC.OperationCode));
			break;
		}
		
		case SRB_FUNCTION_PNP: {
			PSCSI_PNP_REQUEST_BLOCK Request = (PSCSI_PNP_REQUEST_BLOCK)Srb;
			KdPrint((__FUNCTION__": PNP request 0x%x\n", Request->PnPAction));
			
			switch (Request->PnPAction) {
				case StorRemoveDevice:
					SrbStatus = SRB_STATUS_SUCCESS;
					break;
			}
			
			break;
		}
	}
	
	Srb->SrbStatus = SrbStatus;
	StorPortNotification(RequestComplete, DeviceExtension, Srb);
	return TRUE;
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
	UNREFERENCED_PARAMETER(DeviceExtension);

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
		KdPrint((__FUNCTION__": Other control request\n"));
		break;
	}
	
	return ScsiAdapterControlSuccess;
}


/*
 * This routine processes service requests to the miniport,
 * i.e. device control requests with control code IOCTL_MINIPORT_PROCESS_SERVICE_IRP.
 */
VOID GhostHwStorProcessServiceRequest(
  IN PVOID DeviceExtension,
  IN PVOID Irp
)
{
	NTSTATUS status;
	PIRP ServiceRequest = (PIRP)Irp;
	
	KdPrint((__FUNCTION__": Processing service request\n"));
	ServiceRequest->IoStatus.Status = STATUS_SUCCESS;
	
	status = StorPortCompleteServiceIrp(DeviceExtension, ServiceRequest);
	if (!NT_SUCCESS(status)) {
		KdPrint((__FUNCTION__": Service request completion failed with status 0x%lx\n", status));
		return;
	}
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
	InitData.HwProcessServiceRequest = GhostHwStorProcessServiceRequest;
	
	status = StorPortInitialize(DriverObject, RegistryPath, (PHW_INITIALIZATION_DATA) &InitData, NULL);
	if (status != STATUS_SUCCESS) {
		KdPrint((__FUNCTION__": StorPortInitialize failed: 0x%lx\n", status));
	}
	
	return status;
}

