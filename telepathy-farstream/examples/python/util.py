# util.py
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

from constants import *

def call_state_to_s(state):
    return {
        CALL_STATE_UNKNOWN: 'Unknown',
        CALL_STATE_PENDING_INITIATOR: 'Pending Initiator',
        CALL_STATE_PENDING_RECEIVER: 'Pending Receiver',
        CALL_STATE_ACCEPTED: 'Accepted',
        CALL_STATE_ENDED: 'Ended'
    }[state]

def call_flags_to_s(flags):
    flag_strs = {
        CALL_FLAG_LOCALLY_RINGING: 'Locally Ringing',
        CALL_FLAG_QUEUED: 'Queued',
        CALL_FLAG_LOCALLY_HELD: 'Locally Held',
        CALL_FLAG_FORWARDED: 'Forwarded',
        CALL_FLAG_IN_PROGRESS: 'In Progress',
        CALL_FLAG_CLEARING: 'Clearing'
    }

    return ' | '.join([ '%s (%d)' % (flag_strs[i], i)
        for i in flag_strs.keys() if flags & i ]) or 'None'
