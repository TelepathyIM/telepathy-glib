SUBDIRS = tools telepathy-logger data src

ACLOCAL_AMFLAGS = -I m4

DISTCHECK_CONFIGURE_FLAGS =		\
	--disable-scrollkeeper		\
	--disable-schemas-install

EXTRA_DIST = 			\
        ChangeLog               \
        README                  \
	autogen.sh

DISTCLEANFILES = 		\
        ChangeLog

# Workaround broken scrollkeeper that doesn't remove its files on
# uninstall.
distuninstallcheck_listfiles = find . -type f -print | grep -v '^\./var/scrollkeeper'

distclean-local:
	if test "x$(srdcir)" = "x."; then :; else \
		rm -f ChangeLog; \
	fi

ChangeLog:
	@echo Creating $@
	@if test -d "$(srcdir)/.git"; then \
	  (GIT_DIR=$(top_srcdir)/.git ./missing --run git log TPL_1.0.. --stat) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> $@); \
	fi

ctags:
	$(MAKE) -C telepathy-logger ctags
	$(MAKE) -C src ctags
	cat telepathy-logger/tags src/tags >> tags


.PHONY: ChangeLog

