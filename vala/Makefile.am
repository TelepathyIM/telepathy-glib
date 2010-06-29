XSLTPROCFLAGS = --nonet --novalid

vapidir = $(datarootdir)/telepathy/vapi
vapi_DATA = telepathy-glib.vapi

BUILT_SOURCES = \
	telepathy-glib.vapi \
	$(NULL)

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = telepathy-vala.pc

telepathy-glib.vapi: $(top_srcdir)/telepathy-glib/TelepathyGLib-0.12.gir
	$(VAPIGEN_V)$(VAPIGEN) \
		--library telepathy-glib \
		--pkg gio-2.0 \
		$< \
		$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

# set up the verbosity rules to avoid some build noise
# XXX: once automake >= 1.11 is common, remove these, push VALASOURCES files
# into SOURCES and add AM_PROG_VALAC to configure.ac
VAPIGEN_V = $(VAPIGEN_V_$(V))
VAPIGEN_V_ = $(VAPIGEN_V_$(AM_DEFAULT_VERBOSITY))
VAPIGEN_V_0 = @echo "  VAPIG " $^;

VGI_V = $(VGI_V_$(V))
VGI_V_ = $(VGI_V_$(AM_DEFAULT_VERBOSITY))
VGI_V_0 = @echo "  VGI   " $^;
