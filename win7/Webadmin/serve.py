#!/bin/env python

import bottle
import pymongo
import datetime

@bottle.route('/')
@bottle.route('/index.html')
def index():
	machines = db.machines.find()
	rows = []
	categories = []
	for machine in machines:
		if db.reports.find({'Ident': machine['ident']}).count() == 0:
			status = 'Ok'
			categories.append('success')
		else:
			status = 'Detection!'
			categories.append('error')
		rows.append((machine['ident'], machine['hostname'], str(machine['last_seen']), status))
	return bottle.template('index', rows = rows, categories = categories)
	
@bottle.route('/<path:path>')
def general(path):
	return bottle.static_file(path, root = '.')

connection = pymongo.Connection()
db = connection.ghost

bottle.debug(True)
bottle.run(reloader = True, host = '0.0.0.0')
