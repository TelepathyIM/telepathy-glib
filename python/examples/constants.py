# constants.py
# Copyright (C) 2008-2010 Collabora Ltd.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

from telepathy.interfaces import CHANNEL_INTERFACE

CHANNEL = CHANNEL_INTERFACE

CHANNEL_TYPE = CHANNEL + ".ChannelType"
CHANNEL_TYPE_CALL = CHANNEL + ".Type.Call1"
CALL_INITIAL_AUDIO = CHANNEL_TYPE_CALL + '.InitialAudio'
CALL_INITIAL_VIDEO = CHANNEL_TYPE_CALL + '.InitialVideo'
CALL_MUTABLE_CONTENTS = CHANNEL_TYPE_CALL + '.MutableContents'

CALL_CONTENT = 'org.freedesktop.Telepathy.Call1.Content'
CALL_CONTENT_IFACE_MEDIA = \
    'org.freedesktop.Telepathy.Call1.Content.Interface.Media'

CALL_CONTENT_CODECOFFER = \
    'org.freedesktop.Telepathy.Call1.Content.CodecOffer'

CALL_STREAM = 'org.freedesktop.Telepathy.Call1.Stream'
CALL_STREAM_IFACE_MEDIA = \
    'org.freedesktop.Telepathy.Call1.Stream.Interface.Media'

CALL_STREAM_ENDPOINT = 'org.freedesktop.Telepathy.Call1.Stream.Endpoint'

STREAM_TRANSPORT_RAW_UDP = 1
STREAM_TRANSPORT_ICE_UDP = 2
STREAM_TRANSPORT_GTALK_P2P = 3
STREAM_TRANSPORT_WLM_2009 = 4
STREAM_TRANSPORT_SHM = 5
STREAM_TRANSPORT_MULTICAST = 6
STREAM_TRANSPOR_DUMMY = 0xff

CALL_STATE_UNKNOWN = 0
CALL_STATE_PENDING_INITIATOR = 1
CALL_STATE_PENDING_RECEIVER = 2
CALL_STATE_ACCEPTED = 3
CALL_STATE_ENDED = 4

CALL_FLAG_LOCALLY_RINGING = 1
CALL_FLAG_QUEUED = 2
CALL_FLAG_LOCALLY_HELD = 4
CALL_FLAG_FORWARDED = 8
CALL_FLAG_IN_PROGRESS = 16
CALL_FLAG_CLEARING = 32

CALL_STATE_CHANGE_REASON_UNKNOWN = 0
CALL_STATE_CHANGE_REASON_REQUESTED = 1

CONTENT_PACKETIZATION_RTP = 0
CONTENT_PACKETIZATION_RAW = 1
