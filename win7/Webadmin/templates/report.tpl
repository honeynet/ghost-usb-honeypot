<dl>
	<dt>Timestamp</dt><dd>{{str(res['Timestamp'])}}</dd>
	<dt>PID</dt><dd>{{res['PID']}}</dd>
	<dt>TID</dt><dd>{{res['TID']}}</dd>
	%if 'Modules' in res and len(res['Modules']) > 0:
	<dt>Infecting executable</dt><dd>{{res['Modules'][0]}}</dd>
	<dt>Loaded modules</dt><dd>
	%for mod_index in range(len(res['Modules'])):
	{{res['Modules'][mod_index]}}<br>
	%end
	</dd>
	%end
</dl>
