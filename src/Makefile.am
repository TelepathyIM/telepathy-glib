include $(top_srcdir)/tools/flymake.mk

AM_CPPFLAGS =						\
	-I$(top_builddir)				\
	-I$(top_builddir)/extensions			\
	-I$(top_srcdir)					\
	$(ERROR_CFLAGS)					\
	-DG_LOG_DOMAIN=\"tp-logger\"			\
	-DTP_DISABLE_SINGLE_INCLUDE			\
	$(TPL_CFLAGS)					\
	$(DISABLE_DEPRECATED)				\
	$(WARN_CFLAGS)

BUILT_SOURCES =

libexec_PROGRAMS = \
	telepathy-logger

noinst_PROGRAMS = \
	test-api

telepathy_logger_LDADD = \
	$(top_builddir)/telepathy-logger/libtelepathy-logger.la \
	$(TPL_LIBS)

test_api_LDADD = \
	$(top_builddir)/telepathy-logger/libtelepathy-logger.la \
	$(TPL_LIBS)


check_c_sources = \
	$(telepathy_logger_SOURCES)
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

CLEANFILES = $(BUILT_SOURCES) $(bin_PROGRAMS)
