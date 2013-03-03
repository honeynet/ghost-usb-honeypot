from ctypes import *
import time
import threading

class Ghost:
	"""
	This class allows you to control Ghost with Python and retrieve results in Python data structures.
	In order to use it, just override onincident() to do something useful.
	"""
	
	def __init__(self, deviceid):
		if deviceid < 0 or deviceid > 9:
			raise ValueError("Device ID must be between 0 and 9")
		self._deviceid = deviceid
		self._imagefile = "c:\gd%d.img" % deviceid
		self._timeout = 20
	
	def _umounttimer(self):
		time.sleep(self._timeout)
		#print "Unloading..."
		ghostlib.GhostUmountDevice(self._deviceid)
		
	def settimeout(self, seconds):
		"""
		Set the timeout in seconds after which the device is removed from the system.
		"""
		if seconds > 0:
			self._timeout = seconds
			
	def setimagefile(self, filename):
		"""
		Specify the location of the image file. Must be an absolute path, but the file doesn't have to exist.
		"""
		self._imagefile = filename
	
	def run(self, onincident):
		"""
		Mount the virtual device, wait for a certain time and remove the device. In case of write requests,
		onincident(details) is called.
		"""
		#print "Mounting..."
		ghostlib.GhostMountDeviceWithID(self._deviceid, None, None, self._imagefile)
		threading.Thread(target = self._umounttimer).start()
		#print "Waiting for incidents..."
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
			for i in range(nummodules):
				name = create_unicode_buffer('\000' * 1024)
				if ghostlib.GhostGetModuleName(self._deviceid, incident, i, name, 1024) > 0:
					details["Modules"].append(name.value)
			onincident(details)
			incident += 1
			

# Load the DLL
ghostlib = windll.LoadLibrary("GhostLib.dll")
ghostgeterrordescription = ghostlib.GhostGetErrorDescription
ghostgeterrordescription.restype = c_char_p
kernel32 = windll.LoadLibrary("kernel32.dll")

if __name__ == "__main__":
	def onincident(self, details):
		print "Write access from PID %d, TID %d" % (details["PID"], details["TID"])
		if len(details["Modules"]) > 0:
			print "\t", details["Modules"][0]
			
	print "Test..."
	g = Ghost(0)
	g.run(onincident)
