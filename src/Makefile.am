AM_CFLAGS = $(JACK_CFLAGS) $(LIBLO_CFLAGS) $(WSSERVER_CFLAGS) $(JSONC_CFLAGS) $(WARNING_CFLAGS)

bin_PROGRAMS = jackwsmix

jackwsmix_SOURCES = osc.c minimix.c websocket.c db.h
jackwsmix_LDADD = $(JACK_LIBS) $(LIBLO_LIBS) $(WSSERVER_LIBS) $(JSONC_LIBS)
