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
#include <ntddscsi.h>

#include "extensions.h"
#include "file_io.h"
#include "io_worker.h"
#include <initguid.h>
#include "portctl.h"

#define GHOST_VENDOR_ID "Ghost"
#define GHOST_PRODUCT_ID "GhostPort"
#define GHOST_PRODUCT_REVISION "V0.2"


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
	ULONG StorStatus;

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
	HANDLE IoThreadHandle;
	
	KdPrint((__FUNCTION__": Passive initialization\n"));
	
	// Start the I/O thread
	status = PsCreateSystemThread(&IoThreadHandle, GENERIC_ALL, NULL, NULL, NULL, IoWorkerThread, PortExtension);
	if (!NT_SUCCESS(status)) {
		KdPrint((__FUNCTION__": I/O worker thread creation failed (0x%lx)\n", status));
		return FALSE;
	}
	
	// Store the thread object
	status = ObReferenceObjectByHandle(IoThreadHandle, THREAD_ALL_ACCESS, *PsThreadType, KernelMode, &PortExtension->IoThread, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint((__FUNCTION__": Unable to obtain the worker thread object (0x%lx)\n", status));
		KeSetEvent(&PortExtension->IoThreadTerminate, IO_NO_INCREMENT, FALSE);
		// Hope that the thread terminates before the driver is unloaded...
		return FALSE;
	}
	
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
	int i;
	PGHOST_PORT_EXTENSION PortExtension = DeviceExtension;
	
	KdPrint((__FUNCTION__": Initializing the bus\n"));
	
	// Initialize the device extension
	for (i = 0; i < GHOST_MAX_TARGETS; i++) {
		PortExtension->DriveStates[i] = GhostDriveDisabled;
		PortExtension->MountParameters[i] = NULL;
	}
	
	KeInitializeEvent(&PortExtension->IoWorkAvailable, SynchronizationEvent, FALSE);
	KeInitializeEvent(&PortExtension->IoThreadTerminate, SynchronizationEvent, FALSE);
	KeInitializeSpinLock(&PortExtension->IoWorkItemsLock);
	InitializeListHead(&PortExtension->IoWorkItems);
	
	// Enable further initialization at PASSIVE_LEVEL
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
	PGHOST_PORT_EXTENSION PortExtension = DeviceExtension;
	UCHAR SrbStatus = SRB_STATUS_INVALID_REQUEST;
	
	// Find out which function we are supposed to execute
	switch (Srb->Function) {
		// Run an SCSI command on one of our targets
		// (see SCSI Primary Commands - 4 (SPC-4))
		case SRB_FUNCTION_EXECUTE_SCSI: {
			PCDB Cdb = (PCDB)Srb->Cdb;
			PGHOST_DRIVE_PDO_CONTEXT Context;
			
			// Is the device valid?
			Context = StorPortGetLogicalUnit(DeviceExtension, Srb->PathId, Srb->TargetId, Srb->Lun);
			if (Context == NULL) {
				KdPrint((__FUNCTION__": Unknown bus or logical unit\n"));
				SrbStatus = SRB_STATUS_NO_DEVICE;
				break;
			}
			
			// Initialize the device if necessary
			if (PortExtension->DriveStates[Srb->TargetId] == GhostDriveInitRequired) {
				//
				// The device is not yet fully initialized. Unfortunately, we can't initialize it
				// at the current IRQL, so we dispatch to the I/O worker thread. Meanwhile, we complete
				// all requests with status "pending".
				//
				PIO_WORK_ITEM WorkItem;
				
				WorkItem = ExAllocatePoolWithTag(NonPagedPool, sizeof(IO_WORK_ITEM), GHOST_PORT_TAG);
				WorkItem->Type = WorkItemInitialize;
				WorkItem->Srb = NULL;
				WorkItem->DriveContext = Context;
				WorkItem->DeviceID = Srb->TargetId;
				EnqueueWorkItem(PortExtension, WorkItem);
				
				PortExtension->DriveStates[Srb->TargetId] = GhostDriveInitInProgress;
			}
			
			// Is the device online?
			if (PortExtension->DriveStates[Srb->TargetId] == GhostDriveDisabled) {
				KdPrint((__FUNCTION__": Device %d is offline\n", Srb->TargetId));
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
					ULONG MaxBlock;
					
					KdPrint((__FUNCTION__": Reading capacity\n"));
					
					if (Srb->DataTransferLength < sizeof(READ_CAPACITY_DATA)) {
						SrbStatus = SRB_STATUS_ERROR;
						break;
					}
					
					if (PortExtension->DriveStates[Srb->TargetId] != GhostDriveEnabled) {
						KdPrint((__FUNCTION__": Device not ready\n"));
						SrbStatus = SRB_STATUS_PENDING;
						break;
					}
					
					Capacity = (PREAD_CAPACITY_DATA)Srb->DataBuffer;
					//
					// Caution: The command queries the block address of the last block,
					// which is one less than the total number of blocks!
					//
					MaxBlock = (ULONG)Context->ImageSize.QuadPart / GHOST_BLOCK_SIZE - 1;
					REVERSE_BYTES(&Capacity->LogicalBlockAddress, &MaxBlock);
					REVERSE_BYTES(&Capacity->BytesPerBlock, &BytesPerBlock);
					
					SrbStatus = SRB_STATUS_SUCCESS;
					break;
				}
				
				case SCSIOP_READ:	// 0x28
				case SCSIOP_WRITE: {	// 0x2a
					PIO_WORK_ITEM WorkItem;
					
					//
					// We can't do file I/O at the current IRQL, so pass the SRB on to the I/O thread
					//
					KdPrint((__FUNCTION__": Enqueuing work item\n"));
					WorkItem = ExAllocatePoolWithTag(NonPagedPool, sizeof(IO_WORK_ITEM), GHOST_PORT_TAG);
					WorkItem->Type = WorkItemIo;
					WorkItem->Srb = Srb;
					WorkItem->DriveContext = Context;
					EnqueueWorkItem(PortExtension, WorkItem);
					return TRUE;
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
			
			switch (Request->PnPAction) {
				case StorQueryCapabilities: {
					PSTOR_DEVICE_CAPABILITIES Capabilities = (PSTOR_DEVICE_CAPABILITIES)Request->DataBuffer;
					
					if (Request->DataTransferLength < sizeof(STOR_DEVICE_CAPABILITIES)) {
						KdPrint((__FUNCTION__": Buffer too small\n"));
						SrbStatus = SRB_STATUS_ERROR;
						break;
					}
					
					Capabilities->LockSupported = FALSE;
					Capabilities->Removable = TRUE;
					Capabilities->EjectSupported = FALSE;
					
					SrbStatus = SRB_STATUS_SUCCESS;
					break;
				}
				
				default:
					SrbStatus = SRB_STATUS_SUCCESS;
			}
			
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
	PGHOST_PORT_EXTENSION PortExtension = (PGHOST_PORT_EXTENSION)DeviceExtension;
	
	// What are we supposed to do?
	switch (ControlType) {
		// The system asks for the codes that we support
		case ScsiQuerySupportedControlTypes: {
			PSCSI_SUPPORTED_CONTROL_TYPE_LIST SupportedControl = (PSCSI_SUPPORTED_CONTROL_TYPE_LIST) Parameters;
			ULONG i;
			
			KdPrint((__FUNCTION__": Query supported control types\n"));
		
			// We only support "Stop" and "Restart", which are mandatory
			for (i = 0; i < SupportedControl->MaxControlType - 1; i++) {
				if (i == ScsiStopAdapter)// || i == ScsiRestartAdapter)
					SupportedControl->SupportedTypeList[i] = TRUE;
			}
			break;
		}
	
		case ScsiStopAdapter:
			KdPrint((__FUNCTION__": Stopping the adapter\n"));
			
			// Terminate the I/O thread
			KeSetEvent(&PortExtension->IoThreadTerminate, IO_NO_INCREMENT, FALSE);
			if (!NT_SUCCESS(KeWaitForSingleObject(PortExtension->IoThread, Executive, KernelMode, FALSE, NULL))) {
				KdPrint((__FUNCTION__": Unable to terminate the I/O thread\n"));
			}
			KdPrint((__FUNCTION__": Terminated\n"));

			ObDereferenceObject(PortExtension->IoThread);
			
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
	PIO_STACK_LOCATION StackLocation;
	PREQUEST_PARAMETERS Parameters;
	PGHOST_PORT_EXTENSION PortExtension = DeviceExtension;
	
	KdPrint((__FUNCTION__": Processing service request\n"));
	
	// Obtain data from the IRP
	StackLocation = IoGetCurrentIrpStackLocation(ServiceRequest);
	if (StackLocation == NULL) {
		KdPrint((__FUNCTION__": Unable to obtain I/O stack location\n"));
		ServiceRequest->IoStatus.Status = STATUS_INTERNAL_ERROR;
		goto Completion;
	}
	
	// Check the function codes - we only respond to device control/service request
	if (StackLocation->MajorFunction != IRP_MJ_DEVICE_CONTROL) {
		KdPrint((__FUNCTION__": No device control code\n"));
		ServiceRequest->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
		goto Completion;
	}
	
	if (StackLocation->Parameters.DeviceIoControl.IoControlCode != IOCTL_MINIPORT_PROCESS_SERVICE_IRP) {
		KdPrint((__FUNCTION__": No miniport service request\n"));
		ServiceRequest->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
		goto Completion;
	}
	
	KdPrint((__FUNCTION__": %d bytes in, %d bytes out\n", StackLocation->Parameters.DeviceIoControl.InputBufferLength,
		StackLocation->Parameters.DeviceIoControl.OutputBufferLength));
	
	// Exctract the request parameters
	if (StackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(REQUEST_PARAMETERS)) {
		KdPrint((__FUNCTION__": Not enough input data\n"));
		ServiceRequest->IoStatus.Status = STATUS_UNSUCCESSFUL;
		goto Completion;
	}
	
	Parameters = ServiceRequest->AssociatedIrp.SystemBuffer;
	
	// Check the magic number
	if (Parameters->MagicNumber != GHOST_MAGIC_NUMBER) {
		KdPrint((__FUNCTION__": Magic number incorrect (0x%x)\n", Parameters->MagicNumber));
		ServiceRequest->IoStatus.Status = STATUS_UNSUCCESSFUL;
		goto Completion;
	}
	
	// Perform the requested action
	switch (Parameters->Opcode) {
		case OpcodeEnable: {
			PREQUEST_PARAMETERS ParamCopy;
			
			KdPrint((__FUNCTION__": Enabling a device\n"));
			
			// Make sure that the particular device is offline
			if (PortExtension->DriveStates[Parameters->DeviceID] != GhostDriveDisabled) {
				KdPrint((__FUNCTION__": Already enabled\n"));
				ServiceRequest->IoStatus.Status = STATUS_UNSUCCESSFUL;
				goto Completion;
			}

			// Check the size of the data structure
			if ((Parameters->ImageNameLength * sizeof(WCHAR) + sizeof(REQUEST_PARAMETERS) - sizeof(WCHAR))
				> StackLocation->Parameters.DeviceIoControl.InputBufferLength)
			{
				KdPrint((__FUNCTION__": Image name length incorrect\n"));
				ServiceRequest->IoStatus.Status = STATUS_UNSUCCESSFUL;
				goto Completion;
			}
			
			// Copy the parameters to a temporary location
			ParamCopy = ExAllocatePoolWithTag(NonPagedPool, StackLocation->Parameters.DeviceIoControl.InputBufferLength, GHOST_PORT_TAG);
			RtlCopyMemory(ParamCopy, ServiceRequest->AssociatedIrp.SystemBuffer, StackLocation->Parameters.DeviceIoControl.InputBufferLength);
			PortExtension->MountParameters[Parameters->DeviceID] = ParamCopy;
			
			// Set the device online
			PortExtension->DriveStates[Parameters->DeviceID] = GhostDriveInitRequired;
			StorPortNotification(BusChangeDetected, DeviceExtension, 0);
			
			//
			// We can't fully initialize here, because the drive context is not yet available. So we only set the device's
			// state to "InitRequired" and do the actual initialization in HwStorStartIo, where we have the context.
			//
			
			break;
		}
		
		case OpcodeDisable: {
			PGHOST_DRIVE_PDO_CONTEXT Context;
			
			KdPrint((__FUNCTION__": Disabling a device\n"));
			
			// Get the device extension
			Context = StorPortGetLogicalUnit(DeviceExtension, 0, Parameters->DeviceID, 0);
			if (Context == NULL) {
				KdPrint((__FUNCTION__": Invalid device ID (%d)\n", Parameters->DeviceID));
				ServiceRequest->IoStatus.Status = STATUS_UNSUCCESSFUL;
				goto Completion;
			}
			
			// Make sure that the device is online
			if (PortExtension->DriveStates[Parameters->DeviceID] != GhostDriveEnabled) {
				KdPrint((__FUNCTION__": Device is not enabled\n"));
				ServiceRequest->IoStatus.Status = STATUS_UNSUCCESSFUL;
				goto Completion;
			}
			
			status = GhostFileIoUmountImage(Context);
			if (!NT_SUCCESS(status)) {
				KdPrint((__FUNCTION__": Umount failed (0x%lx)\n", status));
				ServiceRequest->IoStatus.Status = STATUS_INTERNAL_ERROR;
				goto Completion;
			}
			
			// Set the device offline
			PortExtension->DriveStates[Parameters->DeviceID] = GhostDriveDisabled;
			StorPortNotification(BusChangeDetected, DeviceExtension, 0);
			break;
		}
		
		default: {
			KdPrint((__FUNCTION__": Invalid opcode\n"));
			ServiceRequest->IoStatus.Status = STATUS_UNSUCCESSFUL;
			break;
		}
	}
		
	ServiceRequest->IoStatus.Status = STATUS_SUCCESS;
	
Completion:
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

