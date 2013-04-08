import ghost
import hpfeeds
import json
import logging
import sys
from time import sleep
import socket
import _winreg

# General parameters
LOGFILE = None

# Ghost parameters
GHOST_INTERVAL = 300
GHOST_DURATION = 40
GHOST_DEVICE_ID = 0

# HPfeeds parameters
HPFEEDS_HOST = 'your_broker'
HPFEEDS_PORT = 10000
HPFEEDS_IDENT = 'your_ident'
HPFEEDS_SECRET = 'your_secret'
HPFEEDS_REPORT_CHANNEL = 'ghost.reports'
HPFEEDS_STATUS_CHANNEL = 'ghost.status'


if LOGFILE is not None:
	logging.basicConfig(filename = LOGFILE, level = logging.INFO)
else:
	logging.basicConfig(stream = sys.stderr, level = logging.INFO)

logger = logging.getLogger('ghostwatch')

def create_status_update(action):
	status_update = {'action': action, 'hostname': socket.gethostname()}
	return json.dumps(status_update)
	
def activate_readonly():
	try:
		key = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, r'SYSTEM\CurrentControlSet\services\ghostreadonly\Parameters', 0, _winreg.KEY_SET_VALUE)
		_winreg.SetValueEx(key, 'BlockWriteToRemovable', 0, _winreg.REG_DWORD, 1)
		_winreg.CloseKey(key)
	except WindowsError:
		logger.info('Ghostreadonly not installed or broken')

def onincident(details):
	logger.warning('Detection! PID %d, TID %d' % (details['PID'], details['TID']))
	activate_readonly()
	wire_report = dict(details)
	wire_report['Ident'] = HPFEEDS_IDENT
	incident_hpc = hpfeeds.new(HPFEEDS_HOST, HPFEEDS_PORT, HPFEEDS_IDENT, HPFEEDS_SECRET)
	incident_hpc.publish(HPFEEDS_REPORT_CHANNEL, json.dumps(wire_report))
	incident_hpc.publish(HPFEEDS_STATUS_CHANNEL, create_status_update('detection'))
	incident_hpc.close()

def main():
	logger.info('Initializing Ghost...')
	g = ghost.Ghost(GHOST_DEVICE_ID)
	g.settimeout(GHOST_DURATION)
	logger.info('Ready')
	while True:
		logger.info('Mounting the virtual device')
		
		hpc = hpfeeds.new(HPFEEDS_HOST, HPFEEDS_PORT, HPFEEDS_IDENT, HPFEEDS_SECRET)
		hpc.publish(HPFEEDS_STATUS_CHANNEL, create_status_update('mount'))
		hpc.close()
		
		g.run(onincident)
		logger.info('Virtual device removed')
		
		hpc = hpfeeds.new(HPFEEDS_HOST, HPFEEDS_PORT, HPFEEDS_IDENT, HPFEEDS_SECRET)
		hpc.publish(HPFEEDS_STATUS_CHANNEL, create_status_update('remove'))
		hpc.close()
		
		sleep(GHOST_INTERVAL)

# Loop
if __name__ == '__main__':		
	try:
		main()
	except KeyboardInterrupt:
		logger.info('Exiting after keyboard interrupt')
		sys.exit(0)
	