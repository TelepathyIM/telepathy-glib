# This directory re-uses telepathy-glib's code generation mechanisms to
# generate code for interfaces that aren't stable enough for telepathy-glib
# yet, so we can start to adapt example code to use them.

tools_dir = $(top_srcdir)/tools

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)
AM_LDFLAGS = \
    $(ERROR_LDFLAGS) \
    $(NULL)

EXTRA_DIST = \
    all.xml \
    call-content.xml \
    call-stream.xml \
    channel.xml \
    misc.xml

noinst_LTLIBRARIES = libfuture-extensions.la

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the .la path
libfuture_extensions_la_LIBADD = \
    $(GLIB_LIBS) \
    $(DBUS_LIBS) \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la

libfuture_extensions_la_SOURCES = \
    call-content.c \
    call-content.h \
    call-stream.c \
    call-stream.h \
    extensions.c \
    extensions-cli.c \
    extensions.h

nodist_libfuture_extensions_la_SOURCES = \
    _gen/signals-marshal.c \
    _gen/signals-marshal.h \
    _gen/signals-marshal.list \
    _gen/register-dbus-glib-marshallers-body.h \
    _gen/enums.h \
    _gen/gtypes.h \
    _gen/gtypes-body.h \
    _gen/interfaces.h \
    _gen/interfaces-body.h \
    _gen/cli-call-content.h \
    _gen/cli-call-content-body.h \
    _gen/cli-call-stream.h \
    _gen/cli-call-stream-body.h \
    _gen/cli-channel.h \
    _gen/cli-channel-body.h \
    _gen/cli-misc.h \
    _gen/cli-misc-body.h \
    _gen/svc-call-content.h \
    _gen/svc-call-content.c \
    _gen/svc-call-stream.h \
    _gen/svc-call-stream.c \
    _gen/svc-channel.h \
    _gen/svc-channel.c \
    _gen/svc-misc.h \
    _gen/svc-misc.c

BUILT_SOURCES = \
    _gen/all.xml \
    _gen/call-content.xml \
    _gen/call-stream.xml \
    _gen/channel.xml \
    _gen/misc.xml \
    $(nodist_libfuture_extensions_la_SOURCES)

CLEANFILES = $(BUILT_SOURCES)

clean-local:
	rm -rf _gen

XSLTPROCFLAGS = --nonet --novalid

# Generated files which can be generated for all categories simultaneously

_gen/all.xml: all.xml $(wildcard $(srcdir)/*.xml) $(wildcard $(top_srcdir)/spec/*.xml) $(tools_dir)/xincludator.py
	$(mkdir_p) _gen
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/xincludator.py $< > $@

_gen/gtypes.h _gen/gtypes-body.h: _gen/all.xml \
	$(top_srcdir)/tools/glib-gtypes-generator.py
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/tools/glib-gtypes-generator.py \
		$< _gen/gtypes Future

_gen/signals-marshal.list: _gen/all.xml \
	$(tools_dir)/glib-signals-marshal-gen.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-signals-marshal-gen.py $< > $@

_gen/signals-marshal.h: _gen/signals-marshal.list Makefile.am
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --header --prefix=_future_ext_marshal $< > $@

_gen/signals-marshal.c: _gen/signals-marshal.list Makefile.am
	$(AM_V_GEN){ echo '#include "_gen/signals-marshal.h"' && \
	$(GLIB_GENMARSHAL) --body --prefix=_future_ext_marshal $< ; } > $@

_gen/register-dbus-glib-marshallers-body.h: _gen/all.xml \
	$(tools_dir)/glib-client-marshaller-gen.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-client-marshaller-gen.py $< \
		_future_ext > $@

_gen/enums.h: _gen/all.xml \
	$(tools_dir)/c-constants-gen.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/c-constants-gen.py \
		Future \
		$< _gen/enums

_gen/interfaces-body.h _gen/interfaces.h: _gen/all.xml \
	$(tools_dir)/glib-interfaces-gen.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-interfaces-gen.py \
		Future _gen/interfaces-body.h _gen/interfaces.h $<

# Generated files which must be generated per "category". Each TpProxy
# subclass you want to use with --subclass will need to have its own category,
# although you can subdivide further if you want.

_gen/%.xml: %.xml $(wildcard $(srcdir)/*.xml) $(wildcard $(top_srcdir)/spec/*.xml) $(tools_dir)/xincludator.py
	$(mkdir_p) _gen
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/xincludator.py $< > $@

_gen/svc-%.c _gen/svc-%.h: _gen/%.xml \
	$(tools_dir)/glib-ginterface-gen.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-ginterface-gen.py \
		--filename=_gen/svc-$* \
		--signal-marshal-prefix=_future_ext \
		--include='<telepathy-glib/dbus.h>' \
		--include='"_gen/signals-marshal.h"' \
		--not-implemented-func='tp_dbus_g_method_return_not_implemented' \
		--allow-unstable \
		$< Future_Svc_

_gen/cli-channel-body.h _gen/cli-channel.h: _gen/channel.xml \
	$(tools_dir)/glib-client-gen.py Makefile.am
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-client-gen.py \
		--group=channel \
		--subclass=TpChannel \
		--subclass-assert=TP_IS_CHANNEL \
		--iface-quark-prefix=FUTURE_IFACE_QUARK \
		--tp-proxy-api=0.7.6 \
		$< Future_Cli _gen/cli-channel

_gen/cli-call-content-body.h _gen/cli-call-content.h: _gen/call-content.xml \
	$(tools_dir)/glib-client-gen.py Makefile.am
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-client-gen.py \
		--group=call_content \
		--subclass=FutureCallContent \
		--subclass-assert=FUTURE_IS_CALL_CONTENT \
		--iface-quark-prefix=FUTURE_IFACE_QUARK \
		--tp-proxy-api=0.7.6 \
		$< Future_Cli _gen/cli-call-content

_gen/cli-call-stream-body.h _gen/cli-call-stream.h: _gen/call-stream.xml \
	$(tools_dir)/glib-client-gen.py Makefile.am
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-client-gen.py \
		--group=call_stream \
		--subclass=FutureCallStream \
		--subclass-assert=FUTURE_IS_CALL_STREAM \
		--iface-quark-prefix=FUTURE_IFACE_QUARK \
		--tp-proxy-api=0.7.6 \
		$< Future_Cli _gen/cli-call-stream

# for now the Endpoint etc. interfaces are on every TpProxy - when we
# have a TpCallEndpoint etc., they should appear on that

_gen/cli-misc-body.h _gen/cli-misc.h: _gen/misc.xml \
	$(tools_dir)/glib-client-gen.py Makefile.am
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-client-gen.py \
		--group=misc \
		--iface-quark-prefix=FUTURE_IFACE_QUARK \
		--tp-proxy-api=0.7.6 \
		$< Future_Cli _gen/cli-misc
