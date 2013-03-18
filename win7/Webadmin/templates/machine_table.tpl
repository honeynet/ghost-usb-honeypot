<thead>
    <tr>
        <th>Machine ID</th>
		<th>Hostname</th>
        <th>Last update (UTC)</th>
        <th>Status</th>
		<th>Actions</th>
    </tr>
</thead>
<tbody>
	%for i in range(len(rows)):
	<tr class="{{categories[i]}}">
		%for col in rows[i]:
		<td>{{col}}</td>
		%end
		%if rows[i][0] in reports:
		<td><a class="btn btn-primary" data-remote="/report/{{rows[i][0]}}" data-target="#modalreport" data-toggle="modal">More info</a> <a href="#" class="btn" onclick="dismiss('{{reports[rows[i][0]]['_id']}}');">Dismiss</a></td>
		%else:
		<td>n/a</td>
		%end
	</tr>
	%end
</tbody>