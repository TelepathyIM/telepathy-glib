CLEANFILES =

if MAKE_SEQ_DIAS

SEQ_DIA_PICS = make-call.pic recieve-call.pic
SEQ_DIA_PNGS = $(patsubst %.pic,%.png,$(SEQ_DIA_PICS))
CLEANFILES += $(SEQ_DIA_FILES)

all-local: $(SEQ_DIA_PNGS)
dist-hook-local:
	-cp $(srcdir)/$(SEQ_DIA_PICS) $(distdir)/

%.png: %.pic sequence.pic
	set -o pipefail; $(PIC2PLOT) -Tps --font-name AvantGarde-Book $< | $(CONVERT) -density 112 - $@ ; rm .sequence.temp
endif
