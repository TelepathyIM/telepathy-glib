#!/usr/bin/python

import sys
import xml.dom.minidom
from string import ascii_letters, digits


from libglibcodegen import signal_to_marshal_name


class Generator(object):

    def __init__(self, dom):
        self.dom = dom
        self.marshallers = {}

    def do_signal(self, signal):
        marshaller = signal_to_marshal_name(signal, 'PREFIX')

        assert '__' in marshaller
        rhs = marshaller.split('__', 1)[1].split('_')

        self.marshallers[marshaller] = rhs

    def __call__(self):
        signals = self.dom.getElementsByTagName('signal')

        for signal in signals:
            self.do_signal(signal)

        all = self.marshallers.keys()
        all.sort()
        for marshaller in all:
            rhs = self.marshallers[marshaller]
            if not marshaller.startswith('g_cclosure'):
                print 'VOID:' + ','.join(rhs)

def types_to_gtypes(types):
    return [type_to_gtype(t)[1] for t in types]

if __name__ == '__main__':
    argv = sys.argv[1:]
    dom = xml.dom.minidom.parse(argv[0])

    Generator(dom)()
