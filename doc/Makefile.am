SUBDIRS = lib

CLEANFILES =
EXTRA_DIST = make-call.pic sequence.pic

if MAKE_SEQ_DIAS

SEQ_DIA_PICS = make-call.pic
SEQ_DIA_PNGS = $(patsubst %.pic,%.png,$(SEQ_DIA_PICS))
CLEANFILES += $(SEQ_DIA_PNGS) log.sh test

all-local: $(SEQ_DIA_PNGS)

# pic2plot wants sequence.pic in the build dir, so use a hack - copy it in
%.png: %.pic sequence.pic
	if ! test -e sequence.pic; then \
		touch copied-sequence-pic && \
		cp $(srcdir)/sequence.pic .; \
	fi
	set -o pipefail; $(PIC2PLOT) -Tps --font-name AvantGarde-Book $< | $(CONVERT) -density 112 - $@ ; rm .sequence.temp
	if test -e copied-sequence-pic; then \
		rm -f copied-sequence-pic sequence.pic; \
	fi
endif
