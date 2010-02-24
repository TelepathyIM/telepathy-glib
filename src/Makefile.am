include $(top_srcdir)/tools/shave.mk
include $(top_srcdir)/tools/flymake.mk

AM_CPPFLAGS =						\
	-I$(top_builddir)				\
	-I$(top_srcdir)					\
	$(ERROR_CFLAGS)					\
	-DG_LOG_DOMAIN=\"TPL\"				\
	$(LIBTPL_CFLAGS)				\
	$(TPL_CFLAGS)					\
	$(DISABLE_DEPRECATED)				\
	$(WARN_CFLAGS)

BUILT_SOURCES =

bin_PROGRAMS = \
	telepathy-logger	\
	test-api

telepathy_logger_LDADD = \
	$(top_builddir)/telepathy-logger/libtelepathy-logger.la \
	$(top_builddir)/extensions/libtpl-extensions.la \
	$(LIBTPL_LIBS)

test_api_LDADD = \
	$(top_builddir)/telepathy-logger/libtelepathy-logger.la \
	$(top_builddir)/extensions/libtpl-extensions.la \
	$(LIBTPL_LIBS)


check_c_sources = \
	$(telepathy_logger_SOURCES)
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

CLEANFILES = $(BUILT_SOURCES) $(bin_PROGRAMS)
