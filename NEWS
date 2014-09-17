telepathy-glib 0.24.1 (2014-08-25)
==================================

Fixes:

• base-client: fix potential uninitialized variable bug (Guillaume)
• Fix a potential crash in contact-list example (fd.o #79006, Guillaume)


telepathy-glib 0.24.0 (2014-03-26)
==================================

The “space Tolkien” release.

Fixes since 0.23.3:

• don't leak every D-Bus method call result, a regression in 0.23.1 (Simon)

telepathy-glib 0.23.3 (2014-03-18)
==================================

This is the release candidate for the future 0.24.0 stable release.

Enhancements:

• TpProtocol gained API to access to its immutable properties as a
  GVariant. (fd.o #55108, Guillaume)

• TpCallStream and TpCallContent now inherit the factory from their
  TpCallChannel. (fd.o #76168, Guillaume)

Fixes:

• fix a memory leak when cleaning up TpProxy "prepare" requests
  (fd.o #76000, Simon)

• fix a memory leak for paths to contacts' avatar data (fd.o #76000, Simon)

• fix crashes in TpFileTransferChannel with GLib 2.39 (fd.o #72319, Xavier)

• fix some paths memory leaks (fd.o #76119, Guillaume)

• tp_list_connection_managers_async() now terminates properly if there is no
  CM installed. (fd.o #68892, Guillaume)

telepathy-glib 0.23.2 (2014-02-26)
==================================

Enhancements:

• TpBaseConnection now has an "account-path-suffix" property
  (fd.o #74030, Xavier)

• New high level TpAccountChannelRequest API, including tubes, Conference and
  SMSChannel. (fd.o #75450, Guillaume)

• 'TargetHandleType: None' is now automatically added when requesting a
  channel with TpAccountChannelRequest if no handle type has been defined.
  (fd.o #75450, Guillaume)

telepathy-glib 0.23.1 (2014-02-04)
==================================

The “undead space elves” release.

Dependencies:

• GLib 2.36 or later is required

Deprecations:

• TpPresenceMixin: optional arguments are deprecated, apart from a
  string named "message". This matches our current D-Bus API.

Enhancements:

• tp_protocol_normalize_contact_async(),
  tp_protocol_identify_account_async(), and high-level API for
  the Protocol Addressing and Presence interfaces (fd.o #71048, Simon)

• More accessors for TpPresenceStatusSpec, which is now a boxed type
  (fd.o #71048, Simon)

• tp_connection_manager_param_dup_variant_type() (fd.o #71093, Simon)

• Better debug output (fd.o #68390, #71048; Simon)

Fixes:

• In the examples, specifically ask for "TelepathyGlib-0.12" (this API
  version), not Telepathy 1.0 (fd.o #49737, Simon)

• Improve tests' isolation from the real session bus (Xavier)

• Fix a critical warning for each new connection under GLib 2.39
  (fd.o #72303, Xavier)

• Fix some possible crashes in file transfer channels, particularly
  under GLib 2.39 (fd.o #72319, Xavier)

• Correct tp_account_request_set_avatar documentation (Xavier)

• Fix a TpConnection reference-leak in TpBaseClient (Guillaume)

telepathy-glib 0.23.0 (2013-10-28)
==================================

We no longer guarantee compatible upgrades within a development (odd) branch,
see README for details.

Dependencies:

• GLib 2.34 or later is required.

Enhancements:

• Spec 0.27.3
  · added Conn.I.Sidecars1
  · added Conn.I.Renaming
  · added CD.I.Messages1

• TpAccount::avatar-changed signal (fd.o #52938, Guillaume)

• tp_value_array_free: equivalent of g_value_array_free but does not provoke
  deprecation warnings from GLib (fd.o #69849, Simon)

• tp_account_is_prepared and tp_account_manager_is_prepared are now deprecated
  (Guillaume)

Fixes:

• tp_contact_set_attributes: don't warn on genuinely absent interfaces
  (fd.o #68149, Simon)

• channel-group: don't crash if no message has been provided (Guillaume)

telepathy-glib 0.22.0 (2013-10-02)
==================================

The “don't starve” release.

This is a new stable branch, recommended for use with GNOME 3.10.

Fixes since 0.21.2:

• When an avatar is downloaded, announce the change to the avatar token
  immediately; if the avatar changes from A to B while we're still doing the
  asynchronous file saving, don't set A as the new avatar when it has been
  saved. Regression in 0.21.2. (fd.o #70010, Simon)

• Don't crash if the AccountManager returns an incorrect type for the
  Avatar (fd.o #69849, Simon)

Significant changes since the previous stable branch, 0.20.x:

• tp_connection_get_self_contact() now returns NULL if the contact's
  connection has been invalidated, in order to break a reference cycle

• Avatars are saved to the cache asynchronously

• TpBaseConnection implements SelfID, SelfContactChanged according to
  telepathy-spec 0.27.2

• TpAccount:uri-schemes property, with change notification requiring
  Mission Control 5.15+

telepathy-glib 0.21.2 (2013-09-24)
==================================

The “always another thing” release.

Enhancements:

• Writing avatars into cache now uses asynchronous I/O. (fd.o #63402;
  Luca Versari, Chandni Verma, Simon McVittie)

• telepathy-spec 0.27.2
  · add SelfID, SelfContactChanged

• tp_dbus_properties_mixin_dup_all() is now public (fd.o #69283, Simon)

• TpBaseProtocol now lists Presence.Statuses as an immutable
  property. (fd.o #69520, Guillaume)

• TpBaseConnection: Implement SelfID and SelfContactChanged as defined in
  spec 0.27.2. (Xavier)

• The inspect-cm example now inspects all CMs if run without arguments
  (fd.o #68390, Simon)

Fixes:

• Don't crash if GetContactInfo() fails (fd.o #46430, Guillaume)

• Fix a race condition that could result in telepathy-haze protocol support
  not being detected (fd.o #67183, Simon)

• Fix documentation for tp_connection_get_self_handle (Emilio)

• Make TpHeap work correctly with GComparator functions that return
  values outside {-1, 0, 1} (fd.o #68932, Debarshi Ray)

• Examples have been updated to use more recent API (Simon)

• Better debug-logging (fd.o #68390, Simon)

telepathy-glib 0.21.1 (2013-06-20)
==================================

The “imperative tense” release.

Fixes:

• Fix a wrong introspection annotation on tp_debug_client_get_messages_finish()
  that would lead to use-after-free (fd.o #65518, Simon)

• Isolate regression tests better (fd.o #63119, Simon)

• Explicitly annotate tp_account_update_parameters_finish()'s
  'unset_parameters' argument to be a NULL-terminated string array. It was
  previously incorrectly inferred to be a string, for some reason. (wjt)

• Always flag delivery reports with Non_Text_Content. (fd.o #61254, wjt)

• Don't announce legacy Group channels twice (fd.o #52011; Jonny, Simon)

• Don't crash if a broken connection manager signals a TLSCertificate
  with no CertificateChainData, just invalidate the channel
  (fd.o #61616, Guillaume)

• Adjust regression tests so we can distcheck under Automake 1.13,
  and various other build-system updates (fd.o #65517, Simon)

telepathy-glib 0.21.0 (2013-04-03)
==================================

The "if only it was JS code" release.

This starts a new development branch.

Enhancements:

• Code-generation now copes with ${PYTHON} being set to Python 3
  (e.g. "./configure PYTHON=python3" on Debian); Python 2 remains
  fully supported (fd.o #56758, Simon)

• Add uri-schemes property on TpAccount, with notify::uri-schemes
  emitted if using a recent AcountManager like Mission Control 5.15 or
  later (Guillaume)

Fixes:

• Remove the pkg-config dependency from .pc files (Will)

• In TpSimpleClientFactory, don't crash when ensuring a contact for an
  obsolete connection manager without "immortal handles" fails
  (Maksim Melnikau)

• Add missing (element-type) introspection annotations to
  tp_capabilities_get_channel_classes, tp_asv_get_bytes and
  tp_client_channel_factory_dup_channel_features (fd.o #58851, Philip Withnall)

• Don't emit the NewChannels signal twice for the obsolete ContactList GROUP
  channels (fd.o #52011, Simon)

• Fix builds with Automake 1.13 (fd.o #59604, Nuno Araujo)

• Fix unit tests when running with glib >=2.36 (fd.o #63069, Xavier)

• Fix refcycle preventing TpConnection objects to be freed. This theoretically
  introduce a behaviour change of tp_connection_get_self_contact() that now
  returns NULL when the connection as been invalidated. (fd.o #63027, Xavier)

Deprecations:

• tp_g_key_file_get_int64, tp_g_key_file_get_uint64 (use the corresponding
  functions from GLib ≥ 2.26)

telepathy-glib 0.20.1 (2012-11-09)
==================================

The "world's slowest ticket machines" release.

Fixes:

• In Call channels, ignore state transitions where the state did not
  actually change (fd.o #56044, Debarshi Ray)

• Process the SelfHandleChanged signal for non-obsolete connection managers
  (fd.o #55666, Simon)

telepathy-glib 0.20.0 (2012-10-03)
==================================

The "why not indeed" release.

This starts a new stable branch, recommended for use with GNOME 3.6.

Summary of changes since the last stable branch, 0.18:

• GLib 2.32 or later is required.

• Many old things are now deprecated. New TP_VERSION_MIN_REQUIRED and
  TP_VERSION_MAX_ALLOWED macros are provided for deprecation control:
  they work like the ones in GLib 2.32.

• Many functions that expected or returned dbus-glib parameterized types
  are now deprecated, and have an equivalent GVariant-based function which
  should be used instead.

• The only headers you should #include are <telepathy-glib/telepathy-glib.h>,
  <telepathy-glib/telepathy-glib-dbus.h> and <telepathy-glib/proxy-subclass.h>.
  Including anything else is deprecated.

• All TpChannel APIs using contact TpHandle have been deprecated in favor of
  their TpContact variants.

• TpRoomList and TpRoomInfo: high level API to list rooms on a server.

• TpDebugClient: high level API to retrieve logs from Telepathy components.

• TpTLSCertificate: TpProxy subclass representing a TLS certificate

• TpAccountRequest: object to help account creation

• TpSimpleClientFactory gained API to prepare TpContact objects with the
  features set on it.

• Add tp_vardict_get_uint32() etc., analogous to tp_asv_get_uint32() etc.

• The configure flags --disable-coding-style-checks, --disable-doc-checks
  and part of --disable-Werror have been superseded
  by --disable-fatal-warning

There were no code changes since 0.19.10.

telepathy-glib 0.19.10 (2012-09-26)
===================================

The “Why not 0.20.0?” release.

This is the release candidate for the future 0.20.0 stable release.

Enhancements:

• Add tp_vardict_get_uint32() etc., analogous to tp_asv_get_uint32() etc.
  (Xavier)

• tp_channel_dispatch_operation_get_channels() is now introspected (fd.o #55102, Simon)

• Add tp_vardict_get_*() helper functions to lookup values in a GVariant of
  type %G_VARIANT_TYPE_VARDICT (Xavier)

• Add tp_variant_convert() and tp_variant_type_classify() to manipulate and
  convert GVariant (Xavier)

A bunch of GVariant oriented API have been added as an alternative of their
GValue equivalent:
• tp_{dbus,stream}_tube_channel_dup_parameters_vardict() (fd.o #55024, Chandni Verma)
• tp_message_set_variant() and tp_message_dup_part() (fd.o #55096, Simon)
• tp_account_channel_request_dup_request(),
• tp_account_channel_request_new_vardict(),
  tp_channel_request_dup_immutable_properties(), tp_channel_request_dup_hints(),
  tp_account_channel_request_set_hints() and
  TpAccountChannelRequest:request-vardict property (fd.o #55099, Simon)
• tp_g_socket_address_from_g_variant() and
  tp_address_g_variant_from_g_socket_address() (fd.o #55101, Simon)
• TpDBusTubeChannel:parameters-vardict and
  TpStreamTubeCha:parameters-vardict properties (fd.o #55024, Simon)
• tp_contact_dup_location() and TpContact:location-vardict
  property (fd.o#55095, Simon)
• tp_base_client_add_{observer,approver,handler}_filter_vardict (fd.o #55100, Simon)

telepathy-glib 0.19.9 (2012-09-11)
==================================

Deprecations:

• Various functions whose names include _borrow_, such as
  tp_proxy_borrow_interface_by_id() and
  tp_channel_borrow_immutable_properties(),
  are deprecated. Use the corresponding function with _get_ or _dup_ in its
  name, such as tp_proxy_get_interface_by_id() or
  tp_channel_dup_immutable_properties(), instead. (Xavier)

• Various functions returning a (transfer container) list, such as
  tp_text_channel_get_pending_messages(), are deprecated. Use the
  corresponding function with _dup_ in its name, such as
  tp_text_channel_dup_pending_messages(), instead. (Xavier)

• tp_handle_set/get_qdata() are now deprecated. Handles are immortal so using
  them would leak the data until Connection gets disconnected.

• tp_channel_request_new() and tp_channel_dispatch_operation_new() constructors
  are now deprecated. Applications should not need them since they are created
  internaly in TpBaseClient.

Enhancements:

• Some functions from util.h, such as tp_utf8_make_valid(),
  tp_escape_as_identifier() and user-action-time functions are now
  available via gobject-introspection (fd.o #54543, Simon)

Fixes:

• Get the remote contact from a MediaDescription correctly (fd.o #54425,
  Sjoerd)

• Fix an incorrect error on UpdateLocalMediaDescription (Sjoerd)

• Fix use of an unterminated string in the tls-certificates test (Sjoerd)

• Fix service-side codegen using single includes, which prevents extensions to
  build with TP_DISABLE_SINGLE_INCLUDE.

telepathy-glib 0.19.8 (2012-08-31)
==================================

Enhancements:

• New introspectable function tp_account_channel_request_set_hint() to add hints
  one by one. (Sjoerd)

Fixes:

• Set the ChannelRequests immutable properties when observing/handling channels.
  (Sjoerd)

telepathy-glib 0.19.7 (2012-08-27)
==================================

Enhancements:

• Add API to TpBaseChannel to allow it to disappear and reappear from
  the bus without disposing the object. (fd.o#48210, Jonny)

Fixes:

• In tp_account_dup_storage_identifier_variant, don't fail if there is no
  storage identifier (fd.o #53201, Guillaume)

• Remove duplicate TpBaseConnection typedef, fixing compilation with pre-C11
  compilers like gcc < 4.6 (fd.o #53100, Simon)

• Use AS_CASE instead of case/esac, and AS_IF instead of
  if/then/[else/]fi in configure.ac, as they are safer and guaranteed
  to work (fd.o#681413, Simon)

telepathy-glib 0.19.6 (2012-08-06)
==================================

Enhancements:

• Add tp_account_manager_can_set_default() (Guillaume)

Fixes:

• Fix generation of reentrant-methods.list in highly parallel builds
  (fd.o #52480, Ross Burton)

• TpBaseChannel: assert that the subclass sets TargetHandleType. (Will)

telepathy-glib 0.19.5 (2012-07-24)
==================================

Fixes:

• fdo#52441 - fix warning when preparing blocked contacts before
  TP_CONNECTION_FEATURE_CONNECTED is officially prepared.
• TpAccountManager: set the requested presence on newly created accounts

telepathy-glib 0.19.4 (2012-07-19)
==================================

The “#hellopaul” release.

Enhancements:

• TpBaseConnectionManager, TpBaseConnection and TpBaseProtocol: add virtual
  methods to get interfaces. (Jonny)

• Add tp_account_request_set_storage_provider(). (Guillaume)

Fixes:

• base-connection: return from RequestHandles if called with no names. (Jonny)

• TpAccountRequest: add missing 'service' property getter. (Guillaume)

telepathy-glib 0.19.3 (2012-07-05)
==================================

Deprecations:

• tp_account_new(), tp_connection_new() and tp_*_channel_new() have been
  deprecated. Those proxies should be created using corresponding
  TpSimpleClientFactory APIs. (Xavier)

• TpAccount, TpConnection and TpConnectionManager: deprecate
  "connection-manager" and "protocol" properties and replace them by "cm-name"
  and "protocol-name" to be more consistent. Ditto for their getters. (Xavier)

Fixes:

• fdo#51444 - Crash in TpBaseClient (Xavier)

telepathy-glib 0.19.2 (2012-06-28)
==================================

Enhancements:

• TpDynamicHandleRepo can now have an asynchronous ID normalization function.
  That function can be set by Connection Manager wishing to do network rountrip
  to normalize an ID. (Xavier)

Fixes:

• fdo#51250 - tp_debug_client_get_messages_async: error is not propagated
  (Guillaume)

telepathy-glib 0.19.1 (2012-06-06)
==================================

Dependencies:

• Valac ≥ 0.16.0 is now required for the Vala bindings.

Deprecations:

• TpHandle reference count related APIs have been deprecated.
  - The CM-side APIs tp_handle(s)_ref/unref() and
    tp_handle(s)_client_hold/release() were already no-op since immortal
    handles.
  - The Client-side APIs tp_connection_hold/unref_handles() are not needed with
    CMs having immortal handles. Others CM are considered legacy, clients
    wanting to keep support for them should continue using those deprecated
    APIs (Notably Empathy already dropped support for them since version 3.4).
  (Xavier)

• Contact attributes APIs have been deprecated. It is considered an internal
  implementation detail for TpContact that clients doesn't need to care about.
  tp_connection_get_contact(_list)_attributes(). (Xavier)

• tp_connection_request_handles() has been deprecated because higher level APIs
  now make it useless. If handle_type is TP_HANDLE_TYPE_CONTACT, use
  tp_connection_dup_contact_by_id_async() instead. For channel requests, use
  tp_account_channel_request_set_target_id() instead. (Xavier)

• tp_channel_manager_emit_new_channels() has been deprecated, emitting multiple
  channels at once is discouraged because it makes client-side code more
  complicated for no good reason. (Jonny)

• tp_connection_parse_object_path() has been deprecated because the connection's
  object-path is already parsed internaly and exposed via
  tp_connection_get_connection_manager_name() and
  tp_connection_get_protocol_name(). (Xavier)

• tp_account_parse_object_path() has been deprecated because the account's
  object-path is already parsed internaly and exposed via
  tp_account_get_connection_manager() and tp_account_get_protocol(). (Xavier)

• tp_account_ensure_connection() has been deprecated. Its purpose was to share
  a common TpConnection object between TpBaseClient and TpAccount. Now proxy
  uniqueness is guaranteed by TpSimpleClientFactory. (Xavier)

• Struct members of TpProxy, TpConnectionManagerParam, TpConnectionManager and
  TpBaseConnection have been sealed. In the same spirit than G_SEAL, we
  introduced _TP_SEAL to force usage of getters and setters. (Simon)

• Including individual headers is now deprecated. Only the teleapthy-glib.h and
  telepathy-glib-dbus.h meta headers should be included in applications. Build
  error is disabled by default and can be turned on by defining
  TP_DISABLE_SINGLE_INCLUDE. (Xavier)

• tp_list_connection_managers() has been deprecated in favor of
  tp_list_connection_managers_async() and tp_list_connection_managers_finish().
  (Simon)

• TpConnectionManagerProtocol and all its related functions have been deprecated
  in favor of TpProtocol. (Simon)

Enhancements:

• New TpAccountRequest object to help account creation (Jonny)

• TpSimpleClientFactory gained API to prepare TpContact objects with the
  features set on it. tp_simple_client_factory_upgrade_contacts_async() is
  convenience API for tp_connection_upgrade_contacts_async().
  tp_simple_client_factory_ensure_contact_by_id_async() is convenience API for
  tp_connection_dup_contact_by_id_async(). (Xavier)

• tp_simple/automatic_client_factory_new() now accept NULL TpDBusDaemon arg.
  tp_dbus_daemon_dup() will be used internaly in that case. (Xavier)

Fixes:

• Fixed possible case where TP_CONTACT_FEATURE_AVATAR_DATA does not get
  prepared. (Xavier)

• Fix Vala bindings build when srcdir is different from
  builddir. (fdo#49802, Colin)

telepathy-glib 0.19.0 (2012-05-09)
==================================

Dependencies:

• GLib ≥ 2.32, as released with GNOME 3.4, is now required.

Deprecations:

• Deprecations are now versioned. telepathy-glib users can define
  TP_VERSION_MIN_REQUIRED and/or TP_VERSION_MAX_ALLOWED, which work like the
  corresponding macros in GLib 2.32. (Simon)

• All TpChannel APIs using contact TpHandle have been deprecated in favor of
  their TpContact variants. Note that replacement APIs are only guaranteed to
  work with Connection Managers implementing spec >= 0.23.4. Any CMs using
  telepathy-glib's TpGroupMixin for implementing the channel's group iface
  are fine. (Xavier)

• TpTextMixin is (officially) deprecated, use TpMessageMixin. (Xavier)

• TpIntsetIter is deprecated, use TpIntsetFastIter. The typedefs
  TpIntSetIter and TpIntSetFastIter are also deprecated. (Simon)

• TP_ERRORS has officially been deprecated since 0.11; it now produces
  deprecation warnings too. (Simon)

• Reimplementation of the RequestHandles method is deprecated. (Simon)

• tp_connection_get_contacts_by_id is deprecated and replaced by
  tp_connection_dup_contact_by_id_async, for proper GAsyncResult API, and is
  now for single identifier to simplify most common use case.
  (fd.o #27687 and #30874, Xavier)

• tp_connection_get_contacts_by_handle() is deprecated with no replacement. It
  is deprecated to create a TpContact without knowing both its id and handle.
  (fd.o #27687 and #30874, Xavier)

• tp_connection_upgrade_contacts is deprecated and replaced by
  tp_connection_upgrade_contacts_async, for proper GAsyncResult API. Note that
  the connection must implement the Contacts interface to use this new API.
  (fd.o #27687 and #30874, Xavier)

• TP_CHANNEL_FEATURE_CHAT_STATES and its corresponding APIs are deprecated and
  replaced by similar API on TpTextChannel.

Enhancements:

• <telepathy-glib/telepathy-glib.h> now includes all non-generated code
  except proxy-subclass.h. Please use it instead of individual headers -
  direct inclusion of most individual headers will become an error in
  future versions, as was done for GLib. (Simon)

• A new meta-header, <telepathy-glib/telepathy-glib-dbus.h>, now includes
  all generated code. Please include it in any file that uses tp_svc_*,
  tp_cli_*, TP_IFACE_*, TP_HASH_TYPE_*, TP_STRUCT_TYPE_* or TP_ARRAY_TYPE_*.
  In telepathy-glib 1.0, it will become a separate pkg-config module. (Simon)

• Replace --disable-coding-style-checks and --disable-doc-checks with
  --disable-fatal-warnings. In addition to what the removed options did,
  it changes the default for --disable-Werror and turns off
  g-ir-scanner warnings. In future releases, it might control additional
  "warnings are treated as errors" options. Developers can also use
  --enable-fatal-warnings to force all of these on, even in official
  releases. (Simon)

• Add TpRoomList and TpRoomInfo (fd.o #30338, Guillaume)

• Add TpDebugClient (fd.o #23344; Will, Guillaume)

• Add tp_account_channel_request_new_text(),
  tp_account_channel_request_new_audio_call(),
  tp_account_channel_request_set_target_contact() etc., which do not require
  the caller to know D-Bus property names (fd.o #48780, Simon)

• Add tp_connection_dup_detailed_error_vardict(),
  tp_base_connection_disconnect_with_dbus_error_vardict(),
  tp_connection_manager_param_dup_default_variant(),
  tp_capabilities_dup_channel_classes_variant() which use/return GVariants
  instead of dbus-glib parameterized types (Simon, Guillaume)

• Add a simple PyGtk3 dialler to the examples, and optionally install
  the Python examples as well as the C ones (fd.o #48504, Simon)

• Add tp_contact_get_account() (Xavier)

• Improve documentation (Jonny, Simon, Xavier)

• Add "clean-for-new-branch" Makefile target which is more thorough than
  clean, but less thorough than distclean (in particular, it doesn't
  forget your ./configure options) (Simon)

• Add TpTLSCertificate, a TpProxy subclass representing a TLS
  certificate (fdo #30460, Guillaume)

• TpMessageMixin now has helpers to implement the ChatState interface.

Fixes:

• Make it safe to hold refs to a remaining GAsyncResult after returning
  to the main loop (fd.o #45554, Simon)

• When a TpProxy method call returns, which can occasionally be synchronous,
  use an idle to finish the corresponding GAsyncResult (fd.o #45514, Simon)

• Add TP_NUM_DBUS_ERRORS (etc.) to supersede NUM_TP_DBUS_ERRORS (etc.)
  (fd.o #46470, Simon)

• Make several methods returning a GStrv introspectable (fd.o #46471, Simon)

• Retry preparation of features that depended on a missing connection
  interface after the connection connects, in the hope that the interface has
  now become available (fd.o #42981, Guillaume)

• Interpret capabilities more strictly, avoiding falsely saying we support
  channel requests with fixed properties we don't understand (Xavier)

telepathy-glib 0.18.1 (2012-04-20)
==================================

The “King's Cross is unrecognisable” release.

Enhancements:

• Make various methods of the form get_foo() available to
  gobject-introspection (Xavier)

• Improve stream tube examples (Guillaume)

• Improve documentation (Xavier)

Fixes:

• Change the type of TpStreamTubeConnection::closed's argument from
  POINTER to ERROR so PyGI can bind it (Guillaume)

• Avoid TpCallChannel potentially returning a TpContact with no identifier
  (Xavier)

• Use the right macro to avoid post-2.30 GLib APIs (Guillaume)

• Fix warnings with newer gtk-doc and g-ir-scanner (fd.o #48592, fd.o #48363,
  fd.o #48620; Stef Walter, Alban Browaeys)

• Make various warnings non-fatal for this stable branch: GIR scanner warnings,
  documentation completeness, and deprecated functions (fd.o #48363, Simon)

• Don't hard-code use of a particular abstract socket in dbus-tube-chan test,
  fixing test failure on non-Linux (fd.o #48600, Simon)

telepathy-glib 0.18.0 (2012-04-02)
==================================

This is the start of a new stable branch. We encourage those shipping
GNOME 3.4 to track this stable branch.

Changes since 0.17.7:

• Support the DownloadAtConnection property in TpBaseContactList. (Alban)

• Add high-level API for TpDBusTubeChannel classes. (fd.o#29271,
  Guillaume and Will)

• Various improvements to the ContactList test suite. (Xavier)

Summary of particularly noteworthy changes since 0.16.x:

• GLib ≥ 2.30, dbus-glib ≥ 0.90, gobject-introspection ≥ 1.30, and
  valac ≥ 0.14 are now required.

• TpCallChannel, TpBaseCallChannel, and other Call-related high-level
  API has been added.

• High-level API to accept and provide file transfers has been added.

• Building on Android using 'androgenizer' is now supported.

telepathy-glib 0.17.7 (2012-03-22)
==================================

API additions:

• …get_local_sending on TpBaseMediaCallStream exposes the local
  sending state. (Olivier)

• TpCallStateReason structure now has a message string
  member. (Olivier)

Fixes:

• TpBaseMediaCallContent: be sure to update the local sending state on
  call acceptance. (Olivier)

• A few miscellaneous fixes to the Call1 code, including:
    • ensuring local sending state is updated on call acceptance,
    • ignoring sending/receiving failures while held,
    • only emitting STUNServersChanged when they have actually
      changed,
    • and not waiting for streams to start again after holding if they
      weren't sending before. (Olivier)

Enhancements:

• Use close_channels_async on the channel dispatch operation in the
  example approver. (Will)

telepathy-glib 0.17.6 (19/03/2012)
==================================

Requirements:

• GLib >= 2.30 is now required for telepathy-glib, and for code generated
  by tools copied from this version.

• dbus-glib >= 0.90 is now required.

Deprecations:

• tp_connection_is_ready(), tp_connection_call_when_ready,
  TpConnection:connection-ready and the TpChannel and TpConnectionManager
  equivalents are deprecated. Use tp_proxy_prepare_async() and
  tp_proxy_is_ready() instead. For connections, remember to ask for
  TP_CONNECTION_FEATURE_CONNECTED if required.

Enhancements:

• TpCapabilities: add API to check for Call and FileTransfer support
  (Xavier)

• Use GObject's FFI-based generic marshaller instead of generating our own
  marshallers (fd.o #46523, Simon)

• Add tp_list_connection_managers_async, tp_protocol_dup_params,
  tp_protocol_dup_param, tp_protocol_borrow_params,
  tp_connection_manager_dup_protocols (Simon)

• Add accessors for TpAccount properties, parameters and storage identifier
  represented as a GVariant (fd.o #30422, Simon)

• TpCallChannel: add API to put the call on hold. (Olivier)

• TpCallContentMediaDescription now implements the RTPHeaderExtensions,
  RTCPFeedback and RTCPExtendedReports interfaces. (Olivier)

Fixes:

• Don't change the direction of Call streams because of a Hold (Olivier)

• Use the right error when rejecting incompatible codecs (Olivier)

• Reject local updates to a media description while an offer is pending
  (Olivier)

• Avoid forward-declaring Call classes, fixing compilation on clang,
  older gcc, and other compilers not targeting C1x (Simon)

• Fix namespaces in example_call.manager (George)

• Produce self-contained header files from glib-interfaces-gen.py
  (fd.o #46835, Simon)

• Correctly implement and document tp_call_channel_has_dtmf() (Xavier)

• Fix various build failures in out-of-tree or parallel builds,
  and don't rebuild everything whenever the documentation changes
  (fd.o #36398, Simon)

• Improve test coverage for the connection manager test (fd.o #46358, Simon)

• TpCallChannel::state-changed is properly annotate (fdo.o #47410 Guillaume)

telepathy-glib 0.17.5 (2012-02-20)
==================================

The “I have no privates but I have a heart!” release.

Enhancements:

• telepathy-spec 0.25.2:
  · the Call1 family of interfaces
  · Conn.I.Addressing1
  · Chan.I.CaptchaAuthentication1
  · Account.Supersedes

• Add TpCallChannel, TpBaseCallChannel and other Call-related high-level API.
  (A team effort involving Olivier, Xavier, Danielle, Sjoerd, Will, Nicolas,
  Jonny, David and possibly others)

• tp_account_manager_get_most_available_presence() now returns
  (AVAILABLE, "available, "") if the only connected accounts does not implement
  SimplePresence. (Guillaume)

• Add tp_base_channel_get_self_handle(). (Xavier)

• TpBaseChannel now has a virtual get_interfaces() method. (Danielle)

• tp_connection_disconnect_async: high level API to disconnect a
  TpConnection. (Simon)

• tp_unix_connection_receive_credentials_with_byte() and
  tp_unix_connection_send_credentials_with_byte() now have async equivalents.
  (Xavier)

• Produce DLL files when compiled for Windows. (Siraj)

Fixes:

• fd.o #45554: fix use-after-free if various async results are kept until
  after the callback has returned, which is considered valid. (Simon)

• tp_account_manager_get_most_available_presence() now returns ("offline, "")
  as status and message if no account is connected, as stated in the doc,
  instead of (NULL, NULL). (Guillaume)

• TpChannel: fix a crash when preparing contacts. (Xavier)

• fdo.o #45982: fix a crash in TpBaseContactList when using RenameGroup()'s
  fallback code. (Guillaume)

telepathy-glib 0.17.4 (2011-12-19)
==================================

Fixes:

• Set the right source on tp_account_set_uri_scheme_association_async's
  GAsyncResult. (Guillaume)

• fdo#43755: Fix a crash when participating in a XMPP MUC containing
members with an unknown real JID. (Guillaume)

telepathy-glib 0.17.3 (2011-11-28)
==================================

       I DON'T ALWAYS
         WRITE NEWS

          ////////
         / _   _  +
         / • . •  |
         | ,~~~,  |
         \ #---# /
          #######

     BUT WHEN I DO, THE
    COMMUNITY DEVOURS IT

Dependencies:

• gobject-introspection ≥ 1.30
• Valac ≥ 0.14.0 is now required for the Vala bindings.

Fixes:

• Fix a crash in TpSimplePasswordManager (Mikhail)

Enhancements:

• Add high level API to check if a connection supports settings aliases
  on contacts. (Guillaume)

telepathy-glib 0.17.2 (2011-11-23)
==================================

The “maple syrup bread” release.

Enhancements:

• TpBaseProtocol now supports the freshly-undrafted
  Protocol.Interface.Addressing from spec 0.25.1. (Eitan, Andre)

• Building on Android using 'androgenizer' is now supported (Derek
  Foreman)

Fixes:

• Speculatively replace _free/_destroy with _unref everywhere, and add a
  coding style check. (Xavier)

• Ensure GSocketConnection objects are properly closed and freed for
  incoming TpFileTransferChannels. (Jonathan)

• Many documentation nits have been fixed. (Jonathan)

• Building the GObject-Introspection repository now works reliably when
  using `make -j`. (Will)

telepathy-glib 0.17.1 (2011-11-15)
==================================

Dependencies:

• gobject-introspection ≥ 1.30 is now required.

• valac ≥ 0.14 is now required.

Enhancements:

• telepathy-glib now generates code for spec version 0.25.0.

• fdo #39188: add high level API to accept and provide file transfers.
  (morten, jonny)

telepathy-glib 0.17.0 (2011-11-08)
==================================

The “Perrier-cassis” release. This is the start of a new development branch
that will lead to 0.18 in roughly 6 months. This release contains all the
fixes from 0.16.2.

Enhancements:

• fdo #41801: add high level API to block/unblock contacts. (Guillaume)

• fdo #42546: add TP_CONNECTION_FEATURE_CONTACT_LIST_PROPERTIES a feature
  preparing ContactList's properties without preparing the contact list
  itself. (Guillaume)

• fdo #42503: the TpChannelDispatchOperation passed to
  TpBaseClientClassObserveChannelsImpl is now prepared. (Xavier)

• fdo #41455: it's now possible to install tp-glib's test suite. (albanc)

telepathy-glib 0.16.2 (2011-11-08)
==================================

The “destructive substance” release.

Fixes:

• Improve documentation of the TpProxy::invalidated signal. (Danni)

• fdo#42305: TpGroupMixin: always set the Members_Changed_Detailed
  flag. (Guillaume)

• fdo#42670: fix a crash when preparing TP_CHANNEL_FEATURE_CONTACTS
  on a channel containing contacts without known owners. (Guillaume)

telepathy-glib 0.16.1 (2011-10-24)
==================================

Fixes:

• fd.o#42063: circular introspection dependency between connection and
  contacts. (Sjoerd)

• fd.o#42049: TpBaseContactList now implements
  TP_TOKEN_CONNECTION_INTERFACE_CONTACT_BLOCKING_BLOCKED. (Guillaume)

• fd.o#41928: Don't crash if the owner of some members of a TpChannel are
  unknown. (Guillaume)

• fd.o#41929: Don't crash if a TpTextChannel receives a message not having a
  sender. (Guillaume)

telepathy-glib 0.16.0 (2011-10-14)
==================================

The “irrelevant details” release. This is the start of a new stable
branch (which will be spoken of by robot historians of the future in
hushed, reverent voices). We encourage those shipping Gnome 3.2 to track
this stable branch.

Fixes since 0.15.9:

• fd.o#41729: TpChannel now trusts the ChannelType property included in
  the dictionary of immutable properties passed to its constructor.
  Practically speaking, this resolves a race condition where channels
  would sometimes fail to prepare (and hence, conversation windows would
  fail to open). (Guillaume)

• fd.o#41714: TpAccount:normalized-name now refers to XMPP JIDs and ICQ
  UINs, to give examples of what its value means.

Summary of particularly noteworthy changes since 0.14.x:

• telepathy-glib depends on GLib 2.28.0;

• Applications can now define their own features on TpProxy subclasses,
  and can create their own proxy factory classes to automatically create
  their own proxy subclasses.

• There's a bunch of new API to make it easier to work with channels,
  particularly text channels and TpContacts affiliated with channels.

• There is now API on TpConnection for managing the user's contact list,
  at long last! Note that this currently doesn't work with
  telepathy-butterfly and telepathy-sunshine, since they do not
  implement the new, dramatically simpler ContactList D-Bus API.

telepathy-glib 0.15.9 (2011-10-12)
==================================

The “important lessons of the past” release. This will teach me to make
releases without checking nothing more's been merged overnight.

Fixes:

. fd.o#41697: unknown handle owners in chat rooms no longer crashes
  TpChannel. (Guillaume)

telepathy-glib 0.15.8 (2011-10-12)
==================================

The “fretting about the now” release.

Enhancements:

• telepathy-glib now generates code for spec version 0.24.0, including
  the Room, Subject and RoomConfig interfaces. It also includes a new
  TpBaseRoomConfig object, which vaguely resembles the good bits of
  TpPropertiesMixin. (The latter is not deprecated yet, but just you
  wait…)

• It is now possible to set D-Bus properties as if in response to a call
  to Set() on the bus, using tp_dbus_properties_mixin_set(). This
  complements the existing tp_dbus_properties_mixin_get() method, and
  makes it possible to fix fd.o#32416.

Fixes:

• fd.o#41470: crash in some situations when using
  TP_CHANNEL_FEATURE_CONTACTS. This was specifically triggered by
  test-cli-group as of the last release.

telepathy-glib 0.15.7 (2011-10-04)
==================================

Fixes:

• fd.o#40555: Memory leaks in TpConnection and protocol.c (Vivek)

• Memory leak if the debug message cache is disabled (Vivek)

• fd.o#38060: Fix a crash in TpMessageMixin, triggered by delivery
  reports. (Danni)

• fd.o#38997: Cope beter if UNIX sockets are not supported. (Guillaume)

• fd.o#40523: Connection Manager crash when a client acks the same
  message twice. (Will)

• fd.o#41414: Make sure tp_connection_upgrade_contacts() is no-op if all
  features are already prepared (Xavier).

• fd.o#41368: Fix introspection by reverting
  48998822d5e9575af822c1936b35be514dc2401b. The order in which files are given
  to gi-scanner matters (Xavier).

• fd.o#41435: Ensure sent messages have a sender even with butterfly, which does
  not set "message-sender" (Xavier).

telepathy-glib 0.15.6 (2011-09-30)
==================================

Enhancements:

• New JavaScript (gjs) code example to demonstrate GObject-Introspection
  of the API.

• TpProtocol: new API to get avatars requirements.

• Factory features are now prepared on various contacts:
    • TpConnection's self contact;
    • TpChannel's target, initiator and self contacts;
    • TpChannel's group memebers;
    • TpTextChannel's message sender; and
    • TpStreamTubeChannel's connection contact.

• New object TpDBusTubeChannel to represent a D-Bus tube channel.
  Its API is still incomplete.

• Group the MembersChanged signals for the initial roster (fd.o#40933). This
  improve login performance.

• Spec upgraded to 0.23.4 except for draft call interfaces. Mixins implement the
  new additions.

• TpDBusPropertiesMixin now has a method for emitting the standard
  PropertiesChanged signal; in conjunction with the code generator, it respects
  the EmitsChangedSignal annotation. Note that the signal is not emitted
  automatically.

Fixes:

• Python examples now works with latest pygobject (stop using static bindings).

Deprecations:

• tp_account_prepare_{async,finish} replaced by tp_proxy_prepare_{async,finish}
• tp_account_manager_prepare_{async,finish} replaced by
  tp_proxy_prepare_{async,finish}

telepathy-glib 0.15.5 (2011-12-17)
==================================

The “feel. experience. know” release. This is a pretty big release,
containing over ten thousand lines of changes, which should make various
aspects of application development easier. Fasten your seatbelts.

Dependencies:

• Valac ≥0.11.2 is now required for the Vala bindings.

• gtk-doc ≥1.17 is now required if you want to build documentation.
  Relatedly, building documentation for releases works again.
  (fd.o#39666)

Enhancements:

• A new pair of classes, TpSimpleClientFactory and
  TpAutomaticClientFactory, have been added, which make it much easier
  for application to provide custom subclasses of specific channel
  types and to request that certain features always be prepared on proxy
  objects before they are given to the application. These replace the
  prevous TpBasicProxyFactory and TpAutomaticProxyFactory classes, which
  are now deprecated. (Xavier)

• A new TpAccount feature, TP_ACCOUNT_FEATURE_CONNECTION, has been
  added, to ask TpAccount to prepare TpConnection objects before
  announcing them. Relatedly, TpAccountManager no longer signals new
  TpAccount objects until they are prepared. (Xavier)

• TpConnection now has API for managing the user's contact list! Hooray.
  This only works with CMs that implement the new ContactList API; dear
  Python CM folks, please do this and we can move forward with grace and
  speed.

• fd.o#26516: Add tp_debug_sender_set_timestamps. (Jonny)

• A metric tonne of new methods were added for joining chat rooms and
  managing members. (Xavier)

• TpFileTransferChannel, a high-level API for file transfer
  channels, was added. (Morten Mjelva)

• fd.o#38061: tp_cm_message_set_message() was added to complement
  tp_cm_message_take_message(). (Danni)

Fixes:

• fd.o#38060: fix a crash caused by an off-by-one error when
  constructing the SendError signal. (Danni)

• fd.o#38997: cope more gracefully if UNIX sockets are unsupported,
  which should improve Windows portability. (Guillaume)

• fd.o#39377: TpContact no longer erroneously re-prepares many features
  if existing contacts are re-requested. (Will)

• fd.o#27855: TpChannelManagers now have access to the original TargetID
  specified in the channel request, if any. Previously, it was
  transformed into a TargetHandle and the TargetID was removed from the
  dictionary.  The TargetHandle is still synthesised (validating the
  TargetID in the process) but the ID is left intact. (Will)

telepathy-glib 0.15.4 (2011-07-12)
==================================

The “A-level magical theory” release.

This new release in the 0.15 development cycle fixes a bug introduced in 0.15.0
which could lead to you losing incoming messages. We strongly advise upgrading
to this release if you're already using 0.15.0 or later. 0.14.x releases are
not affected by this bug.

Fixes:

• TpTextChannel never finishes preparing if there are pending messages with no
  message-sender-id (fd.o#39172, Will, ably assisted by Jonny)

• Cope better if UNIX sockets are unsupported (fd.o#38997, Guillaume)

• Fix some compiler warnings (Xavier)

telepathy-glib 0.15.3 (2011-07-08)
==================================

This new release in the 0.15 development cycle contains all the fixes
released in 0.14.9 except the TpChannelIface:handle-type change which has
*not* been reverted in 0.15.3. Connection managers should be fixed to work
properly with the new default value.

Enhancements:

• TpTextChannel: tp_text_channel_ack_all_pending_messages_async: convenient
  function to easily ack all the pending messages. (fdo #38559 Guillaume)

• TpChannelRequest: add properties and accessors for Account, UserActionTime
  and PreferredHandler preferred-handler and user-action-time.
  (fdo #38605 Guillaume)

• TpAccountChannelRequest and TpBaseClient: API to use the
  DelegateToPreferredHandler hint. (Guillaume)

• TpMessage: tp_message_get_pending_message_id: convenient function to get the
  pending-message-id of the message. (Guillaume)

• TpChannel: Annotate tp_channe_group_ methods. (Guillaume)

Fixes:

• Fix some set-but-not-used warnings with --disable-debug. (Will)

• Honor NOCONFIGURE for compatibility with gnome-autogen.sh. (Colin)

• Fix tests failing on some arch. (Will, Adam, Emilio)

• Fix memory leaks in TpConnection. (fdo #38944 Siraj)

telepathy-glib 0.15.2 (2011-06-21)
==================================

The “I'm a PC” release.

This new release in the 0.15 development cycle contains all the fixes
released in 0.14.8.

Enhancements:

• Reduce debug spam. (Will)

• tp_channel_destroy_async(): high level API to Destroy a TpChannel.

• tp_channel_dispatch_operation_{leave,destroy}_channels_async: convenience API to
  claim a ChannelDispatchOperation and leave/destroy all its channels.
  (fdo #28015 Guillaume)

• TpChannel: high level API for password protected channels; the TP_CHANNEL_FEATURE_PASSWORD
  feature is automatically prepared by TpAutomaticProxyFactory.
  (fdo #37360 Guillaume)

• TpConnection high level avatars API and TpChannel high level group API are
  now introspected. (Xavier)

Fixes:

• TpChannelIface: set TP_UNKNOWN_HANDLE_TYPE as default
  handle type. (fd.o#38524 Guillaume)

telepathy-glib 0.15.1 (2011-05-30)
==================================

The "Bugzilla etiquette" release.

This new release in the 0.15 development cycle contains all the fixes
released in 0.14.7.

Fixes:

• tp_text_channel_set_chat_state_finish: check the right
  source tag. (Guillaume)

• TpConnection: set the self handle to something sane instead of
  leaving uninitialized. (Jonny)

• Fix a race in tp_channel_dispatch_operation_claim_with_async()
  (fdo #37280 Guillaume)

Enhancements:

• TpTextChannel: high level API for SMS; the TP_TEXT_CHANNEL_FEATURE_SMS
  feature is automatically prepared by TpAutomaticProxyFactory.
  (fdo #37358 Guillaume)

• TpConnection: high level API for Balance.
  (fdo #36334 Emilio, Danielle, Guillaume)

• tp_channel_dispatch_operation_close_channels_async: convenient API to
  claim a ChannelDispatchOperation and close all its channels.
  (fdo #28015 Guillaume)

• TpBaseContactList: add ContactBlocking support (fdo #35331 Will)

telepathy-glib 0.15.0 (2011-05-17)
==================================

This first release in the 0.15 development cycle contains all the fixes
released in 0.14.6.

Dependencies:

• GLib 2.28.0

Enhancements:

• Update to spec 0.23.2 (Guillaume):
  · Generated code for Channel.Interface.SMS.GetSMSLength()
  · Generated code for ChannelDispatcher.DelegateChannels() and
    ChannelDispatcher.PresentChannel()

• tp_channel_dispatch_operation_claim_with_async() replacing
  tp_channel_dispatch_operation_claim_async() (fdo #36490 Guillaume)

• TpProxyFeature is now part of the API allowing users to define their own
  features (fdo #31583 Guillaume)

• tp_base_client_delegate_channels_{async,finish} and
  add tp_channel_dispatcher_present_channel_{async,finish}: high level
  API to delegate and present channels (fdo #34610 Guillaume)

• TpChannelDispatcher is now exported in the GIR file and so can be used using
  gobject-introspection (Guillaume)

telepathy-glib 0.14.6 (2011-05-16)
==================================

Fixes:

• tp_dbus_daemon_watch_name_owner leaked a DBusMessage (fledermaus)
• tp_dbus_daemon_list[_activatable]_names leaked a DBusMessage (fledermaus)
• tp_base_connection_change_status: delay side-effects until all
  preconditions are checked (Simon)
• TpGroupMixin: correctly use contact-ids, not member-ids (Will)
• TpBaseContactList: fix leak of source object (Mike)

telepathy-glib 0.14.5 (2011-04-20)
==================================

The “seven years wasn't strange” release.

Enhancements:

• Update to spec 0.22.2, generating code for the Balance.ManageCreditURI
  and SimplePresence.MaximumStatusMessageLength properties.

• tp_base_client_is_handling_channel(), which does what it says on the
  tin. (cassidy)

• TpPresenceMixin now supports the MaximumStatusMessageLength class.
  (andrunko)

Fixes:

• The documentation now builds correctly with gtk-doc 1.16 and newer.
  (wjt)

• The test suite now passes on systems with glib-networking installed.
  (wjt)

telepathy-glib 0.14.4 (2011-04-15)
==================================

Enhancements:

• fd.o#27459: TpConnection now avoids a bunch of redundant D-Bus method 
  calls when preparing the CORE feature with recent services, lowering
  latency (oggis)

• tp_proxy_add_interfaces() for adding discovered interfaces from TpProxy
  subclasses (oggis, cassidy)

• tp_base_protocol_get_immutable_properties() now fills the Proto.I.Avatars
  properties (cassidy)

Fixes:

• fd.o#36134 - TpProtocol claims it doesn't support any extra iface (cassidy)

telepathy-glib 0.14.3 (2011-03-31)
==================================

The “where the wind blows” release. This release flatly contradicts the
statement that the previous stable release would be the last to add API,
by adding API from version 0.22.1 of the specification plus a new
utility function. This time, we shall merely claim that no major API
additions will be made on this stable branch, and once a 0.15.x release
is made no further API additions will be made on 0.14.x at all.

Enhancements:

• Update to spec 0.22.1 (Will):
  · Generated code for new StreamHandler methods and signals and types;
  · TP_ERROR_INSUFFICIENT_BALANCE.
• tp_g_ptr_array_extend() for concatenating two GPtrArrays. (Jonny)

telepathy-glib 0.14.2 (never)
=============================

There is no telepathy-glib 0.14.2.

telepathy-glib 0.14.1 (2011-03-22)
==================================

The “work by the windows” release. This release adds API from version
0.22 of the Telepathy specification which was accidentally omitted from
the previous release. Further releases on this stable branch should not
add any further API.

Enhancements:

• Update to spec 0.22.0 (Guillaume)
  · TP_PROP_CHANNEL_TYPE_SERVER_TLS_CONNECTION_REFERENCE_IDENTITIES
  · generate code for Connection.I.ContactBlocking

telepathy-glib 0.14.0 (2011-03-21)
==================================

The “welded in gridlock” release, starting a 0.14.x stable branch. This
branch corresponds to the 0.22 stable branch of the specification.

Highlights since 0.12.0
-----------------------

• TpBaseContactList, a base class for the ContactList and ContactGroups
  connection interfaces, as well as old-style ContactList channels.
  While this helper class supports the old 'deny' channel for blocked
  contact, it unfortunately does not implement the new ContactBlocking
  interface yet.

• CMs implemented using telepathy-glib will now have immortal handles.

• TpContact supports ClientTypes.

• TpClientChannelFactory, TpAutomaticProxyFactory and
  TpBasicProxyFactory have been added to help applications construct
  particular TpChannel subclasses for channels of different types, with
  particular features prepared.

• TpStreamTubeChannel, a high-level client API for stream tubes, has
  been added.

• TpBaseProtocol supports the Avatars and Presence interfaces.

• tp_get_bus() is deprecated (again). Please use tp_dbus_daemon_dup(),
  followed by tp_dbus_daemon_register_object() if that's what you're
  using it for, or tp_proxy_get_dbus_connection() if you really need a
  DBusGConnection.

• TpClientMessage and TpSignalledMessage, client-side representations
  for multipart messages used by TpTextChannel, have been added.

• The TpContactSearch object has been added. It represents ongoing
  searches for contacts.

• Code is now generated to emit and listen for the PropertiesChanged
  signal on org.freedesktop.DBus.Properties. Note that
  TpDBusPropertiesMixin does not emit this signal on it own, nor does
  TpProxy listen for it of its own accord.

• Previously, tp_clear_object (NULL), tp_clear_boxed (type, NULL) and
  tp_clear_pointer (NULL) were no-ops. However, this behaviour was not
  very useful—these functions are always called as
  tp_clear_object (&priv->foo) in practice—and triggered compiler
  warnings (because these are actually implemented as macros). Thus,
  this usage is no longer supported. (This should not affect anything
  except contrived code, but CM authors may wish to check.)

Fixes since 0.13.18
-------------------

• The error handling code paths when looking up the senders of incoming
  messages have been fixed. This issue led to TpSignalledMessages not
  specifying a sender with CMs that omit sender-id, like telepathy-idle.
  (stormer)

telepathy-glib 0.13.18 (2011-03-15)
===================================

The “chilled coconut-chocolate security blanket” release.

Changes:

• Previously, tp_clear_object (NULL), tp_clear_boxed (type, NULL) and
  tp_clear_pointer (NULL) were no-ops. However, this behaviour was not
  very useful—these functions are always called as
  tp_clear_object (&priv->foo) in practice—and triggered compiler
  warnings (because these are actually implemented as macros). Thus,
  this usage is no longer supported. (This should not affect anything
  except contrived code, but CM authors may wish to check.)

Fixes:

• TpCMMessage is no longer included in the .gir file. This class is only
  useful in CMs; we only support client-side API in the .gir file.
  (Benjamin Otte)

• Correctly include TpBasePasswordChannel documentation. (Danielle)

• Fix GCC 4.6 and Clang analyzer warnings (Dan Winship, Will)

telepathy-glib 0.13.17 (2011-03-09)
===================================

Enhancements:

• Update to spec 0.21.11 (Sjoerd)
  · TP_ERROR_STR_SOFTWARE_UPGRADE_REQUIRED
  · TP_ERROR_STR_EMERGENCY_CALLS_NOT_SUPPORTED

telepathy-glib 0.13.16 (2011-03-07)
===================================

Enhancements:

• Update to spec 0.21.11 (Guillaume)
  · TP_PROP_CLIENT_OBSERVER_DELAY_APPROVERS

• Two new functions, tp_connection_get_connection_manager_name() and
  tp_connection_get_protocol_name(), allow you to grab these properties
  from a TpConnection without having to drive
  tp_connection_parse_object_path() yourself. (sjokkis)

• GBinding utilities for connection-status on TpAccount and TpConnection:
  tp_connection_bind_connection_status_to_property() and
  tp_account_bind_connection_status_to_property(). (Danielle)

• TpTextChannel now has a "message-types" property and accessor.
  It also gained tp_text_channel_supports_message_type() a convenient function
  to check if a specific message type is supported. (Guillaume)

• TpContactSearch: only close channels if there was an error (Emilio)

• TpBaseClient gained tp_base_client_set_observer_delay_approvers() which can
  be used to indicate that an Observer has to block Approvers.

Fixes:

• Various crashes fixed in TpBaseContactList. (Marco)

telepathy-glib 0.13.15 (2011-02-24)
===================================

The “Continents all made of clay” release.

Enhancements:

• Update to spec 0.21.10 (Guillaume)
  · TP_PROP_CHANNEL_INTERFACE_SASL_AUTHENTICATION_MAY_SAVE_RESPONSE

• tp_utf8_make_valid(): an UTF-8 string validation function. (Senko)

• SimplePasswordManager now has API to let the CM create its own
  SASL channel. (Jonner)

• TpBasePasswordChannel: a base class to implement SASL channels. (Jonner)

telepathy-glib 0.13.14 (2011-02-23)
===================================

The “Things that London never saw” release.

Enhancements:

• TpContact now supports modifying the contact's groups on connection
  managers which implement Connection.Interface.ContactGroups. (Zdra)

• It's now possible to get a list of TpChannelRequest objects from a
  TpObserveChannelsContext or TpHandleChannelsContext; adding hints to
  channel requests, and retrieving them again, is supported with
  telepathy-mission-control ≥ 5.7.2. (cassidy)

• tp_account_channel_request_create_and_observe_channel_async() and
  friends, analogous to
  tp_account_channel_request_create_and_handle_channel_async() but for
  Observers rather than Handlers, have been added. (cassidy)

• Code is now generated to emit and listen for the PropertiesChanged
  signal on org.freedesktop.DBus.Properties. Note that
  TpDBusPropertiesMixin does not emit this signal on it own, nor does
  TpProxy listen for it of its own accord. (danni)

• tp_capabilities_supports_room_list(), a convenient way to check
  whether a connection supports listing chat rooms, has been added.
  (cassidy)

Fixes:

• Including telepathy-glib/protocol.h now correctly includes generated
  client-side functions. (danni)

telepathy-glib 0.13.13 (2011-02-09)
===================================

The “Duckworth Lewis” release.

Enhancements:

• Many doc fixes, including: TpBaseClientClass is now included;
  INCOMING_MESSAGES is now explained. (wjt)

• Compiler flags reordered (clang is order-sensitive) to allow
  static analysis. (wjt)

• Account Channel Requests now give you access to the originating
  TpChannelRequest. (cassidy)

• The speculative debug cache may now be disabled at compile time.
  tp_debug_sender_add_message_vprintf and
  tp_debug_sender_add_message_printf added to allow callers who care
  about optimisation to reduce debug overhead. (fledermaus)

telepathy-glib 0.13.12 (2011-02-01)
===================================

The “You look good by siren light” release.

Enhancements:

• TpContact now tracks incoming and outgoing presence subscriptions,
  albeit only if the connection implements the new
  Connection.Interface.ContactList API. (Xavier)

• Code is generated for new API added in telepathy-spec version 0.21.9;
  specifically, the FileTransfer.URI property is supported. (Guillaume)

• TpPresenceMixinStatusAvailableFunc, which has confused many a CM
  author, now has clearer documentation. (Will)

Fixes:

• We no longer accidentally depend on GLib 2.28. (Will)

• tests/contact-search-result no longer fails on x86. (Will)

telepathy-glib 0.13.11 (2011-01-27)
===================================

The “and, erm … I own an M-16 fully-automatic ground assault rifle” release.

Enhancements:

• fd.o#32053: The TpContactSearch object has been added. It represents
  ongoing searches for contacts. (pochu)

Fixes:

• fd.o#32551: tp_base_client_set_observer_recover now works with all
  possible gboolean arguments! (jonny)

• tp_debug_timestamped_log_handler will now print the message and not
  just the time, which was broken over a year ago. (jonny)

• TpProtocol will now consider a manager file's AuthenticationTypes
  value. (jonny)

telepathy-glib 0.13.10 (2010-12-20)
===================================

The “I own a 9 millimetre, a 357, a 45 handgun, a 38 special” release.

Enhancements:

• New TpMessage API: tp_message_get_message_type, tp_cm_message_new_text (smcv)

• fd.o #32411: warn and do nothing if unsupported flags are passed to
  tp_g_signal_connect_object (smcv)

• Updated to spec 0.21.8.
  · The ContactList.ContactsChangedWithID signal was added. It's automatically
    emitted by TpBaseContactList, so CMs don't need to make any changes to take
    advantage of it. (smcv)

Fixes:

• tp_account_manager_ensure_account() no longer criticals if you pass it a
  malformed account path. (wjt)

• TpBaseClient will now return (an error) from ObserveChannels if an invalid
  connection path is passed to it by the Channel Dispatcher. (wjt)

• fd.o#32184: Connection bus names are no longer erroneously released while
  connections are still open. This was a regression in 0.13.5. (wjt)

• fd.o #32423: Preparing TpAccount features when the CORE feature is already
  prepared now works (smcv)

• fd.o #32391: correctly deal with removing name owner watches during
  dispatch of their callbacks (wjt, smcv)

• Documentation improvements (smcv)

• TpBaseClient no longer breaks if HandleChannels is called more than once for
  the same channel. This was a regression introduced by the leak fix in the
  previous release (sjoerd)

telepathy-glib 0.13.9 (2010-12-10)
==================================

The "please mind the gap between the table and the table" release.

This release includes all the bugfixes from version 0.12.6.

Deprecations:

• tp_message_new (replace with tp_cm_message_new)
• tp_message_ref_handle (no longer needed)
• tp_message_set_handle (use tp_cm_message_set_sender for the only supported
  handle in a message)
• tp_message_take_message (replace with tp_cm_message_take_message)

Enhancements:

• tp_account_get_path_suffix: new function to get the varying suffix of an
  account's object path (wjt)

• TpAccountManager: document which TpAccount objects are prepared (wjt)

• tp_connection_dup_contact_if_possible: new function to make
  TpContact objects synchronously in some situations (smcv)

• convert TpMessage into a GObject, with subclasses for use in CMs
  (TpCMMessage) and clients (TpClientMessage, TpSignalledMessage) (cassidy)

• add TpConnection:self-contact, a TpContact for the self-handle (smcv)

Fixes:

• In TpSimplePasswordManager, clear the pointer to the channel when it's closed
  (jonny)

• fd.o #32116: don't leak LegacyProtocol object references when a
  TpBaseConnectionManager is registered (smcv)

• Fix an unlikely crash in which a TpBaseConnection outlives its
  TpBaseConnectionManager (smcv)

• Documentation improvements (cassidy)

• fd.o #32191: when tp_connection_get_contacts_by_handle would return
  contacts that already exist, make sure they have the desired features (smcv)

• Fix memory leaks in TpAccount and TpDynamicHandleRepo introduced in 0.13.8
  (cassidy)

• fd.o #32222: fix a leak of TpChannel objects in TpBaseClient, document that
  Handlers are responsible for closing their channels, warn if channels are
  still handled when a Handler is disposed, and close channels in some
  regression tests (Zdra)

telepathy-glib 0.13.8 (2010-12-01)
==================================

The "many of my best conversations are when un-agonized" release.

Deprecations:

• fd.o #24114: tp_get_bus() is deprecated (again). Please use
  tp_dbus_daemon_dup(), followed by tp_dbus_daemon_register_object() if that's
  what you're using it for, or tp_proxy_get_dbus_connection() if you really
  need a DBusGConnection. (smcv)

Changes:

• fd.o #23155: handles now persist until the TpBaseConnection disconnects,
  and most of the reference-counting machinery has been removed (smcv)

• fd.o #31997: in the ContactList channels produced by TpBaseContactList,
  AddMembers, RemoveMembers etc. don't return until the implementation
  reports success or failure (smcv)

Enhancements:

• Update to spec 0.21.6 (smcv)
  · Connection.HasImmortalHandles property

• fd.o #31900: add TpSimplePasswordManager (jonny)

• fd.o #32004: add tp_account_get_automatic_presence,
  tp_account_get_normalized_name, tp_account_set_automatic_presence_async
  (smcv)

• fd.o #31918: add convenience API for Account.I.Addressing (smcv)

Fixes:

• return a zero-terminated array of features from
  tp_client_channel_factory_dup_channel_features (cassidy)

• fd.o #32004: emit GObject::notify for TpAccount::requested-* (smcv)

telepathy-glib 0.13.7 (2010-11-25)
==================================

The "moustache pattern released under a Creative Commons licence" release.

This release includes all bugfixes from 0.12.5.

Enhancements:

• Update to spec 0.21.5 (smcv)
  · Conn.I.PowerSaving
  · Chan.T.ServerAuthentication, Chan.I.SASLAuthentication, Chan.I.Securable
  · Account.I.Addressing
  · Protocol.I.Avatars
  · enhanced ChannelDispatcher and ChannelRequest API with "request hints"
  · new errors: CONFUSED, SERVER_CONFUSED
  · new property: Messages.MessageTypes
  · TP_CONTACT_INFO_FIELD_FLAG_OVERWRITTEN_BY_NICKNAME

• fd.o #31686: add Protocol.I.Avatars support to TpBaseProtocol (eeejay)

• implement the MessageTypes property in the TpMessagesMixin (smcv)

• use G_N_ELEMENTS more (smcv)

Fixes:

• use the right getter for TpAccountChannelRequest:request (smcv)

telepathy-glib 0.13.6 (2010-11-17)
==================================

The "please stop trying to find me on Wikipedia" release.

This release includes all the fixes from 0.12.4.

API changes:

• Pointers to a GObject implementing TP_TYPE_CLIENT_CHANNEL_FACTORY are now
  referred to as having type TpClientChannelFactory*, rather than misusing
  TpClientChannelFactoryInterface*. The ABI has not changed.

Enhancements:

• Return the reffed handle from tp_handle_ref() (jonny)

Fixes:

• fd.o #31473: force the namespace TelepathyGLib for the g-i-derived Vala
  bindings (treitter)

• fd.o #31581: don't modify a const array in
  tp_group_mixin_remove_members_with_reason (smcv)

• fd.o #31631: set a TpBaseClient's TpClientChannelFactory correctly (cassidy)

• fd.o #31631: fix confusion between TpClientChannelFactory and
  TpClientChannelFactoryIface (smcv)

• Run the stream tube IPv6 tests again, if ::1 is assigned to an
  interface (smcv)

telepathy-glib 0.13.5 (2010-11-05)
==================================

The "gunpowder, treason and plot" release.

Enhancements:

• fd.o #30088: add support for Protocol.I.Presence to TpBaseProtocol
  (fledermaus, smcv)

Fixes:

• fd.o #10613: release connections' object paths before their bus
  names, and do both sooner (smcv)

• fd.o #31377: fix a race condition in the connection-interests test that
  sometimes caused it to fail or segfault, and similar races (not seen
  in practice) in two other tests (smcv)

telepathy-glib 0.13.4 (2010-11-03)
==================================

The "request_module: runaway loop modprobe binfmt-464c" release.

This release includes all the fixes from 0.12.3.

Enhancements:

• Update to spec 0.21.4 (smcv)
  - fd.o #31215: fix incorrect namespace for MailNotification
  - add bindings for NewActiveTransportPair

Fixes:

• fd.o #31321: don't crash if the TpAccountManager is disposed while
  an account from tp_account_manager_ensure_account is preparing (cassidy)

• fd.o #31198: avoid some C99 features not supported by MSVC 9
  (Thomas Fluueli, smcv)

• fd.o #31291: add pkg-config and C header information to GIR for the
  benefit of future Vala versions (Evan Nemerson)

• In the echo2 example CM, advertise Messages' immutable properties (cassidy)

• In TpBaseChannel, don't unref handles we didn't ref (jonny)

telepathy-glib 0.13.3 (2010-10-26)
==================================

The "reminds me of daf's random dbus type generator" release.

This release includes all the fixes from version 0.12.2.

Deprecations:

• <telepathy-glib/debug-ansi.h> is now deprecated, and the Group and
  Properties mixins no longer output brightly-coloured logs

Enhancements:

• fd.o #31102: update to spec 0.21.3 (smcv):
  - add TP_ERROR_PICKED_UP_ELSEWHERE
  - generate code for Chan.I.DTMF.TonesDeferred and DeferredTones
  - generate code for Conn.I.MailNotification
  - generate code for Protocol.I.Presence
  - generate code for AddClientInterest, RemoveClientInterest
  - update the Call example CM and its regression test

• fd.o #27948: add generic support for AddClientInterest,
  RemoveClientInterest on TpBaseConnection and TpConnection (smcv)

• fd.o #30505: add TpDTMFPlayer, a DTMF dialstring interpreter (smcv)

• TpClientChannelFactory: ask callers to prepare a given set of features,
  and do so in TpBaseClient (cassidy)

Fixes:

• fd.o #30730: order tests' and examples' CFLAGS and LIBS
  consistently, fixing builds in some situations (an older telepathy-glib
  built with -rpath already installed, possibly) (smcv)

• fd.o #30949: fix a typo that made DeliveryReportingSupport always come out
  as 0, and test a nonzero value (smcv)

• Don't leak an array of features in TpBaseClient (cassidy)

• fd.o #31027: if stdout is a tty, tests now succeed silently, and only
  produce output on failure; also, they will automatically fail after a
  few seconds if an expected event does not happen (smcv)

• fd.o #30999: tests now succeed on machines where IPv6 is supported but ::1
  is not assigned to an interface (smcv)

telepathy-glib 0.13.2 (2010-10-15)
==================================

The "whose thighs are capacitive?" release.

This release includes all the fixes from version 0.12.1.

Enhancements:

• Update to spec 0.21.2 (smcv)
  - add TP_ERROR_REJECTED, SendNamedTelephonyEvent, SendSoundTelephonyEvent
  - change the experimental Call interfaces and adjust the example CM to match

• fd.o #29973: add TpClientChannelFactory, TpAutomaticProxyFactory and
  TpBasicProxyFactory, and use them in TpBaseClient and TpAccountChannelRequest
  (cassidy)

• fd.o #29218: add TpStreamTubeChannel, a higher-level API for stream tubes
  (danni, cassidy)

• fd.o #30478: add TP_ACCOUNT_FEATURE_STORAGE (danni)

• Improve the error message for an undefined D-Bus interface (wjt)

Fixes:

• fd.o #30791: fix building with gtk-doc enabled, and an older telepathy-glib
  installed in a non-default library search path (danni)

• fd.o #30644: don't 'return' a void expression from a void function, which
  isn't valid C99 and breaks compilation on Sun Studio C (smcv)

• Remove redundant trailing semicolons from G_DEFINE_TYPE etc., which are
  not valid C99 (smcv)

• Add DeliveryReportingSupport to the properties offered by TpMessageMixin
  (cassidy)

• Add ContactListState to the properties offered by TpBaseContactList (smcv)

• Avoid using a gboolean (which is signed) as a one-bit bitfield (smcv)

telepathy-glib 0.13.1 (2010-10-04)
==================================

The "we're out of bear-shaped biscuits" release.

Enhancements:

• Update to spec 0.21.1 (smcv):
  - add Access_Control, Access_Control_Type, Conn.I.ClientTypes

• Add ClientTypes support to TpContact (jonny)

Fixes:

• In TpCapabilities, do more checks on the self pointer (cassidy)

telepathy-glib 0.13.0 (2010-09-28)
==================================

The "this whiteboard needs scrollbars" release.

Dependencies:

• Automake ≥ 1.11 is now required (when building from git or changing the
  build system)
• If GObject-Introspection is enabled, it must be version 0.9.6 or later

Enhancements:

• Update to spec 0.21.0 (smcv)
  · generate code for the ContactList and ContactGroups interfaces

• fd.o #28200: add TpBaseContactList, a base class for contact list/contact
  groups implementations (smcv)

• fd.o #30204: add checks for stream and D-Bus tubes to
  TpCapabilities (cassidy)

• fd.o #30327: add some new utility functions for TpHandleSet (smcv)

• fd.o #30310: make tp_contacts_mixin_get_contact_attributes public for
  re-use (eeejay)

• debug-log the error message when a Protocol filter rejects a parameter (wjt)

telepathy-glib 0.12.0 (2010-09-20)
==================================

The "you rang?" release, starting a 0.12.x stable branch.

Highlights since 0.10.x
-----------------------

Changes:

• when the local user is removed from a Group Channel, the GError used to
  invalidate the TpChannel has changed

New features:

• the TpProxy "feature" (prepare_async) mechanism
• high-level bindings for ChatStates, ContactCapabilities, ContactInfo, Avatars,
  detailed connection errors, and Protocol objects
• TpAccountChannelRequest, a high-level channel-requesting mechanism
• TpBaseClient, a base class for Observers, Approvers and Handlers
• generated constants for contact attributes and handler capability tokens
• base classes for Channel and Protocol in connection managers
• TpWeakRef, a wrapper for a weak reference and optional extra pointer
• experimental GObject-Introspection bindings, requiring version 0.6.14 or later
• experimental Vala bindings, requiring GObject-Introspection 0.9.6 and Vala
  0.10.0 or later
• generated code for all stable APIs in telepathy-spec 0.20, apart from
  client code for Channel.Type.ServerTLSConnection (which will follow in 0.13.x)

Note that the GObject-Introspection and Vala bindings are not subject to the
same API guarantees as the C API, and are likely to have incompatible changes
during the 0.13.x series.

Changes since 0.11.16
---------------------

• Disable documentation completeness checks and redirect documentation uploads
  for stable branch
• Add the version number, and a link to the latest version, to the documentation
• Increase dependencies for Vala bindings to versions that the libfolks
  developers have verified to work: gobject-introspection 0.9.6 and
  Vala 0.10.0

telepathy-glib 0.11.16 (2010-09-15)
===================================

The "Fear my moo of fury!" release.

Enhancements:

• Update to stable spec 0.20.1 (smcv)
  - generate basic API for Chan.I.Conference
  - generate basic API for Chan.T.ServerTLSConnection and TLSCertificate
    (server-side only for now, since TLSCertificate will require a new TpProxy
    subclass)

• When connections are created in a CM, debug-log the sanitized parameter
  values (wjt)

• When a TpBaseChannel is created, log an error if it doesn't have a parent
  connection (wjt)

Fixes:

• fd.o #30134: rename TpIntSet to TpIntset, with compatibility typedefs for
  the old name, to avoid breaking recent gobject-introspection (pwithnall)

• fd.o #30134: add more gobject-introspection annotations to work better
  with recent versions, and work around another case of (skip) not working
  in older versions (pwithnall, treitter, smcv)

Compatibility notes:

• If the Vala bindings are enabled, either GObject-Introspection must be
  older than 0.9.5, or GObject-Introspection and Vala must both be very
  recent (g-i 0.9.6 and Vala 0.9.9 will hopefully be suitable).

telepathy-glib 0.11.15 (2010-09-13)
===================================

The “Castle Turing” release.

Enhancements:

• Update to spec 0.19.12 (smcv)
  - generate code for SMS interface for Text channels, NotYet error,
    Object_Immutable_Properties type and
    TP_PROP_CONNECTION_INTERFACE_CELLULAR_OVERRIDE_MESSAGE_SERVICE_CENTRE

• fd.o #28420: add tp_channel_get_requested() etc. (cassidy)

• Add basic introspectability for the Connection mixins (treitter)

Fixes:

• fd.o #30090: fix parsing TpProtocol information from .manager files
  (fledermaus, smcv)

• fd.o #29943: make tp_debug_sender_log_handler thread-safe (smcv)

• fd.o #30111: make GObject-Introspection work again with g-i >= 0.9.5
  (danni, smcv)

• fd.o #30134: make configure fail if Vala bindings are enabled but g-i is
  disabled, which can't work (smcv)

• fd.o #25582, #27806, #30118: fix miscellaneous memory leaks (smcv)

telepathy-glib 0.11.14 (2010-08-25)
===================================

The “One hundred men can skin 5,000 cats a day.” release.

Enhancements:

• fd.o #29375: there's now a TpBaseChannel class which deals with all the
  boring boilerplate previously needed to implement channels. Public response
  to the class has been uniform. "The class is perfect", said one passer-by.
  (jonner, wjt)

• fd.o #29614: add TpBaseClient:account-manager (smcv)

• Allow TpBaseClient instances to wait for any desired set of TpAccount,
  TpConnection and TpChannel features (smcv)

• fd.o #29671: add TP_ARRAY_TYPE_UCHAR_ARRAY_LIST, i.e. signature 'aay' in
  dbus-glib (smcv)

Fixes:

• Ensure that when a TpAccountChannelRequest produces a connection and
  a channel, they're obtained from the same TpAccount we started from (smcv)

• fd.o #29756, #29795: various documentation improvements (smcv)

telepathy-glib 0.11.13 (2010-08-17)
===================================

The “Brand New Name” release.

Dependencies:

• When building from git or otherwise running automake, automake 1.11 is
  strongly recommended. If an older version is used, it will not be possible
  to generate Vala bindings, or to make tarball distributions.

Changes to experimental API:

• fd.o #29070: remove telepathy-vala.pc. Vala bindings should ask pkg-config
  for telepathy-glib, and can check that the VAPI file exists by attempting
  to link a trivial Vala program; see libfolks for example code (cassidy)

Enhancements:

• fd.o #29358: add TP_ERROR as an alias for TP_ERRORS, for introspectability
  (pwithnall)

• add TP_USER_ACTION_TIME_NOT_USER_ACTION, TP_USER_ACTION_TIME_CURRENT_TIME,
  tp_user_action_time_from_x11, tp_user_action_time_should_present (smcv)

• improve various documentation (smcv, danni)

• convert TpBaseClient virtual methods into normal GObject virtual methods
  so they can be introspected (smcv)

Fixes:

• Don't rely on vala-1.0.pc to check for vala version and vapigen (treitter)

• fd.o #25019: let the TpPresenceMixin work on connections that implement
  SimplePresence but not complex Presence (Butch Howard)

• Fix a harmless misuse of enums that caused warnings on gcc 4.5 (smcv)

• Fix out-of-tree builds with Vala enabled (smcv)

telepathy-glib 0.11.12 (2010-08-10)
===================================

The “Fire and Forget” release.

Enhancements:

• Added TpAccountChannelRequest, a request to create or ensure a channel (cassidy)
  ‣ fd.o #29456: tp_account_channel_request_create_async (plus an _ensure_
    variant): create a channel which will be handled by an existing Handler,
    probably another application
  ‣ fd.o #13422: tp_account_channel_request_create_and_handle_async (plus an
    _ensure_ variant): create a channel and handle it yourself

• fd.o #29461: updated to telepathy-spec 0.19.11 (smcv)
  ‣ more error codes for SSL/TLS - Insecure, Revoked, LimitExceeded
  ‣ Conference_Host call state flag

Fixes:

• fd.o #29174: update example connection managers to follow current
  telepathy-spec best practices, including Protocol objects (smcv)

• fd.o #29268: fix compilation from a tarball with --enable-vala-bindings (smcv)

telepathy-glib 0.11.11 (2010-07-26)
===================================

The “xev claims I'm typing in Japanese” release.

Enhancements:

↭ Updated spec to 0.19.10 (smcv):
  ↯ generate code for Protocol objects, and the ContactSearch channel type

↭ fd.o #27997: add TpProtocol client-side API, and TpBaseProtocol service-side
  base class, for Protocol objects (smcv)

↭ fd.o #28751: tp_proxy_has_interface is now a real function, not a macro, for
  better introspection (smcv)

↭ TpBaseClient's properties now have accessor methods for convenient use in C
  (smcv)

↭ tp_capabilities_get_channel_classes is now visible to g-i (pwithnall)

Fixes:

↭ Install a .deps file for the Vala bindings (pwithnall)

↭ Vala bindings now install to the normal location, making it unnecessary to
  look up telepathy-vala in pkg-config; that pkg-config file will be removed
  in a future version (treitter)

↭ fd.o #29197: expand g-i coverage of connection manager code enough to
  use it for libfolks' regression tests (pwithnall, smcv)

telepathy-glib 0.11.10 (2010-07-12)
===================================

The “as many fossils as last year” release.

Enhancements:

❱ Updated spec to 0.19.9 (wjt):
  ❭ added support for Read and Deleted delivery reports

Fixes:

❱ Improved GObject-Introspection annotations to be sufficient for
  libfolks (treitter)

❱ Fixed a typo in the documentation (jonny)

❱ fd.o #28920: fixed tp_contact_request_contact_info_async cancellation
  handling when dealing with synchronous errors (pwithnall)

telepathy-glib 0.11.9 (2010-07-02)
==================================

The “mistakenly displays 2 more bars than it should” release.

Enhancements:

❉ Updated spec to 0.19.8 (smcv):
  ➠ generate code for some new properties, Conn.I.Cellular and
    Account.I.Storage
  ➠ add convenience methods to TpAccount to access Account.Service

❉ Added tp_simple_async_report_success_in_idle, a convenience function to
  return "void" from an async method (smcv)

Fixes:

❉ Fixed libdbus errors when unregistering a TpBaseClient that isn't a
  Handler (cassidy)

❉ Made some TpGroupMixin methods more const-correct (smcv)

❉ Fixed some memory leaks in regression tests (smcv)

❉ Suppressed more valgrind false-positives (smcv, cassidy)

❉ Improved generation of experimental Vala bindings (treitter)

telepathy-glib 0.11.8 (2010-06-22)
==================================

The "moving to Canada for maple syrup and bacon" release.

Requirements:

⁂ If GObject-Introspection is enabled, it must be version 0.6.14 or later.

Enhancements:

⁂ Improve GObject-Introspection annotations, and optionally build Vala
  bindings, which are currently considered highly experimental (treitter)

Fixes:

⁂ Make tp_base_client_set_handler_bypass_approval able to set the value to
  FALSE, and hence make it possible for a TpSimpleHandler to not bypass
  approval (cassidy)

⁂ If the weak object for contact info retrieval disappears, stop, and don't
  call the callback (sjoerd)

⁂ Fix a va_list leak in tp_value_array_build (wjt)

⁂ Fix a memory leak for unlikely errors in tp_base_connection_register (wjt)

telepathy-glib 0.11.7 (2010-06-14)
==================================

The “why do my legs not work?” release.

Requirements:

⎎ If GObject-Introspection is enabled, it must be version 0.6.13 or later.

Deprecations:

⎎ TpChannelFactoryIface is officially deprecated (it shouldn't have been used
  since 0.8).

⎎ tp_verify() should not be used in new code: use GLib 2.20's G_STATIC_ASSERT.

Enhancements:

⎎ Update to telepathy-spec 0.19.7 (smcv)
  ⎓ generate code for the Anonymity and ServicePoint interfaces
  ⎓ add ChatStates property and Chat_State_Map type
  ⎓ add Account.ConnectionError and ConnectionErrorDetails properties

⎎ fd.o #27676: add TP_CONNECTION_FEATURE_CONTACT_INFO,
  TP_CONTACT_FEATURE_CONTACT_INFO, tp_contact_request_contact_info_async, etc.
  (Zdra)

⎎ fd.o #28241: add tp_channel_dispatch_operation_handle_with_time_async
  (cassidy)

⎎ fd.o #28379: add connection-error and connection-error-details properties
  to TpAccount, and implement the corresponding parameters of
  TpAccount::status-changed (smcv)

⎎ fd.o #28312: add TpContact::presence-changed signal (sjokkis)

⎎ fd.o #28368: use GStrv instead of gchar ** in structs, so
  GObject-Introspection ≥ 0.6.13 can introspect it correctly (Zdra)

⎎ Add tp_handle_set_new_from_array (smcv)

⎎ fd.o #28345: add tp_clear_object, tp_clear_pointer, tp_clear_boxed
  (also proposed for GLib/GObject, as Gnome bug #620263) (smcv)

⎎ Add TP_ERROR as a synonym for TP_ERRORS, to be nice to Vala (treitter)

⎎ fd.o #28334, #28347: speed up the regression tests, clean up their code, and
  put their utility code in a namespace so Vala tests can use it (treitter,
  smcv)

Fixes:

⎎ test-finalized-in-invalidated-handler: eliminate a race condition (smcv)

⎎ tp_connection_get_detailed_error: fix a memory leak introduced in 0.11.4
  (smcv)

telepathy-glib 0.11.6 (2010-05-25)
==================================

The "anybody need this sign?" release.

This version includes all the bugfixes from today's 0.10.6 release.

Requirements:

* If gtk-doc is enabled, it must be version 1.15 or later.
* If GObject-Introspection is enabled, it must be version 0.6.11 or later.

Enhancements:

* Updated to telepathy-spec 0.19.6:
  * ChangingPresence property on the Account interface
  * SupportedLocationFeatures property on the Location interface
  * HandleWithTime method on ChannelDispatchOperation
  * MultipleTones method, SendingTones and StoppedTones signals, and
    CurrentlySendingTones and InitialTones properties on the DTMF interface

* TpAccount:changing-presence and tp_account_get_changing_presence, a binding
  for the new ChangingPresence property (smcv)

* fd.o #27872: enhance TpBaseClient to support being a Handler (cassidy)

* fd.o #27873: TpSimpleHandler, a simple TpBaseClient subclass for
  projects that don't need their own subclass, and an example Approver that
  uses it (cassidy)

* fd.o #20035: add TP_CONTACT_FEATURE_AVATAR_DATA, the ability to cache and
  access avatar data (Zdra)

* fd.o #16170: cope better with sparse TpIntSets (smcv)

* Add more API for int sets and handle sets (smcv)

* Make the ContactList example connection manager more realistic, and add a
  regression test for it (smcv)

Fixes:

* fd.o #28203: TpGroupMixin: allow "adding" contacts who are already members,
  even if the Can_Add flag isn't set (e.g. accepting a subscription request
  twice), and allow "removing" contacts who are not in the channel
  (e.g. rejecting a subscription request twice) (smcv)

* Correct the syntax of TP_IS_HANDLE_REPO_IFACE (smcv)

* Move _tp_proxy_set_features_failed (which isn't intended to be API, and
  isn't ABI) to an internal header (smcv)

* Fix the namespace version for GObject-Introspection (smcv)

telepathy-glib 0.11.5 (2010-05-10)
==================================

The "also, hi from bl.uk" release.

API changes:

⌬ fd.o #23369: when the local user is removed from a Group Channel, the
  GError with which the TpChannel is invalidated has changed:

  → if possible, the detailed error name from D-Bus is mapped to a TpError,
    or a custom GError domain set up with tp_proxy_subclass_add_error_mapping
  → otherwise, the TpChannelGroupChangeReason is translated into a TpError

  Previously, we used an error from the TP_ERRORS_REMOVED_FROM_GROUP domain
  in most cases; this domain is no longer used, unless we get a change reason
  that isn't recognised.

Enhancements:

⌬ fd.o #25236: TpBaseClient, a base class for Observers and Approvers, which
  will also support Handlers in a future release (cassidy)

⌬ fd.o #27871, #24214: TpSimpleObserver, a simple TpBaseClient subclass for
  projects that don't need their own subclass, and an example Observer that
  uses it (cassidy)

⌬ fd.o #27875: TpSimpleApprover, a simple TpBaseClient subclass for
  projects that don't need their own subclass, and an example Approver that
  uses it (cassidy)

⌬ fd.o #27899: internal macros for ERROR(), CRITICAL() etc., analogous to
  DEBUG() (jonny)

⌬ fd.o #23369: improve the errors with which a Group TpChannel is invalidated
  if we're removed, as per "API changes" above (smcv)

⌬ fd.o #18055: generate GEnum types for TpCMInfoSource, TpContactFeature,
  TpDBusError, and GFlags types for TpDBusNameType and
  TpDBusPropertiesMixinFlags. Note that TpConnectionManager:info-source is
  still of type G_TYPE_UINT, not TP_TYPE_CM_INFO_SOURCE, since switching it
  would be an ABI break. (danni, smcv)

Fixes:

⌬ fd.o #26211: correct the generated constants for contact attributes and
  handler capability tokens, which were present-but-wrong since 0.11.3 (smcv)

⌬ fd.o #24689: document more clearly that the TpConnectionManager.protocols
  struct member can be reallocated (smcv)

⌬ fd.o #28043: explicitly link tests/* against dbus-glib, fixing compilation
  with GNU gold, with GNU ld with LDFLAGS=-Wl,--no-add-needed, and hopefully
  also with Fedora 13's patched GNU ld (see Red Hat #564245) (smcv)

⌬ some fixes to GObject-Introspection metadata (danni)

telepathy-glib 0.11.4 (2010-04-28)
==================================

The “not sure whether to be amused or terrified” release.

This version includes all the bugfixes from today's 0.10.5 release.

Requirements:

∮ gtk-doc 1.14 is now required. Applying commit 0a874b3a from gtk-doc git
  to support the (skip) annotation (as was done in Debian's gtk-doc 1.14-2) is
  also highly recommended; it'll be in upstream release 1.15.

∮ GLib 2.24 is now required.

Enhancements:

∮ telepathy-glib now has experimental GObject-Introspection bindings, for use
  by language bindings like PyGI and gjs. At this stage, these bindings are
  incomplete, and are *not* covered by our normal API guarantees - incompatible
  changes between versions are likely. (danni, smcv)

∮ fd.o #27769: add TP_CONNECTION_FEATURE_AVATAR_REQUIREMENTS (Zdra)

∮ fd.o #27794: improve regression test coverage for TpAccount (cassidy, smcv)

∮ fd.o #19164: tighten the definition of TpChannel:identifier to guarantee
  that it's always non-NULL, even before the channel is ready (previously,
  it could be NULL before the channel was ready)

∮ fd.o #23369 (partial): improve error mapping on TpConnection and
  TpBaseConnection, and add tp_connection_get_detailed_error (smcv)

Fixes:

∮ fd.o #27780: when TpAccount:connection changes, emit notify::connection (smcv)

∮ Don't leak TpAccount:parameters when disposed (smcv)

∮ Fix more assertion failures (this time in TpContact) if getting contact
  attributes fails or yields the wrong type (wjt)

∮ Remove some dead code to keep coverity happy (wjt)

telepathy-glib 0.11.3 (2010-04-20)
==================================

The "can we have a hippopotamus?" release.

This version includes both the bugfixes from today's 0.10.4 release.

Enhancements:

↠ Upgrade to telepathy-spec 0.19.5 (smcv)
  → Connection.Status, Connection.Interfaces properties (all telepathy-glib
    CMs that use TpBaseConnection should gain support for these automatically)
  → Observer.Recover property
  → ContactInfo interface

↠ Add TpWeakRef, a wrapper for a weak reference and an optional extra pointer
  (smcv)

↠ fd.o #21097: push the "feature" concept from TpAccount and TpAccountManager
  into the TpProxy base class, and use it to implement feature-preparation
  for core functionality of TpChannel, TpConnection, TpConnectionManager (smcv)

↠ add TP_CHANNEL_FEATURE_CHAT_STATES (smcv)

↠ fd.o #27511: add TpCapabilities, TP_CONNECTION_FEATURE_CAPABILITIES and
  TP_CONTACT_FEATURE_CAPABILITIES (cassidy)

↠ fd.o #27690, #27709: add boxed types for TpIntSet,
  TpConnectionManagerProtocol and TpConnectionManagerParam (danni, smcv)

↠ fd.o #27741: make it easier to export objects without using tp_get_bus(),
  particularly in connection managers (smcv)

↠ fd.o #26211: generate TP_TOKEN_${INTERFACE}_${TOKEN} constants for contact
  attributes and handler capability tokens (KA)

Fixes:

↠ Use the fast-path for Location correctly (cassidy)

↠ fd.o #27714: support G_CONNECT_AFTER in tp_g_signal_connect_object, and
  document exactly which flags we support (Maiku, smcv)

↠ fd.o #27537: fix assertion failure if getting contact attributes
  fails (cassidy)

↠ fd.o #27695: only try the slow path in Contacts if the fast path isn't
  supported (cassidy)

telepathy-glib 0.11.2 (2010-04-06)
==================================

The "not actually deprecated" release.

This version includes all the bugfixes from today's 0.8.3 and 0.10.3 releases
(they were all included in the previous version, in fact).

Un-deprecations:

☀ tp_get_bus is not considered to be deprecated yet after all; many connection
  managers use it, and the current alternative is considerably more verbose.
  This reopens fd.o #24114. (smcv)

telepathy-glib 0.11.1 (2010-04-05)
==================================

The “26-bit address bus” release.

Enhancements:

◈ Add tp_str_empty() macro, a shortcut for ‘NULL or ""’ (smcv)

◈ Add TP_TYPE_UCHAR_ARRAY, a dbus-glib GArray of guchar (i.e. the default
  representation for the D-Bus 'ay' type) (cassidy)

◈ Add tp_account_set_avatar_async() (cassidy)

◈ Add TP_CONTACT_FEATURE_LOCATION (cassidy)

Fixes:

◈ Only fail “make check” on documentation warnings in unreleased versions, to
  avoid build failures in releases when gtk-doc in a distribution doesn't have
  the same definition of full coverage that we do (smcv)

◈ Fix compatibility with gtk-doc 1.14 (smcv)

telepathy-glib 0.11.0 (2010-03-31)
==================================

The ‘bah, you removed my “beautiful” quotes’ release.

Dependencies:

‣ GLib, GObject and GIO ≥ 2.22 are now required

Deprecations:

‣ fd.o #22206: all the re-entrant functions (of the form tp_FOO_run_until_ready
  and tp_cli_FOO_run_BAR) are deprecated in this version, please use
  asynchronous calls instead (smcv)

‣ fd.o #24114: tp_get_bus() is deprecated, please use tp_dbus_daemon_dup()
  followed by tp_proxy_get_dbus_connection() (smcv)

Enhancements:

‣ Update to telepathy-spec 0.19.3 (smcv)
  ❧ generate code for new Connection.Interfaces, Connection.Status properties,
    and implement them in TpBaseConnection
  ❧ generate code for Connection.Interface.Balance

‣ Add an example connection manager for the experimental Call API that will
  eventually replace StreamedMedia (smcv)

‣ Add tp_g_socket_address_from_variant,
  tp_address_variant_from_g_socket_address (danni)

‣ Add tp_g_value_slice_new_byte (smcv)

‣ Add tp_value_array_unpack, the inverse of tp_value_array_build (danni)

‣ Make various minor improvements to the tests (smcv)

Fixes:

‣ tp_account_set_nickname_async: set the right source_tag (cassidy)

‣ fd.o #27281: clarify documentation for tp_message_mixin_sent, using
  telepathy-spec 0.19.2 as a reference (Maiku)

‣ Avoid using re-entrant functions, other than in regression tests (smcv)

‣ fd.o #21956: clean up documentation/defaults of TpContact properties (smcv)

‣ telepathy.am: if copied into a project where nothing is checked for
  unreleased version annotations, don't hang waiting for input (smcv)

telepathy-glib 0.10.2 (2010-03-31)
==================================

The "is that a koala in your roster or are you just nearby?" release.

Fixes:

* TpAccount: correctly add interfaces such as Avatars (danni)

* Make GetContactAttributes() in GLib CMs tolerate unsupported interfaces,
  as per telepathy-spec 0.19.2 (wjt)

* Improve documentation of TpContactsMixinFillContactAttributesFunc (mikhailz)

telepathy-glib 0.10.1 (2010-03-24)
==================================

The "usually quite loud" release.

This version includes all the bugfixes from 0.8.2, plus some documentation
improvements in code added since 0.8.

Fixes:

* Don't make an idle call to put received messages in the TpMessageMixin
  queue, potentially avoiding a reference leak (Vivek)

* tp_contacts_mixin_set_contact_attribute now takes a const gchar *
  instead of a gchar * (mikhailz)

* Escape the doc-comments better in generated service interfaces (smcv)

* Fix some typos and broken cross-references in the documentation, and
  improve the TpAccount documentation (smcv)

* Chain up to GObject's dispose method when destroying a
  TpBaseConnectionManager (smcv)

* Remove a misleading debug message from tp_list_connection_names (wjt)

telepathy-glib 0.10.0 (2010-01-21)
==================================

The "where did you get your bear?" release.

This release begins a bugfix-only 0.10.x branch, in which new API/ABI will no
longer be added; 0.11.x development releases will continue to be made from
the master branch. The 0.10.x branch targets the D-Bus API from
telepathy-spec 0.18.0.

The major enhancement since 0.8.x is that TpAccountManager and TpAccount,
previously simple stub classes, now have high-level API to manipulate
accounts. GLib 2.20 and dbus-glib 0.82 are now required, and telepathy-glib
now links against GIO.

Enhancements since 0.9.2:

* Add compile-time warnings if the results of functions that allocate memory
  are ignored; for a couple of these functions it's not obvious that a
  result is allocated, leading to non-obvious leaks (smcv)

* Add compile-time warnings if the results of certain functions with no
  side-effects are ignored, which is harmless but makes no sense (smcv)

* Improve lcov.am, syncing with telepathy-gabble (smcv)

Fixes since 0.9.2:

* fd.o #23848: when making a release, make the build system check for files
  that indicate unreleased status; correct a few such comments (wjt)

* fd.o #25149: when a TpAccount is invalidated (deleted), signal connection
  disconnection first (smcv)

* Exit the main loop gracefully when CMs are disconnected from the session bus
  (sjoerd)

* fd.o #14603: don't set fatal criticals in tp_run_connection_manager, CMs
  are now responsible for doing this (sjoerd)

* fd.o #25600: fix inadvertant GLib 2.20 dependency (jonny)

* In code generation tools (glib-ginterface-gen.py), allow D-Bus methods whose
  names are C keywords (smcv)

* Fix with-session-bus.sh dbus-monitor logging when /bin/sh is not bash (smcv)

telepathy-glib 0.9.2 (2009-12-03)
=================================

The "old-fashioned, with no silly mods" release.

Dependencies:

* dbus-glib (>= 0.82) is now required

Enhancements:

* Add tp_value_array_build utility function (sjoerd)

* Add tp_g_signal_connect_object, a non-leaky version of
  g_signal_connect_object (alsuren)

* fd.o #25283: add constants for namespaced D-Bus property names,
  such as TP_PROP_CHANNEL_CHANNEL_TYPE (smcv)

* fd.o #25235: add <telepathy-glib/telepathy-glib.h> which includes
  the most commonly-used headers (danni)

Fixes:

* fd.o #24257: make sure tp_account_prepare, tp_account_manager_prepare
  will fail if the object is invalidated, rather than never finishing
  (alsuren)

* fd.o #25051: fix a use-after-free in TpAccountManager by disconnecting
  signal handlers on destruction (alsuren)

* fd.o #24654: fix a potential use-after-free in TpAccount and TpAccountManager
  by copying the list of features required (alsuren)

* Future-proof TpAccount and TpAccountManager to allow more than one Feature
  (alsuren)

* fd.o #24394: improve code portability to Windows headers and compilers,
  based on patches from Matti Reijonen (smcv)

* fd.o #25121: fix failure to link when -Wl,--no-add-needed is used, which is
  the (faster) default behaviour for binutils-gold (Debian #556486) (smcv)

* Fix various coverity nits, including a missing call to va_end,
  and a typo in the documentation (smcv)

* fd.o #25359: alter code generation to cope with arbitrary UTF-8 in the
  spec (wjt)

* fd.o #25335: glib-client-gen: annotate deprecated D-Bus methods (jonny)

* Don't rely on enum types being unsigned (sjoerd)

* fd.o #25181: avoid unnecessary D-Bus calls re-fetching existing TpContact
  objects (alsuren)

* fd.o #25384: if accounts fail to prepare while the account manager is
  preparing, drop them from the list of valid accounts rather than
  never terminating (alsuren)

* If the fake AccountManager doesn't appear for some reason during AM
  regression tests, don't start the system implementation (alsuren)

* Fix a theoretical reference-leak in TpAccountManager, and some memory
  leaks in examples and regression tests (smcv)

telepathy-glib 0.9.1 (2009-10-15)
=================================

The "to quote Rob: sdflkaytliahdskljfhgaqgh;shf" release.

Fixes:

* Corrected the GLib dependency to 2.20 (this was also needed for 0.9.0,
  but that fact was undocumented) (smcv)

* Corrected the error message given when a write-only D-Bus property
  is read (Pekka Pessi)

* Work around GLib 2.20 being less const-correct than 2.22 (jonny)

* fd.o #23853: if a connection manager is discovered not to be running while
  TpConnectionManager has a ListProtocols call in-flight, then a new instance
  of the CM starts up and replies to that call, don't crash with an assertion
  failure (smcv)

* If a connection manager returns error from GetParameters(), don't dereference
  a NULL pointer and segfault (smcv)

* When asked to activate or introspect a connection manager, don't do anything
  until we have at least worked out whether it was initially running, in order
  to provide the documented behaviour (smcv)

* When getting parameter details from a running connection manager, consider
  parameters called "password" or ending with "-password" to be secret even
  if they lack the SECRET flag, as was already done when reading .manager
  files (smcv)

telepathy-glib 0.9.0 (2009-09-28)
=================================

The "purging all the lies" release.

Dependencies:

* GLib 2.20 is now required.
* telepathy-glib now links to GIO as well as GLib and GObject (in practice
  they're packaged together, and we already depended on a new enough GLib
  version that it would come with GIO).

Enhancements:

* TpAccountManager, TpAccount: add convenience API similar to libempathy's
  (jonny, with contributions from wjt/danni/sjoerd/smcv)

* telepathy-glib now uses Automake 1.11's "silent rules" feature for
  kernel-style output; as a result, we no longer use shave. If you were
  previously using --enable-shave to get prettier output, use
  --enable-silent-rules instead, and upgrade to Automake >= 1.11 if you will
  be altering the build system. (jonny)

telepathy-glib 0.8.0 (2009-09-24)
=================================

The "line in the sand" release.

This release begins a bugfix-only 0.8.x branch, in which new API/ABI will no
longer be added; 0.9.x development releases will continue to be made from
the master branch.

Summary of API changes since 0.6.x:

* Since 0.7.35, it is no longer guaranteed that the self-handle in
  TpBaseConnection is set to 0 when the state changes to DISCONNECTED;
  instead, it remains valid until the connection is disposed. This will cause
  assertion failures during disconnection in telepathy-sofiasip < 0.5.17 and
  telepathy-gabble < 0.7.9.

Summary of major enhancements since 0.6.x:

* updated telepathy-spec from 0.16.x to 0.18.0, with many new interfaces, the
  AccountManager, the ChannelDispatcher, and Clients
* added TpProxy, a base class representing remote D-Bus objects
  (see <http://smcv.pseudorandom.co.uk/2009/05/tp-proxy/>)
* added subclasses of TpProxy for all the major Telepathy objects (apart from
  Debug, which will be added later)
* added TpContact, an object representing a Telepathy contact
* added macros for interface-name GQuarks, and for Telepathy dbus-glib GTypes
* added the tp_asv_get_foo() family of functions to manipulate a{sv} maps
* used versioned symbols to document the ABI
* implemented various simple example connection managers and clients

Changes since 0.7.37:

* spec: update from 0.17.28 to 0.18.0 (no real changes) (smcv)
* ContactList example CM: fix a crash during shutdown (andrunko)
* StreamedMedia example CM: check for direction changes correctly (andrunko)
