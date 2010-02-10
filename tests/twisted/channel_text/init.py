"""
Test text channel initiated by me.
"""

import dbus

from tpltest import exec_test
from servicetest import call_async, EventPattern
import constants as cs

def test(q, bus):
    #pect('dbus-signal', signal='NewChannels')

    print "GOT NewChannels TEXT"

if __name__ == '__main__':
    exec_test(test)
