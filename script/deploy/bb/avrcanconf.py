import can
import os
from math import ceil
from struct import pack, unpack
import argparse

parser = argparse.ArgumentParser(description='avr can bus nodes configuration')
parser.add_argument('--id', type=int, choices=list(range(1, 31)))
parser.add_argument('--all', action='store_true')
parser.add_argument('--fw_upload', action='store_true')
parser.add_argument('--stat', action='store_true')

args = parser.parse_args()
canconf_bcast = 0xEC33F000 >> 3


def canconfidfromnodeid(nodeid):
    return canconf_bcast | (nodeid << 2)


bus = can.Bus(interface='socketcan',
              channel='can1',
              receive_own_messages=False)


def upload_fw(nodeid):
    avrid1tx = canconfidfromnodeid(nodeid)
    avrid1rx = avrid1tx + 1
    message = can.Message(arbitration_id=avrid1tx, is_extended_id=True, data=[ord('s')])
    bus.send(message)
    messagein = bus.recv(1.5)
    print(f"in: {messagein}")
    if messagein.dlc != 4 or messagein.data[0] != ord('s') or (messagein.data[3] & 0x80) != 0:
        raise 'invalid response, maybe not bootloader'

    binfile = '/mnt/w/byt/avr/fw/fw.bin'
    size = os.path.getsize(binfile)
    pgsize = 256
    pagenr = ceil(size / pgsize)
    print(f"upload file size:{size} pagenr:{pagenr} nodeid:{nodeid}")
    with open(binfile, mode='rb') as f:
        bytes_left = size
        for page in range(pagenr):
            pgdata = f.read(pgsize)
            page_bytes_in = 0
            pgxor = 0xA5
            print(f"upload page:{page} {len(pgdata)}")
            bus.send(can.Message(arbitration_id=avrid1tx, is_extended_id=True, data=[ord('d')]))
            while page_bytes_in < pgsize:
                msgdata = pgdata[page_bytes_in:page_bytes_in + 8].ljust(8, bytes.fromhex('ff'))
                for i in msgdata:
                    pgxor ^= i
                bus.send(can.Message(arbitration_id=avrid1tx, is_extended_id=True, data=msgdata))
                page_bytes_in += 8
            rsp = bus.recv()
            print(rsp)
            if rsp.dlc != 2:
                raise 'invalid size'
            if rsp.data[1] != pgxor:
                raise 'xorsum mismatch'
            pgaddr = page * pgsize
            bus.send(can.Message(arbitration_id=avrid1tx, is_extended_id=True, data=pack('<BI', ord('f'), pgaddr)))
            rsp = bus.recv()
            if rsp.dlc == 1 and rsp.data[0] == ord('f'):
                print(f"flash page:{page} pageaddr:{pgaddr} OK")
            else:
                raise f"flash page {page} error"
        print(f"Upload fw nodeid:{nodeid} finnished.")


if args.stat:
    message = can.Message(arbitration_id=canconfidfromnodeid(args.id), is_extended_id=True, data=[ord('s')])
    rv = bus.send(message)
    print(f"send rv:{rv}")
    messagein = bus.recv(1.5)
    print(f"in: {messagein}")

if args.fw_upload:
    upload_fw(args.id)
