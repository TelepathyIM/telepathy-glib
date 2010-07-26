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
pkgconfig_DATA = telepathy-vala.pc

telepathy-glib.vapi: $(top_srcdir)/telepathy-glib/TelepathyGLib-0.12.gir
	$(VAPIGEN_V)$(VAPIGEN) \
		--library telepathy-glib \
		--pkg gio-2.0 \
		$< \
		$(NULL)
	@mv $@ telepathy-glib.vapi.tmp1
	$(AM_V_GEN)sed -e \
		's/TelepathyGLib-0.12.h/telepathy-glib\/telepathy-glib.h/' \
		< telepathy-glib.vapi.tmp1 > telepathy-glib.vapi.tmp2
	$(AM_V_GEN)sed -e \
		's/cprefix = "tp"/cprefix = "Tp"/' \
		< telepathy-glib.vapi.tmp2 > $@
	@rm telepathy-glib.vapi.tmp*

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

# set up the verbosity rules to avoid some build noise
VAPIGEN_V = $(VAPIGEN_V_$(V))
VAPIGEN_V_ = $(VAPIGEN_V_$(AM_DEFAULT_VERBOSITY))
VAPIGEN_V_0 = @echo "  VAPIG " $^;
