#!/usr/bin/python2.4
import telepathy.interfaces
import inspect 
import gengobject

out = open("telepathy-interfaces.h", 'w')

gengobject.print_license(out, "telepathy-interfaces.h", "Header for Telepathy interface names")

gengobject.print_header_begin(out, "telepathy_interfaces")

interfaces = telepathy.interfaces.__dict__.keys()
interfaces.sort()

for cname in interfaces:
    val = telepathy.interfaces.__dict__[cname]
    if cname[:2] !='__':
        out.write('#define TP_IFACE_'+cname +' \\\n')
        out.write('        "'+val+'"\n')


gengobject.print_header_end(out, "telepathy_interfaces");

