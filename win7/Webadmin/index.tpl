<!DOCTYPE html>
<!--[if lt IE 7]>      <html class="no-js lt-ie9 lt-ie8 lt-ie7"> <![endif]-->
<!--[if IE 7]>         <html class="no-js lt-ie9 lt-ie8"> <![endif]-->
<!--[if IE 8]>         <html class="no-js lt-ie9"> <![endif]-->
<!--[if gt IE 8]><!--> <html class="no-js"> <!--<![endif]-->
    <head>
        <meta charset="utf-8">
        <meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
        <title>Ghost status overview</title>
        <meta name="description" content="">
        <meta name="viewport" content="width=device-width">

        <link rel="stylesheet" href="css/bootstrap.min.css">
        <style>
            body {
                padding-top: 60px;
                padding-bottom: 40px;
            }
        </style>
        <link rel="stylesheet" href="css/bootstrap-responsive.min.css">
        <link rel="stylesheet" href="css/main.css">

        <script src="js/vendor/modernizr-2.6.2-respond-1.1.0.min.js"></script>
    </head>
    <body>
        <!--[if lt IE 7]>
            <p class="chromeframe">You are using an <strong>outdated</strong> browser. Please <a href="http://browsehappy.com/">upgrade your browser</a> or <a href="http://www.google.com/chromeframe/?redirect=true">activate Google Chrome Frame</a> to improve your experience.</p>
        <![endif]-->

        <!-- This code is taken from http://twitter.github.com/bootstrap/examples/hero.html -->

        <div class="navbar navbar-inverse navbar-fixed-top">
            <div class="navbar-inner">
                <div class="container">
                    <a class="btn btn-navbar" data-toggle="collapse" data-target=".nav-collapse">
                        <span class="icon-bar"></span>
                        <span class="icon-bar"></span>
                        <span class="icon-bar"></span>
                    </a>
                    <a class="brand" href="https://code.google.com/p/ghost-usb-honeypot/">Ghost</a>
                    <div class="nav-collapse collapse">
                        <ul class="nav">
                        	<li class="active"><a href="/">Machines</a></li>
                            <li><a href="#">Server</a></li>
                        </ul>
                    </div><!--/.nav-collapse -->
                </div>
            </div>
        </div>

        <div class="container">

            <h1>Machines</h1>

            <p>The following table shows a list of machines that are running Ghost and have recently reported back to the server.</p>

            <table class="table">
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
						<td><a href="#modal{{i}}" class="btn btn-primary" data-toggle="modal">More info</a> <a href="#" class="btn" onclick="dismiss('{{reports[rows[i][0]]['_id']}}');">Dismiss</a></td>
						<div id="modal{{i}}" class="modal hide fade" tabindex="-1">
							<div class="modal-header">
								<button class="close" data-dismiss="modal">&times;</button>
								<h3>Detection report</h3>
							</div>
							<div class="modal-body">
								%res = reports[rows[i][0]]
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
							</div>
						</div>
						%else:
						<td>n/a</td>
						%end
					</tr>
					%end
                </tbody>
            </table>

            <hr>

            <footer>
                <p>&copy; Sebastian Poeplau 2013</p>
            </footer>

        </div> <!-- /container -->

        <script src="//ajax.googleapis.com/ajax/libs/jquery/1.9.1/jquery.min.js"></script>
        <script>window.jQuery || document.write('<script src="js/vendor/jquery-1.9.1.min.js"><\/script>')</script>

        <script src="js/vendor/bootstrap.min.js"></script>

        <script src="js/main.js"></script>
    </body>
</html>
