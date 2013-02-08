#ifndef PORTCTL_H
#define PORTCTL_H

#define GHOST_MAGIC_NUMBER 0xFEEDABCD
#define GHOST_MAX_TARGETS 10


typedef enum {
	OpcodeEnable,
	OpcodeDisable,
	OpcodeGetWriterInfo
} GHOST_PORT_OPCODE;


typedef struct _REQUEST_PARAMETERS {
	ULONG MagicNumber;	// always set to GHOST_MAGIC_NUMBER
	GHOST_PORT_OPCODE Opcode;
	UCHAR DeviceID;
	union {
		struct {
			USHORT WriterIndex;
			BOOLEAN BlockingCall;
		} WriterInfoParameters;	// required for OpcodeGetWriterInfo
		
		struct {
			LARGE_INTEGER ImageSize;	// in bytes
			USHORT ImageNameLength;	// in characters
			WCHAR ImageName[1];
		} MountInfo;	// required for OpcodeEnable
	};
} REQUEST_PARAMETERS, *PREQUEST_PARAMETERS;


/*
 * This struct is returned by the writer info request, if the buffer is large enough.
 */
typedef struct _GHOST_DRIVE_WRITER_INFO_RESPONSE {
	
	HANDLE ProcessId;
	HANDLE ThreadId;
	ULONG ProcessImageBase;
	USHORT ModuleNamesCount;
	SIZE_T ModuleNameOffsets[1];
	
} GHOST_DRIVE_WRITER_INFO_RESPONSE, *PGHOST_DRIVE_WRITER_INFO_RESPONSE;

#ifdef DEFINE_GUID
// {171721E5-8A83-4a8b-AE95-2466F6429466}
DEFINE_GUID(GUID_DEVINTERFACE_GHOST, 0x171721e5, 0x8a83, 0x4a8b, 0xae, 0x95, 0x24, 0x66, 0xf6, 0x42, 0x94, 0x66);
#endif

#endif
