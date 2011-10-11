
bin_PROGRAMS = test-server test-client

test_server_SOURCES = test-server.c verbose.c
test_server_LDADD = $(top_builddir)/src/librudp.la $(ELA_LIBS)
test_server_CFLAGS = -I$(top_srcdir)/include $(ELA_CFLAGS)

test_client_SOURCES = test-client.c verbose.c
test_client_LDADD = $(top_builddir)/src/librudp.la $(ELA_LIBS)
test_client_CFLAGS = -I$(top_srcdir)/include $(ELA_CFLAGS)
