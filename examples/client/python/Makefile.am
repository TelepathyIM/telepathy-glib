EXTRA_DIST = \
  contact-list.py \
  dialler.py \
  ensure-channel.py \
  inspect-cm.py \
  text-handler.py \
  file-transfer.py \
  ft-handler.py \
  stream-tube-offerer.py \
  stream-tube-accepter.py

installable_scripts = \
    telepathy-1-example-python-dialler \
    telepathy-1-example-python-contact-list \
    telepathy-1-example-python-ensure-channel \
    telepathy-1-example-python-file-transfer \
    telepathy-1-example-python-ft-handler \
    telepathy-1-example-python-inspect-cm \
    telepathy-1-example-python-text-handler \
    telepathy-1-example-python-stream-tube-accepter \
    telepathy-1-example-python-stream-tube-offerer \
    $(NULL)

telepathy-1-example-python-%: %.py
	cp $< $@
	chmod +x $@

if INSTALL_EXAMPLES
bin_SCRIPTS = $(installable_scripts)
endif

CLEANFILES = $(installable_scripts)
