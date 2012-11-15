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

#include "extensions.h"
#include <initguid.h>
#include "portctl.h"

#define GHOST_MAX_TARGETS 10
#define GHOST_VENDOR_ID "Ghost"
#define GHOST_PRODUCT_ID "GhostPort"
#define GHOST_PRODUCT_REVISION "V0.2"
#define GHOST_BLOCK_SIZE 512


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
	NTSTATUS status;
	PGHOST_PORT_EXTENSION PortExtension = DeviceExtension;

	KdPrint((__FUNCTION__": Setting adapter properties\n"));
	
	//
	// Set the virtual adapter's properties
	//
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
	
	//
	// Register the device interface
	//
	KdPrint((__FUNCTION__": Registering device interface\n"));
	status = IoRegisterDeviceInterface(LowerDevice, &GUID_DEVINTERFACE_GHOST, NULL, &PortExtension->DeviceInterface);
	if (!NT_SUCCESS(status)) {
		KdPrint((__FUNCTION__": Unable to register device interface\n"));
		return SP_RETURN_ERROR;
	}
	
	KdPrint((__FUNCTION__": Device interface is at %wZ\n", &PortExtension->DeviceInterface));
	
	return SP_RETURN_FOUND;
}


/*
 * This routine allows for initialization at IRQL PASSIVE_LEVEL.
 */
BOOLEAN GhostHwStorPassiveInitializeRoutine(
  IN PVOID DeviceExtension
)
{
	PGHOST_PORT_EXTENSION PortExtension = DeviceExtension;
	NTSTATUS status;
	
	KdPrint((__FUNCTION__": Passive initialization\n"));
	
	// Enable the device interface
	status = IoSetDeviceInterfaceState(&PortExtension->DeviceInterface, TRUE);
	if (!NT_SUCCESS(status)) {
		KdPrint((__FUNCTION__": Unable to activate device interface\n"));
		return FALSE;
	}
	
	return TRUE;
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
	
	if (!StorPortEnablePassiveInitialization(DeviceExtension, GhostHwStorPassiveInitializeRoutine)) {
		KdPrint((__FUNCTION__": Passive initialization not possible\n"));
	}
	
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
	PGHOST_PORT_EXTENSION PortExtension = DeviceExtension;

	KdPrint((__FUNCTION__": Freeing resources\n"));
	if (PortExtension->DeviceInterface.Buffer != NULL) {
		RtlFreeUnicodeString(&PortExtension->DeviceInterface);
	}
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
	
	// Find out which function we are supposed to execute
	switch (Srb->Function) {
		// Run an SCSI command on one of our targets
		// (see SCSI Primary Commands - 4 (SPC-4))
		case SRB_FUNCTION_EXECUTE_SCSI: {
			PCDB Cdb = (PCDB)Srb->Cdb;
			
			// We only support a single bus and one logical unit per target
			if (Srb->PathId != 0 || Srb->Lun != 0) {
				KdPrint((__FUNCTION__": Unknown bus or logical unit\n"));
				SrbStatus = SRB_STATUS_NO_DEVICE;
				break;
			}
			
			// Is the device online?
			if (Srb->TargetId != 7) {
				KdPrint((__FUNCTION__": Device is offline\n"));
				SrbStatus = SRB_STATUS_NO_DEVICE;
				break;
			}
			
			// Which command are we supposed to execute on the target?
			switch (Cdb->CDB6GENERIC.OperationCode) {
				// Are we ready?
				case SCSIOP_TEST_UNIT_READY: {	// 0x00
					SrbStatus = SRB_STATUS_SUCCESS;
					break;
				}
				
				// The system queries information about the target
				case SCSIOP_INQUIRY: {	// 0x12
					PINQUIRYDATA Inquiry;
					NTSTATUS status;
					
					if (Cdb->CDB6INQUIRY3.EnableVitalProductData) {
						SrbStatus = SRB_STATUS_ERROR;
						break;
					}
					
					if (Srb->DataTransferLength < INQUIRYDATABUFFERSIZE) {
						SrbStatus = SRB_STATUS_ERROR;
						break;
					}
					
					/*status = StorPortGetSystemAddress(DeviceExtension, Srb, &Inquiry);
					if (!NT_SUCCESS(status)) {
						SrbStatus = SRB_STATUS_ERROR;
						break;
					}*/
					Inquiry = (PINQUIRYDATA)Srb->DataBuffer;
					
					RtlZeroMemory(Inquiry, Srb->DataTransferLength);
					Inquiry->DeviceType = DIRECT_ACCESS_DEVICE;
					Inquiry->DeviceTypeQualifier = DEVICE_CONNECTED;
					Inquiry->RemovableMedia = TRUE;
					Inquiry->Versions = 6;
					Inquiry->ResponseDataFormat = 2;
					//Inquiry->Wide32Bit = TRUE;
					//Inquiry->Synchronous = TRUE;
					Inquiry->CommandQueue = FALSE;
					//Inquiry->LinkedCommands = FALSE;
					Inquiry->AdditionalLength = INQUIRYDATABUFFERSIZE - 5;
					RtlCopyMemory(Inquiry->VendorId, GHOST_VENDOR_ID, strlen(GHOST_VENDOR_ID));
					RtlCopyMemory(Inquiry->ProductId, GHOST_PRODUCT_ID, strlen(GHOST_PRODUCT_ID));
					RtlCopyMemory(Inquiry->ProductRevisionLevel, GHOST_PRODUCT_REVISION, strlen(GHOST_PRODUCT_REVISION));
					
					Srb->DataTransferLength = INQUIRYDATABUFFERSIZE;
					SrbStatus = SRB_STATUS_SUCCESS;
					break;
				}
				
				// We return only minimal information...
				case SCSIOP_MODE_SENSE: {	// 0x1a
					PMODE_PARAMETER_HEADER ModeParamHeader = (PMODE_PARAMETER_HEADER)Srb->DataBuffer;
					ULONG RequiredSize;
					
					KdPrint((__FUNCTION__": Mode sense with control %x and page code %x\n", Cdb->MODE_SENSE.Pc, Cdb->MODE_SENSE.PageCode));
					
					RequiredSize = sizeof(MODE_PARAMETER_HEADER);
					if (Cdb->MODE_SENSE.PageCode == 0) {
						RequiredSize += sizeof(MODE_PARAMETER_BLOCK);
					}
					
					if (Srb->DataTransferLength < RequiredSize) {
						KdPrint((__FUNCTION__": Buffer too small\n"));
						SrbStatus = SRB_STATUS_ERROR;
						break;
					}
					
					RtlZeroMemory(Srb->DataBuffer, Srb->DataTransferLength);
					ModeParamHeader->ModeDataLength = (UCHAR)(RequiredSize - sizeof(ModeParamHeader->ModeDataLength));
					ModeParamHeader->BlockDescriptorLength = (Cdb->MODE_SENSE.PageCode == 0) ? sizeof(MODE_PARAMETER_BLOCK) : 0;
					
					SrbStatus = SRB_STATUS_SUCCESS;
					break;
				}
				
				case SCSIOP_READ_CAPACITY: {	// 0x25
					PREAD_CAPACITY_DATA Capacity;
					ULONG BytesPerBlock = GHOST_BLOCK_SIZE;
					ULONG BlockCount = 200 * 1024 * 1024 / GHOST_BLOCK_SIZE;
					
					KdPrint((__FUNCTION__": Reading capacity\n"));
					
					if (Srb->DataTransferLength < sizeof(READ_CAPACITY_DATA)) {
						SrbStatus = SRB_STATUS_ERROR;
						break;
					}
					
					Capacity = (PREAD_CAPACITY_DATA)Srb->DataBuffer;
					REVERSE_BYTES(&Capacity->LogicalBlockAddress, &BlockCount);
					REVERSE_BYTES(&Capacity->BytesPerBlock, &BytesPerBlock);
					
					SrbStatus = SRB_STATUS_SUCCESS;
					break;
				}
				
				case SCSIOP_READ: {	// 0x28
					ULONG BlockOffset;
					USHORT BlockCount;
					PVOID OutBuffer;
					ULONG ByteLength;
					
					REVERSE_BYTES(&BlockOffset, &Cdb->CDB10.LogicalBlockByte0);
					REVERSE_BYTES_SHORT(&BlockCount, &Cdb->CDB10.TransferBlocksMsb);
					
					KdPrint((__FUNCTION__": Reading %d blocks from offset %d, TODO\n", BlockCount, BlockOffset));
					
					ByteLength = BlockCount * GHOST_BLOCK_SIZE;
					if (Srb->DataTransferLength < ByteLength) {
						KdPrint((__FUNCTION__": Buffer too small\n"));
						SrbStatus = SRB_STATUS_ERROR;
						break;
					}
					
					if (!NT_SUCCESS(StorPortGetSystemAddress(DeviceExtension, Srb, &OutBuffer))) {
						KdPrint((__FUNCTION__": No system address\n"));
						SrbStatus = SRB_STATUS_ERROR;
						break;
					}
					
					RtlZeroMemory(OutBuffer, Srb->DataTransferLength);
					Srb->DataTransferLength = ByteLength;
					SrbStatus = SRB_STATUS_SUCCESS;
					break;
				}
				
				case SCSIOP_WRITE: {	// 0x2a
					KdPrint((__FUNCTION__": Write from PID %d\n", PsGetCurrentProcessId()));
					break;
				}
				
				// We don't support this...
				case SCSIOP_REPORT_LUNS: {	// 0xa0
					break;
				}
				
				default: {
					KdPrint((__FUNCTION__": Unknown SCSI command 0x%x for target 0x%x\n", Cdb->CDB6GENERIC.OperationCode, Srb->TargetId));
					break;
				}
			}
			
			break;
		}
		
		// Process a PnP request
		case SRB_FUNCTION_PNP: {
			PSCSI_PNP_REQUEST_BLOCK Request = (PSCSI_PNP_REQUEST_BLOCK)Srb;
			KdPrint((__FUNCTION__": PNP request 0x%x\n", Request->PnPAction));
			
			// These requests are merely informative, we just complete them
			SrbStatus = SRB_STATUS_SUCCESS;
			break;
		}

		case SRB_FUNCTION_IO_CONTROL: {
			KdPrint((__FUNCTION__": I/O control for target 0x%x\n", Srb->TargetId));
			break;
		}
		
		case SRB_FUNCTION_WMI: {
			KdPrint((__FUNCTION__": WMI request for target 0x%x\n", Srb->TargetId));
			break;
		}
		
		default: {
			KdPrint((__FUNCTION__": Unknown I/O request (0x%x)\n", Srb->Function));
			break;
		}
	}
	
	// Complete the request
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
	
	// What are we supposed to do?
	switch (ControlType) {
		// The system asks for the codes that we support
		case ScsiQuerySupportedControlTypes: {
			PSCSI_SUPPORTED_CONTROL_TYPE_LIST SupportedControl = (PSCSI_SUPPORTED_CONTROL_TYPE_LIST) Parameters;
			ULONG i;
			
			KdPrint((__FUNCTION__": Query supported control types\n"));
		
			// We only support "Stop" and "Restart", which are mandatory
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
	
	InitData.DeviceExtensionSize = sizeof(GHOST_PORT_EXTENSION);
	InitData.SpecificLuExtensionSize = sizeof(GHOST_DRIVE_PDO_CONTEXT);
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

