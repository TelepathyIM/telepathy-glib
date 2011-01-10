noinst_LTLIBRARIES = libtp-logger-tests.la

libtp_logger_tests_la_SOURCES = \
    contacts-conn.c \
    contacts-conn.h \
    simple-account.c		\
    simple-account.h		\
    simple-account-manager.c	\
    simple-account-manager.h	\
    simple-conn.c \
    simple-conn.h \
    textchan-null.c \
    textchan-null.h \
    util.c \
    util.h

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(TPL_CFLAGS)\
    $(NULL)

libtp_logger_tests_la_LIBADD =  $(TPL_LIBS)