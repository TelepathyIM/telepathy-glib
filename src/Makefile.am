include $(top_srcdir)/tools/shave.mk
include $(top_srcdir)/tools/flymake.mk

AM_CPPFLAGS =						\
	$(ERROR_CFLAGS)					\
	-I$(top_srcdir)                          	\
	-I$(top_srcdir)/include                        	\
	-DG_LOG_DOMAIN=\"tpl\"				\
	$(TPL_CFLAGS)					\
	$(LIBTPL_CFLAGS)					\
	$(DISABLE_DEPRECATED)				\
	$(WARN_CFLAGS)

LDADD =								\
	$(TPL_LIBS)						\
	$(LIBTPL_LIBS)	
#	$(top_builddir)/libempathy-gtk/libempathy-gtk.la	\
#	$(top_builddir)/libempathy/libempathy.la		\
#	$(top_builddir)/extensions/libemp-extensions.la		\
#	$(LIBNOTIFY_LIBS)					\
#	$(LIBCHAMPLAIN_LIBS)					\
#	$(WEBKIT_LIBS)

bin_PROGRAMS =			\
	telepathy-logger

BUILT_SOURCES= 
#	empathy-tube-dispatch-enumtypes.h \
#	empathy-tube-dispatch-enumtypes.c

telepathy_logger_handwritten_source = \
		test.c \
		tpl_channel_data.c \
		tpl_contact.c \
		tpl_headless_logger_init.c \
		tpl_log_entry_text.c \
		tpl_observer.c \
		tpl_text_channel_data.c \
		tpl-time.c \
		tpl_utils.c \
		tpl-log-manager.c \
		tpl-log-store.c \
		tpl-log-store-empathy.c

telepathy_logger_SOURCES =	\
	$(telepathy_logger_handwritten_source)	

nodist_telepathy_logger_SOURCES = $(BUILT_SOURCES)

check_c_sources = \
    $(telepathy_logger_handwritten_source) \
    $(telepathy_logger_logs_SOURCES)
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

#DISABLED#uidir = $(datadir)/telepathy-logger
#DISABLED#ui_DATA = 					\
#DISABLED#	empathy-accounts-dialog.ui		\
#DISABLED#	empathy-call-window-fullscreen.ui	\
#DISABLED#	empathy-call-window.ui			\
#DISABLED#	empathy-chat-window.ui			\
#DISABLED#	empathy-chatrooms-window.ui		\
#DISABLED#	empathy-ft-manager.ui			\
#DISABLED#	empathy-import-dialog.ui		\
#DISABLED#	empathy-main-window.ui			\
#DISABLED#	empathy-new-chatroom-dialog.ui		\
#DISABLED#	empathy-preferences.ui			\
#DISABLED#	empathy-status-icon.ui

EXTRA_DIST =			\
	$(autostart_DATA)	\
	$(ui_DATA)

#DISABLED#if HAVE_LIBCHAMPLAIN
#DISABLED#empathy_handwritten_source += 				\
#DISABLED#	empathy-map-view.c			\
#DISABLED#	empathy-map-view.h
#DISABLED#
#DISABLED#ui_DATA += 					\
#DISABLED#	empathy-map-view.ui
#DISABLED#else
#DISABLED#EXTRA_DIST +=					\
#DISABLED#	empathy-map-view.c			\
#DISABLED#	empathy-map-view.h			\
#DISABLED#	empathy-map-view.ui
#DISABLED#endif

dist_man_MANS =			

# rules for making the glib enum objects
%-enumtypes.h: %.h Makefile.in
	$(QUIET_GEN)glib-mkenums \
	--fhead "#ifndef __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__\n#define __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
	--fprod "/* enumerations from \"@filename@\" */\n" \
	--vhead "GType @enum_name@_get_type (void);\n#define $(shell echo $* | tr [:lower:]- [:upper:]_ | sed 's/_.*//')_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n"         \
	--ftail "G_END_DECLS\n\n#endif /* __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__ */" \
	$< > $@

%-enumtypes.c: %.h Makefile.in
	$(QUIET_GEN)glib-mkenums \
	--fhead "#include <$*.h>\n#include <$*-enumtypes.h>" \
	--fprod "\n/* enumerations from \"@filename@\" */" \
	--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {"     \
	--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@VALUENAME@\" }," \
	--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
	$< > $@

CLEANFILES = $(BUILT_SOURCES)
