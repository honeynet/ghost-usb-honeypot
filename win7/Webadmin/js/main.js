function dismiss(report) {
	$.get('/dismiss/' + report, function() {
		location.reload();
	});
}
