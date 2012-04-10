LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

include ../telepathy-glib/telepathy-glib/codegen.am

TELEPATHY_GLIB_BUILT_SOURCES := \
	telepathy-glib/telepathy-glib.pc \
	telepathy-glib/telepathy-glib-uninstalled.pc \
	telepathy-glib/Android.mk

TELEPATHY_GLIB_GENMARSHAL := $(nodist_libtelepathy_glib_internal_la_SOURCES) \
	$(nodist_geninclude_HEADERS)

telepathy-glib-configure-real:
	cd $(TELEPATHY_GLIB_TOP) ; \
	CC="$(CONFIGURE_CC)" \
	CFLAGS="$(CONFIGURE_CFLAGS)" \
	LD=$(TARGET_LD) \
	LDFLAGS="$(CONFIGURE_LDFLAGS)" \
	CPP=$(CONFIGURE_CPP) \
	CPPFLAGS="$(CONFIGURE_CPPFLAGS)" \
	PKG_CONFIG_LIBDIR="$(CONFIGURE_PKG_CONFIG_LIBDIR)" \
	PKG_CONFIG_TOP_BUILD_DIR=$(PKG_CONFIG_TOP_BUILD_DIR) \
	$(TELEPATHY_GLIB_TOP)/$(CONFIGURE) --host=arm-linux-androideabi && \
	for file in $(TELEPATHY_GLIB_BUILT_SOURCES); do \
		rm -f $$file && \
		make -C $$(dirname $$file) $$(basename $$file) ; \
	done

telepathy-glib-configure: telepathy-glib-configure-real

.PHONY: telepathy-glib-configure

CONFIGURE_TARGETS += telepathy-glib-configure

#include all the subdirs...
-include $(TELEPATHY_GLIB_TOP)/telepathy-glib/Android.mk
