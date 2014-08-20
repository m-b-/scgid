# Description
The associated code is an Simple CGI dæmon implementation
for Unix. Protocol description can be find in scgi.txt[1].
Default port is 9000.

	(earth)% cc scgid.c -o scgid # -pedantic -W -Wall -g -DDBG
	(earth)% ./scgid -h
	scgid [-p port] cmd
	scgid -h
	(earth)% ./scgid ./hw.cgi
	Listening on 127.0.0.1:9000.

The DBG compilation flag:

- outputs SCGI variables;
- outputs informations on incoming connection;
- set SO_REUSEADDR flag on socket;

# Protocol variation
We relax the protocol on both the position of the
CONTENT_LENGTH variable and the value of the SCGI.
We require SCGI headers to fit in at most BUFSIZ bytes
(8192 on a modern 32bits Linux distribution).

Protocol does not define communication between SCGI
dæmon and web program. We use the following convention:

- scgid writes list of SCGI variables `NAME=value\n` on program's
  stdin followed by the string `EOF\n`, and finally the content
  of the SCGI request (of length CONTENT_LENGTH).
- scgid stops sending data from program to webserver upon reception
  of `</html>` and subsequently close the socket.

# Deployment example
The hw.cgi shell script demonstrates basic behavior.

The following configuration file may be used for
Nginx:

	location ~ \.cgi {
		include		scgi_params;
		scgi_pass	127.0.0.1:9000;
	}

See [2] for more on Nginx's SCGI module.

---

[1]: http://www.python.ca/scgi/protocol.txt
[2]: http://nginx.org/en/docs/http/ngx_http_scgi_module.html
