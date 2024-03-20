#!/usr/bin/python3

import os, rpm
from pprint import pprint

ts = rpm.TransactionSet()
rpm_fd = rpm.fd("./rpm/RPMS/armv7hl/hello-1.0-1.armv7hl.rpm", "r")
hdr = ts.hdrFromFdno(rpm_fd)
print(f"rpm_fd.tell() = {rpm_fd.tell()}")

compr_fd = rpm.fd(rpm_fd, "r", flags=hdr[rpm.RPMTAG_PAYLOADCOMPRESSOR])
print(f"compr_fd.tell() = {compr_fd.tell()}")

ar = rpm.files(hdr).archive(compr_fd, False)
ar.tell()

print(f"compr_fd.tell() = {compr_fd.tell()}")

print(f"rpm_fd.tell() = {rpm_fd.tell()}")

#print(f"lseek fdno = {os.lseek(fdno, 0, os.SEEK_CUR)}")


for key in hdr.keys():
    print(f"{rpm.tagnames[key]} = {hdr[key]}")

#ar = rpm.files(hdr).archive(fdno2)
#pprint(ar)
#print(dir(ar))
#print(f"hascontent = {ar.hascontent()}")
#print(f"read = {ar.read(100)}")
#print(f"hascontent = {ar.hascontent()}")
#print(f"{fdno.tell}")
