#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * Launch the processus described by cmd.
 * Connect its stdin/stdout to in/out.
 */
int
launchproc(int in, int out, char **cmd)
{
	switch (fork()) {
	case -1:
		perror("fork");
		return -1;
	case 0:
		dup2(in, 0);
		dup2(out, 1);
		execvp(cmd[0], cmd);
		perror("execvp");
		return -1;
	default:
		break;
	}
	return 0;
}

/*
 * parse SCGI header, returning CONTENT_LENGTH.
 * returns -1 if either SCGI entry was not found or
 * CONTENT_LENGTH was not there.
 * (don't bother having CONTENT_LENGTH as first header
 * nor checking SCGI value).
 */
int
parseheaders(char *s, int n, int in)
{
	int scgi, clen;
	char *p, *q;
	int i, j;

	scgi =  0;
	clen = -1;

	for (i = 0; i < n;) {
		/* variable name */
		for (p = s+i; s[i] != '\0' && i < n; i++)
			;
		i++;
		/* and value */
		for (q = s+i; s[i] != '\0' && i < n ; i++)
			;
		i++;

		if (strcmp(p, "SCGI") == 0)
			scgi = 1;

		if (strcmp(p, "CONTENT_LENGTH") == 0)
			for (j = clen = 0; q[j] != '\0'; j++)
				clen = clen*10 + (q[j] - '0');

		write(in, p, strlen(p));
		write(in, "=", 1);
		write(in, q, strlen(q));
		write(in, "\n", 1);
#ifdef DBG
		fprintf(stderr, "%s=%s\n", p, q);
#endif
		setenv(p, q, 1);
	}

	write(in, "EOF\n", 4);

	return scgi ? clen : -1;
}

/* const */
char *htmlend = "</html>";
int lhtmlend = 7; /* strlen(htmlend) */

void
doscgi(int s, int in, int out)
{
	char buf[BUFSIZ];
	int len, c, i;
	char ov[16];	/* overlap big enough */
	int clen;
	FILE *f;
	int n;

	/* bufferize socket */
	f = fdopen(s, "r+");
	if (f == NULL) {
		perror("fdopen");
		return;
	}

	/* read length */
	len = 0;
	for (i = 0; i < 10; i++) {
		c = fgetc(f);
		if (c == EOF)
			return;
		if (c == ':')
			break;
		len = len*10 + ((char)c - '0');
	}

	if (c != ':') {
		fprintf(stderr, "error: bad netstring length\n");
		return;
	}

	/* not enough space to read header */
	if (len >= (int)sizeof(buf)) {
		fprintf(stderr, "error: can't buffer SCGI headers\n");
		return;
	}
	fread(buf, 1, len, f);
	clen = parseheaders(buf, len, in);
	if (clen == -1) {
		fprintf(stderr, "Bad SCGI headers\n");
		return;
	}

	/* expect end of netstring */
	if (fgetc(f) != ',') {
		fprintf(stderr, "Missing ',' at end of netstring\n");
		return;
	}

	/* send content to program */
	for (i = 0; i < (int)(clen/sizeof(buf)); i++) {
		fread(buf, 1, sizeof(buf), f);
		write(in, buf, sizeof(buf));
		clen -= sizeof(buf);
	}
	memset(buf, '\0', sizeof(buf));
	fread(buf, 1, clen, f);
	write(in, buf, clen);
	if (buf[clen-1] != '\n')
		write(in, "\n", 1);

	/*
	 * We terminate connection upon
	 * reading htmlend (ie. </html>). It
	 * may happen that htmlend is
	 * break on two reads. We thus check
	 * for an overlapping htmlend using ov.
	 */
	memset(ov, '\n', sizeof(ov));

	/* fetch data from program */
	for (;;) {
		n = read(out, buf, sizeof(buf));
		if (n <= 0)
			break;
		write(s, buf, n);
		if (strstr(buf, htmlend))
			break;

		/* look for a splitted htmlend */
		strncat(ov, buf+n-lhtmlend, lhtmlend);
		if (strstr(ov, htmlend))
			break;
		strncpy(ov, buf+n-lhtmlend, lhtmlend);
	}
}

int
main(int argc, char *argv[])
{
	struct sockaddr_in sin, cin;
	char cip[INET_ADDRSTRLEN];
	int in[2], out[2];
	socklen_t clen;
	char **cmd;
	int port;
	int s, c;
#ifdef DBG
	int on;
#endif

	port = 9000;

	if (argc < 2 || strncmp(argv[1], "-h", 2) == 0)
		goto help;
	if (strcmp(argv[1], "-p") == 0) {
		if (argc < 3)
			goto help;
		errno = 0;
		port = strtol(argv[2], NULL, 10);
		if (errno != 0) {
			perror("strtol");
			return -1;
		}
		cmd = argv+3;
	}
	else
		cmd = argv+1;

	if (pipe(in) == -1 || pipe(out) == -1) {
		perror("pipe");
		return -1;
	}
	if (launchproc(in[0], out[1], cmd) == -1)
		return -1;

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == -1) {
		perror("socket");
		return -1;
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);

#ifdef DBG
	on = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#endif

	if (bind(s, (struct sockaddr *)&sin, sizeof(struct sockaddr)) == -1) {
		perror("bind");
		return -1;
	}

	if (listen(s, 1) == -1) {
		perror("listen");
		return -1;
	}

	fprintf(stderr, "Listening on 127.0.0.1:%d.\n", port);

	clen = sizeof(cin);

	for (;;) {
		c = accept(s, (struct sockaddr *)&cin, &clen);
		if (c == -1) {
			perror("accept");
			close(s);
			return -1;
		}

		memset(cip, '\0', sizeof(cip));
#if DBG
		if (inet_ntop(AF_INET, &(cin.sin_addr), cip, INET_ADDRSTRLEN))
			fprintf(stderr, "Connection from %s:%d.\n", cip, cin.sin_port);
		else
			perror("inet_ntop");
#endif

		doscgi(c, in[1], out[0]);

		close(c);
#if DBG
		fprintf(stderr, "Connection to %s closed.\n", cip);
#endif
	}

	close(s);

	return 0;

help:
	fprintf(stderr, "%s [-p port] cmd\n", argv[0]);
	fprintf(stderr, "%s -h\n", argv[0]);
	return 0;
}
