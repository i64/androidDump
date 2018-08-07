import binascii
import sys

filename = sys.argv[1]
with open(filename, 'rb') as f:
    content = f.read()
hexoc = binascii.hexlify(content).split(b'6465780a')
hexoc.pop(0)
hexoc = b'6465780a' + b''.join(hexoc)
dex = open(sys.argv[1][:-4]+".dex","wb")
dex.write(binascii.a2b_hex(hexoc[:int.from_bytes(binascii.a2b_hex(hexoc[:72][-8:]),byteorder='little')*2]))
dex.close()
