#!/bin/awk -f
#!/home/mb/plan9/bin/awk -f

function httpheader(len) {
	printf "HTTP/1.1 200 OK\r\n"
	printf "Content-length: %d\r\n", len
	printf "Content-type: text/html\r\n"
	printf "Location: %s\r\n", vars["REQUEST_URI"]
	printf "\r\n"
}

function h(b, c, a) {
	if (a != "")
		a = " " a
	if (c != "")
		return "<" b a ">" c "</" b ">\n"
	else
		return "<" b a "/>\n"
}

function htmlvars(s) {
	for (v in vars) 
		s = s h("tr", h("td", v) h("td", vars[v]))
	s = h("table",
			h("thead",
				h("tr",
					h("th", "Environment variable") \
					h("th", "Value"))) \
			s)
		
	return s
}

function htmlbody(s) {
	s =   h("h1", "GET")
	s = s h("p", "Here are SCGI variables sent by the webserver:")
	s = s htmlvars()
	s = s h("h1", "POST")
	s = s h("form",
			h("input", "", "name=\"data\" placeholder=\"Enter some data\"") \
			h("input", "", "type=\"submit\" value=\"Send!\""),
			"action=\""vars["REQUEST_URI"]"\" method=\"POST\"")
	s = s h("p", "Previous data:")
	s = s h("code", h("pre", content))

	return s
}

function display() {
	html = htmlhead htmlbody() htmlfoot

	httpheader(length(html))
	print html
	fflush()
}

function reset() {
	haseof = 0; content = ""; clen = 0; delete vars
}

BEGIN {
	reset()

	while (getline x <"./head.tmpl")
		htmlhead = htmlhead x "\n"
	while (getline x <"./foot.tmpl")
		htmlfoot = htmlfoot x "\n"
}

# Be careful to the rules' order

haseof && clen > 0 {
	content = content $0
	clen -= length($0)
}

!haseof && $0 == "EOF" {
	haseof = 1
	clen = vars["CONTENT_LENGTH"]
}

!haseof {
	eq = index($0, "=")
	vars[substr($0,0,eq-1)] = substr($0,eq+1,length($0)-eq)
}

haseof && clen == 0 {
	display(); reset()
}
