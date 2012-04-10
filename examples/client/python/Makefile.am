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
    telepathy-example-python-dialler \
    telepathy-example-python-contact-list \
    telepathy-example-python-ensure-channel \
    telepathy-example-python-file-transfer \
    telepathy-example-python-ft-handler \
    telepathy-example-python-inspect-cm \
    telepathy-example-python-text-handler \
    telepathy-example-python-stream-tube-accepter \
    telepathy-example-python-stream-tube-offerer \
    $(NULL)

telepathy-example-python-%: %.py
	cp $< $@
	chmod +x $@

if INSTALL_EXAMPLES
bin_SCRIPTS = $(installable_scripts)
endif

CLEANFILES = $(installable_scripts)
