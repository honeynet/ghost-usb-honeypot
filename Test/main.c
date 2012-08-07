
#include "ghostlib.h"

#include <stdio.h>
#include <Windows.h>


void Callback(int DeviceID, int IncidentID, void *Context) {
	printf("Process ID: %d\n", GhostGetProcessID(DeviceID, IncidentID));
	printf("Thread ID: %d\n", GhostGetThreadID(DeviceID, IncidentID));
}


int __cdecl main(int argc, char *argv[]) {
	int ID;
	
	printf("Mount\n");
	ID = GhostMountDevice(Callback, NULL);
	if (ID < 0) {
		printf("Error: %s\n", GhostGetErrorDescription(GhostGetLastError()));
		return -1;
	}
	
	Sleep(10 * 1000);
	
	printf("Umount\n");
	if (GhostUmountDevice(ID) < 0) {
		printf("Error: %s\n", GhostGetErrorDescription(GhostGetLastError()));
		return -1;
	}
	
	return 0;
}