function loadmachines() {
	$("#machinetable").load('/machinetable');
}

function dismiss(report) {
	$.get('/dismiss/' + report, loadmachines);
}

$(document).ready(function() {
	loadmachines();
	setInterval(loadmachines, 1000);
});
