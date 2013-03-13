import ghost
import hpfeeds
import json
import logging
import sys
from time import sleep
import socket

# General parameters
LOGFILE = None

# Ghost parameters
GHOST_INTERVAL = 300
GHOST_DURATION = 40
GHOST_DEVICE_ID = 0

# HPfeeds parameters
HPFEEDS_HOST = 'hpfriends.honeycloud.net'
HPFEEDS_PORT = 20000
HPFEEDS_IDENT = 'wWLnJ949'
HPFEEDS_SECRET = 'yjniQJ6WzZybYy6q'
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

def onincident(details):
	logger.warning('Detection! PID %d, TID %d' % (details['PID'], details['TID']))
	wire_report = dict(details)
	wire_report['Ident'] = HPFEEDS_IDENT
	hpc.publish(HPFEEDS_REPORT_CHANNEL, json.dumps(wire_report))
	hpc.publish(HPFEEDS_STATUS_CHANNEL, create_status_update('detection'))

def main():
	logger.info('Initializing Ghost...')
	g = ghost.Ghost(GHOST_DEVICE_ID)
	g.settimeout(GHOST_DURATION)
	logger.info('Ready')
	while True:
		logger.info('Mounting the virtual device')
		hpc.publish(HPFEEDS_STATUS_CHANNEL, create_status_update('mount'))
		g.run(onincident)
		logger.info('Virtual device removed')
		hpc.publish(HPFEEDS_STATUS_CHANNEL, create_status_update('remove'))
		sleep(GHOST_INTERVAL)

# Module setup
hpc = hpfeeds.new(HPFEEDS_HOST, HPFEEDS_PORT, HPFEEDS_IDENT, HPFEEDS_SECRET)

# Loop
if __name__ == '__main__':		
	try:
		main()
	except KeyboardInterrupt:
		logger.info('Exiting after keyboard interrupt')
		sys.exit(0)
	