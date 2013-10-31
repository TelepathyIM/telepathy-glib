noinst_LTLIBRARIES = libtp-logger-tests.la

libtp_logger_tests_la_SOURCES = \
    contacts-conn.c \
    contacts-conn.h \
    contact-list-manager.c \
    contact-list-manager.h \
    debug.h \
    echo-chan.c \
    echo-chan.h \
    room-list-chan.c \
    room-list-chan.h \
    simple-account.c		\
    simple-account.h		\
    simple-account-manager.c	\
    simple-account-manager.h	\
    simple-conn.c \
    simple-conn.h \
    util.c \
    util.h \
    logger-test-helper.c \
    logger-test-helper.h

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(TPL_CFLAGS)\
    $(NULL)

libtp_logger_tests_la_LIBADD =  $(TPL_LIBS)
