AM_CFLAGS = -I$(top_srcdir)/src/bin -Wall -O3

AM_CFLAGS += $(GLIB_CFLAGS)
AM_CFLAGS += $(SQLITE3_CFLAGS)
AM_CFLAGS += $(RABBITMQ_CFLAGS)

bin_PROGRAMS = commsd
commsd_SOURCES = commsd.c utils.h utils.c

commsd_LDADD = $(AM_LDFLAGS)
commsd_LDADD += $(GLIB_LIBS)
commsd_LDADD += $(SQLITE3_LIBS)
commsd_LDADD += $(RABBITMQ_LIBS)
