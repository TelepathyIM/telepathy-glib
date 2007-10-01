CLEANFILES =
EXTRA_DIST = make-call.pic sequence.pic

if MAKE_SEQ_DIAS

SEQ_DIA_PICS = make-call.pic
SEQ_DIA_PNGS = $(patsubst %.pic,%.png,$(SEQ_DIA_PICS))
CLEANFILES += $(SEQ_DIA_PNGS) log.sh test

all-local: $(SEQ_DIA_PNGS)

# pic2plot wants sequence.pic in the build dir, so use a hack - copy it in
%.png: %.pic sequence.pic
	cp $(srcdir)/sequence.pic .
	set -o pipefail; $(PIC2PLOT) -Tps --font-name AvantGarde-Book $< | $(CONVERT) -density 112 - $@ ; rm .sequence.temp
	rm -f sequence.pic
endif
