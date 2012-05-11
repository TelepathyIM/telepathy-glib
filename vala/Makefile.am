vapidir = $(datarootdir)/vala/vapi
vapi_DATA = \
	telepathy-glib.vapi \
	$(NULL)
dist_vapi_DATA = \
	telepathy-glib.deps \
	$(NULL)

BUILT_SOURCES = \
	$(vapi_DATA) \
	$(NULL)

pkgconfigdir = ${libdir}/pkgconfig

telepathy-glib.vapi: $(top_builddir)/telepathy-glib/TelepathyGLib-0.12.gir
	$(VAPIGEN_V)$(VAPIGEN) \
		--library telepathy-glib \
		--metadatadir=$(top_srcdir)/telepathy-glib \
		--pkg gio-2.0 \
		$< \
		$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

# set up the verbosity rules to avoid some build noise
VAPIGEN_V = $(VAPIGEN_V_$(V))
VAPIGEN_V_ = $(VAPIGEN_V_$(AM_DEFAULT_VERBOSITY))
VAPIGEN_V_0 = @echo "  VAPIG " $^;
