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
				if ghostlib.GhostGetLastError() != 0:
					print "Error: " + ghostgeterrordescription(ghostlib.GhostGetLastError())
				break
			details = dict()
			details["PID"] = ghostlib.GhostGetProcessID(self._deviceid, incident)
			details["TID"] = ghostlib.GhostGetThreadID(self._deviceid, incident)
			details["Modules"] = []
			nummodules = ghostlib.GhostGetNumModules(self._deviceid, incident)
			print nummodules, "modules"
			name = create_unicode_buffer('\000' * 1024)
			print ghostlib.GhostGetModuleName(self._deviceid, incident, 0, name, 1024)
			print "broken?"
			print name.value
			#for i in range(nummodules):
			#	name = create_string_buffer('\000' * 1024)
			#	if ghostlib.GhostGetModuleName(self._deviceid, incident, i, name, 1024) > 0:
			#		details["Modules"].append(str(name))
			self.onincident(details)
			incident += 1
		
	def onincident(self, details):
		print details["PID"], details["TID"]
		#for name in details["Modules"]:
		#	print '\t', name

# Load the DLL
ghostlib = windll.LoadLibrary(r"..\bin\i386\GhostLib.dll")
ghostgeterrordescription = ghostlib.GhostGetErrorDescription
ghostgeterrordescription.restype = c_char_p
kernel32 = windll.LoadLibrary("kernel32.dll")

if __name__ == "__main__":
	print "Test..."
	g = Ghost(0)
	g.run()
