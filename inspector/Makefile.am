if FALSE

bin_PROGRAMS = telepathy-inspector

telepathy_inspector_SOURCES = \
    main.c \
    ti-channel.c \
    ti-conn-manager.c \
    ti-connection.c \
    ti-dlg-about.c \
    ti-dlg-add-members.c \
    ti-dlg-advertise-caps.c \
    ti-dlg-get-handles.c \
    ti-dlg-get-text.c \
    ti-dlg-hold-handles.c \
    ti-dlg-preferences.c \
    ti-dlg-handles-list.c \
    ti-dlg-request-channel.c \
    ti-dlg-request-conn.c \
    ti-dlg-set-aliases.c \
    ti-dlg-user-status.c \
    ti-handle-list-editor.c \
    ti-handle-mapper.c \
    ti-message-bus.c \
    ti-page.c \
    ti-page-active-conns.c \
    ti-page-aliasing.c \
    ti-page-capabilities.c \
    ti-page-channel-group.c \
    ti-page-channel-text.c \
    ti-page-presence.c \
    ti-page-properties.c \
    ti-page-room-list.c \
    ti-preferences.c \
    ti-properties.c \
    ti-signals-marshal.c \
    ti-util.c \
    ti-wdg-conn-param.c \
    ti-wdg-generic-conn-param.c \
    ti-wdg-boolean-conn-param.c \
    ti-wnd-channel.c \
    ti-wnd-connection.c \
    ti-wnd-main.c

ti_datadir = $(datadir)/telepathy-inspector

ti_data_DATA =\
    dlg-add-members.xml \
    dlg-advertise-caps.xml \
    dlg-hold-handles.xml \
    dlg-preferences.xml \
    dlg-handles-list.xml \
    dlg-request-conn.xml \
    dlg-set-aliases.xml \
    dlg-user-status.xml \
    page-active-conns.xml \
    page-aliasing.xml \
    page-capabilities.xml \
    page-channel-text.xml \
    page-presence.xml \
    page-properties.xml \
    page-room-list.xml \
    wnd-channel.xml \
    wnd-main.xml \
    dlg-get-handles.xml \
    dlg-get-text.xml \
    dlg-request-channel.xml \
    page-channel-group.xml \
    wnd-connection.xml

_gen/signals-marshal.h: ti-signals-marshal.list
	$(mkdir_p) _gen
	$(GLIB_GENMARSHAL) --header --prefix=ti_marshal $< > $@

_gen/signals-marshal.c: ti-signals-marshal.list
	$(mkdir_p) _gen
	$(GLIB_GENMARSHAL) --body --prefix=ti_marshal $< > $@

endif
