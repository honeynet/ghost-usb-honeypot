#ifndef PORTCTL_H
#define PORTCTL_H

typedef enum {
	OpcodeEnable,
	OpcodeDisable
} GHOST_PORT_OPCODE;

typedef struct _REQUEST_PARAMETERS {
	GHOST_PORT_OPCODE Opcode;
	ULONG DeviceID;
} REQUEST_PARAMETERS, *PREQUEST_PARAMETERS;

#endif
