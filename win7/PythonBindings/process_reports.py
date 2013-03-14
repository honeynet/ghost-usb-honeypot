#!/bin/env python

import hpfeeds
import sys
import pymongo
import json
import datetime

DB_HOST = 'localhost'
DB_PORT = 27017
HPFEEDS_HOST   = 'hpfriends.honeycloud.net'
HPFEEDS_PORT   = 20000
HPFEEDS_IDENT  = 'n8AY3Kuw'
HPFEEDS_SECRET = 'zEzb23Ta8QhJBaC9'
HPFEEDS_REPORT_CHANNEL = 'ghost.reports'
HPFEEDS_STATUS_CHANNEL = 'ghost.status'

def on_message(ident, chan, payload):
	if chan == HPFEEDS_STATUS_CHANNEL:
		print('Status update from {}'.format(str(ident)))
		status_update = json.loads(str(payload))
		# Get the status document from the DB
		status = db.machines.find_one({'ident': str(ident)})
		if status is None:
			# Create a new status page
			status = {'ident': str(ident)}
		# The hostname may change, so update it here
		status['hostname'] = status_update['hostname']
		status['last_seen'] = datetime.datetime.utcnow()
		db.machines.save(status)
	elif chan == HPFEEDS_REPORT_CHANNEL:
		print('Incoming report from {}'.format(str(ident)))
		# Write to MongoDB
		report = json.loads(str(payload))
		report['Ident'] = str(ident)
		report['Timestamp'] = datetime.datetime.utcnow()
		report['Dismissed'] = False
		db.reports.insert(report)

def on_error(payload):
	print('HPfeeds error')
	hpc.stop()
	sys.exit(-1)

def main():
	hpc = hpfeeds.new(HPFEEDS_HOST, HPFEEDS_PORT, HPFEEDS_IDENT, HPFEEDS_SECRET)
	hpc.subscribe([HPFEEDS_REPORT_CHANNEL, HPFEEDS_STATUS_CHANNEL])
	hpc.run(on_message, on_error)

# Establish the global database connection
connection = pymongo.Connection(DB_HOST, DB_PORT)
db = connection.ghost

if __name__ == '__main__':
	# Run the loop
	try:
		main()
	except KeyboardInterrupt:
		sys.exit(0)

