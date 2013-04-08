#!/bin/env python

import bottle
import pymongo
from pymongo.objectid import ObjectId
import datetime

@bottle.route('/')
def index():
	return bottle.static_file('/index.html', root = '.')

@bottle.route('/machinetable')
def machinetable():
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
	return bottle.template('templates/machine_table', rows = rows, categories = categories, reports = reports)
	
@bottle.route('/dismiss/<obj_id>')
def dismiss(obj_id):
	report = db.reports.find_one({'_id': ObjectId(obj_id)})
	report['Dismissed'] = True
	db.reports.save(report)
	
@bottle.route('/report/<machine_ident>')
def report(machine_ident):
	detections = db.reports.find({'Ident': machine_ident, 'Dismissed': False}).sort('Timestamp')
	if detections.count() > 0:
		return bottle.template('templates/report', res = detections[0])
	else:
		return None
	
@bottle.route('/<path:path>')
def general(path):
	return bottle.static_file(path, root = '.')

connection = pymongo.Connection()
db = connection.ghost

bottle.debug(True)
bottle.run(reloader = True, host = '0.0.0.0')
