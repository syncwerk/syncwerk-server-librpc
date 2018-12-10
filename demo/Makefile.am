generated_sources = rpcsyncwerk-signature.h rpcsyncwerk-marshal.h

AM_CFLAGS = @GLIB_CFLAGS@ \
	-I${top_srcdir}/lib

# we need to generate the first
BUILT_SOURCES = gensource

noinst_PROGRAMS = rpcsyncwerk-demo-server rpcsyncwerk-demo-client rpcsyncwerk-async-client

rpcsyncwerk_demo_server_SOURCES = test-object.c rpcsyncwerk-demo-server.c rpcsyncwerk-demo-packet.h

noinst_HEADERS = test-object.h

rpcsyncwerk_demo_server_LDADD = ${top_builddir}/lib/librpcsyncwerk.la @LIB_WS32@ \
	@GLIB_LIBS@ @JANSSON_LIBS@

rpcsyncwerk_demo_client_SOURCES = test-object.c rpcsyncwerk-demo-client.c rpcsyncwerk-demo-packet.h

rpcsyncwerk_demo_client_LDADD = ${top_builddir}/lib/librpcsyncwerk.la @LIB_WS32@ \
	@GLIB_LIBS@ @JANSSON_LIBS@

rpcsyncwerk_async_client_SOURCES = demo-async-client.c rpcsyncwerk-demo-packet.h

rpcsyncwerk_async_client_LDADD = ${top_builddir}/lib/librpcsyncwerk.la @LIB_WS32@ \
	@GLIB_LIBS@ @JANSSON_LIBS@

EXTRA_DIST = rpc_table.py

gensource: ${generated_sources}

rpc_table.stamp: ${top_srcdir}/demo/rpc_table.py ${top_srcdir}/lib/rpcsyncwerk-codegen.py
	@rm -f rpc_table.tmp
	@touch rpc_table.tmp
	@echo "[librpcsyncwerk]: generating rpc header files"
	@PYTHON@ ${top_srcdir}/lib/rpcsyncwerk-codegen.py ${top_srcdir}/demo/rpc_table.py
	@echo "[librpcsyncwerk]: done"
	@mv -f rpc_table.tmp $@

${generated_sources}: rpc_table.stamp

clean-local:
	rm -f ${generated_sources}
	rm -f rpc_table.pyc
	rm -f rpc_table.stamp
	rm -f rpc_table.tmp

