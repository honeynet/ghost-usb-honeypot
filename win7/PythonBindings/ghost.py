from ctypes import *
import time
import threading

class Ghost:
	def __init__(self, deviceid):
		self._deviceid = deviceid
		self._timeout = 20
	
	def umounttimer(self):
		time.sleep(self._timeout)
		print "Unloading..."
		ghostlib.GhostUmountDevice(self._deviceid)
	
	def run(self):
		print "Mounting..."
		ghostlib.GhostMountDeviceWithID(self._deviceid, None, None, "c:\gd0.img")
		threading.Thread(target = self.umounttimer).start()
		print "Waiting for incidents..."
		incident = 0
		while True:
			if ghostlib.GhostWaitForIncident(self._deviceid, incident) < 0:
				print "Error: " + ghostgeterrordescription(ghostlib.GhostGetLastError())
				break
			details = dict()
			details["PID"] = ghostlib.GhostGetProcessID(self._deviceid, incident)
			self.onincident(details)
			incident += 1
		
	def onincident(self, details):
		print details["PID"]

# Load the DLL
ghostlib = windll.LoadLibrary(r"..\bin\i386\GhostLib.dll")
ghostgeterrordescription = ghostlib.GhostGetErrorDescription
ghostgeterrordescription.restype = c_char_p

if __name__ == "__main__":
	print "Test..."
	g = Ghost(0)
	g.run()
