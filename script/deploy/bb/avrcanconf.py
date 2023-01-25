import can
import os
from math import ceil
from struct import pack, unpack
import argparse

import yaml
from yaml import load, dump

parser = argparse.ArgumentParser(description='avr can bus nodes configuration')
parser.add_argument('--id', type=int, choices=list(range(1, 31)))
parser.add_argument('--all', action='store_true')
parser.add_argument('--fw_upload', action='store_true')
parser.add_argument('--stat', action='store_true')
parser.add_argument('--config', action='store_true')
parser.add_argument('--candev', default='can1')

args = parser.parse_args()
canconf_bcast = 0xEC33F000 >> 3


def canconfidfromnodeid(nodeid):
    return canconf_bcast | (nodeid << 2)


bus = can.Bus(interface='socketcan',
              channel=args.candev,
              receive_own_messages=False)

def class_info(trieda):
    count = len(trieda.keys())
    ids = set()
    for i in trieda.keys():
        ids.add(trieda[i]['addr'][0])
    return { 'ids' : ids, 'count' : count }

class ClassInfo:
    def __init__(self, node):
        self.node = node

    def info(self):
        count = len(self.node.keys())
        ids = set()
        for i in self.node.keys():
            ids.add(self.node[i]['addr'][0])
        return {'ids': ids, 'count': count}


class ClassInfoUniq(ClassInfo):
    def info(self):
        return {'ids': {self.node['addr']}, 'count': 1}


def buildClassInfo(trieda, node):
    if trieda in ('DigIN', 'OwT'):
        return ClassInfo(node)
    else:
        return ClassInfoUniq(node)


def configure_node(node):
    nodeId = node['id']
    outputCanIds = set()
    triedyNrIN = dict()

    for trieda in ('DigIN', 'OwT', 'SensorionCO2', 'Vlhkomer'):
        if trieda in node:
            triedaInfo = buildClassInfo(trieda, node[trieda])
            info = triedaInfo.info()
            outputCanIds.update(info['ids'])
            triedyNrIN[trieda] = info['count']

    print(f"nodeId:{nodeId} {triedyNrIN} outputCanIds:{outputCanIds}")



def configure_all():
    with open('config.yaml', "r") as stream:
        config = yaml.load(stream)
        for node in config['NodeCAN']:
            print(f"node name: {node}")
            configure_node(config['NodeCAN'][node])

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

if args.config:
    configure_all()