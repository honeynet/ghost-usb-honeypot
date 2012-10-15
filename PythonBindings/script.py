from ctypes import *
import time
import threading

def callback(deviceID, incidentID, context):
	print deviceID, incidentID
	print ghostlib.GhostGetProcessID(deviceID, incidentID)
	
ghostCallback = WINFUNCTYPE(None, c_int, c_int, c_void_p)

ghostlib = windll.LoadLibrary("..\Release XP\GhostLib.dll")
print ghostlib

cb = ghostCallback(callback)
param = ghostlib.GhostMountDeviceWithIDNoThread(0, cb, None)
info = threading.Thread(target = ghostlib.InfoThread, args = (param, ))
info.start()

raw_input()
ghostlib.GhostUmountDevice(0)
