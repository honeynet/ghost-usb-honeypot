#!/bin/env python

import bottle
import pymongo
from pymongo.objectid import ObjectId
import datetime

@bottle.route('/')
@bottle.route('/index.html')
def index():
	machines = db.machines.find()
	rows = []
	categories = []
	reports = {}
	for machine in machines:
		detections = db.reports.find({'Ident': machine['ident'], 'Dismissed': False}).sort('Timestamp')
		if detections.count() == 0:
			status = 'Ok'
			categories.append('success')
		else:
			status = 'Detection ({} reports)'.format(detections.count())
			categories.append('error')
			reports[machine['ident']] = detections[0]
		rows.append((machine['ident'], machine['hostname'], str(machine['last_seen']), status))
	return bottle.template('index', rows = rows, categories = categories, reports = reports)
	
@bottle.route('/dismiss/<obj_id>')
def dismiss(obj_id):
	db.reports.remove({'_id': ObjectId(obj_id)})
	
@bottle.route('/<path:path>')
def general(path):
	return bottle.static_file(path, root = '.')

connection = pymongo.Connection()
db = connection.ghost

bottle.debug(True)
bottle.run(reloader = True, host = '0.0.0.0')
