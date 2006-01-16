#!/usr/bin/python2.4
import telepathy.interfaces
import inspect 
import gengobject

out = open("telepathy-interfaces.h", 'w')

gengobject.print_license(out, "telepathy-interfaces.h", "Header for Telepathy interface names")

gengobject.print_header_begin(out, "telepathy_interfaces")

for (cname,val) in telepathy.interfaces.__dict__.items():
    if cname[:2] !='__':
        out.write('#define '+cname +' '+'"'+val+'"\n')


gengobject.print_header_end(out, "telepathy_interfaces");

