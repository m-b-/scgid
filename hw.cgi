#!/bin/sh

# Use three temporary files:
#	- /tmp/hw.$$		HTML output of this script
#	- /tmp/hw.vars.$$	Variables sent by SCGI
#	- /tmp/hw.in.$$		Content sent by SCGI

# tbody of HTML table
mkenvtable() {
	awk '
	BEGIN {
		print "<table><thead><tr>"
		print "<th>Environment variable</th>"
		print "<th>Value</th>"
		print "</tr></thead><tbody>"
	}
	{
		i=index($0, "=")
		print "<tr><td>", substr($0,0,i-1), "</td>"
		print "<td>", substr($0,i+1,length($0)-i) "</td></tr>"
	}
	END {
		print "</tbody></table>"
	} ' /tmp/hw.vars.$$
}

# return CONTENT_LENGTH
getconn() {
	while read varval ; do
		if [ "$varval" == "EOF" ]; then
			break;
		fi
		echo $varval >> /tmp/hw.vars.$$
	done
	awk -F'=' '$1 == "CONTENT_LENGTH" { print $2; }' /tmp/hw.vars.$$
}

while true; do
	CONTENT_LENGTH=`getconn`

	dd count=$CONTENT_LENGTH bs=1 >/tmp/hw.in.$$ 2>/dev/null </dev/stdin

	# Write body to temp file first to gets its size.
	cat > /tmp/hw.$$ <<EOF
<!DOCTYPE>
<html>
	<head>
		<meta charset="utf-8">
		<title>Hello, world!</title>
		<style type="text/css">
			table {
				background-color:	#fafafa;
				border-bottom:		solid 1px #778899;
				border-top:			solid 1px #4586ab;
				border:				none;
				margin:				auto;
				padding:			0em;
			}
			tr {
				margin:		0em;
				padding:	0em;
			}
			td {
				padding:		0em 0.75em;
				vertical-align:	middle;
			}
			tr + tr td {
				border-top:		dashed 1px #778899;
			}
			td + td {
				border-left:	dashed 1px #778899;
			}
			hr {
				width:	70%;
			}
		</style>
	</head>
	<body>
		<h1>GET</h1>
		<p>Here are SCGI variables sent by the webserver:</p>
		$(mkenvtable)
		<hr />
		<h1>POST</h1>
		<form action="$REQUEST_URI" method="post">
			<input name="data" placeholder="Enter some data" />
			<input type="submit" value="Send!" />
		</form>
		<p>Previous data:</p>
		<code><pre>$(cat /tmp/hw.in.$$)</pre></code>
	</body>
</html>
EOF

	# Get body size
	clen=`wc -c /tmp/hw.$$ | awk '{ print $1 }'`

	# Now we can send both header and body.
	sed 's/$/\r/g' <<EOF
HTTP/1.1 200 OK
Content-length: $clen
Content-Type: text/html
Location: $REQUEST_URI

EOF

	sed 's/&/\&amp;/g' /tmp/hw.$$

	# Delete temporary files.
	rm /tmp/hw.$$ /tmp/hw.in.$$ /tmp/hw.vars.$$
done
