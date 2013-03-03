import ghost
import hpfeeds
import json
import logging
import sys
from time import sleep

# General parameters
LOGFILE = None

# Ghost parameters
GHOST_INTERVAL = 300
GHOST_DURATION = 40
GHOST_DEVICE_ID = 0

# HPfeeds parameters
HPFEEDS_HOST = '10.0.0.2'
HPFEEDS_PORT = 10000
HPFEEDS_IDENT = 'me'
HPFEEDS_SECRET = 'secret'
HPFEEDS_CHANNEL = 'test'


if LOGFILE is not None:
	logging.basicConfig(filename = LOGFILE, level = logging.INFO)
else:
	logging.basicConfig(stream = sys.stderr, level = logging.INFO)
logger = logging.getLogger('ghostwatch')

def onincident(details):
	logger.warning('Detection! PID %d, TID %d' % (details['PID'], details['TID']))
	hpc = hpfeeds.new(HPFEEDS_HOST, HPFEEDS_PORT, HPFEEDS_IDENT, HPFEEDS_SECRET)
	hpc.publish(HPFEEDS_CHANNEL, json.dumps(details))
	hpc.close()

def main():
	logger.info('Initializing Ghost...')
	g = ghost.Ghost(GHOST_DEVICE_ID)
	g.settimeout(GHOST_DURATION)
	logger.info('Ready')
	while True:
		logger.info('Mounting the virtual device')
		g.run(onincident)
		logger.info('Virtual device removed')
		sleep(GHOST_INTERVAL)
		
try:
	main()
except KeyboardInterrupt:
	logger.info('Exiting after keyboard interrupt')
	sys.exit(0)
	