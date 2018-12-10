
AM_CFLAGS = @GLIB_CFLAGS@ \
	@JANSSON_CFLAGS@ \
	-I${top_builddir}/lib \
	-I${top_srcdir}/lib \
	-DG_LOG_DOMAIN=\"Rpcsyncwerk\"

lib_LTLIBRARIES = librpcsyncwerk.la

include_HEADERS = rpcsyncwerk-client.h rpcsyncwerk-server.h rpcsyncwerk-utils.h rpcsyncwerk.h rpcsyncwerk-named-pipe-transport.h

librpcsyncwerk_la_SOURCES = rpcsyncwerk-client.c rpcsyncwerk-server.c rpcsyncwerk-utils.c rpcsyncwerk-named-pipe-transport.c

librpcsyncwerk_la_LDFLAGS = -version-info 1:2:0  -no-undefined

librpcsyncwerk_la_LIBADD = @GLIB_LIBS@ @JANSSON_LIBS@ -lpthread

dist_bin_SCRIPTS = rpcsyncwerk-codegen.py
