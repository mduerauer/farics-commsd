AM_CFLAGS = -I$(top_srcdir)/src/bin

AM_CFLAGS += $(GLIB_CFLAGS)
AM_CFLAGS += $(SQLITE3_CFLAGS)
AM_CFLAGS += $(RABBITMQ_CFLAGS)

bin_PROGRAMS = commsd
commsd_SOURCES = main.c

commsd_LDADD = $(GLIB_LIBS)
commsd_LDADD += $(SQLITE3_LIBS)
commsd_LDADD += $(RABBITMQ_LIBS)
