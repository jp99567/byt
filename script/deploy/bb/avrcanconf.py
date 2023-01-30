#!/usr/bin/python3

import can
import os
from math import ceil
from struct import pack, unpack
import argparse
import time
import yaml
import enum
from struct import pack

parser = argparse.ArgumentParser(description='avr can bus nodes configuration')
parser.add_argument('--candev', default='vcan0')
parser.add_argument('--id', type=int, choices=list(range(1, 31)))
parser.add_argument('--all', action='store_true')
parser.add_argument('--fw_upload', action='store_true')
parser.add_argument('--stat', action='store_true')
parser.add_argument('--config', action='store_true')
parser.add_argument('--yamlfile', default='config.yaml')
parser.add_argument('--generate', action='store_true', help='header file for can fw')
parser.add_argument('--cmd', help='svc command send to node')
parser.add_argument('--exit_bootloader', action='store_true')
parser.add_argument('msg', nargs='*', help='data for command')

args = parser.parse_args()
canid_bcast = 0xEC33F000 >> 3
canid_bcast_mask = ((1 << 20) - 1) << 9


class SvcProtocol(enum.Enum):
    CmdSetAllocCounts = enum.auto()
    CmdGetAllocCounts = enum.auto()
    CmdSetStage = enum.auto()
    CmdSetOwObjParams = enum.auto()
    CmdSetOwObjRomCode = enum.auto()
    CmdSetDigINObjParams = enum.auto()
    CmdSetDigOUTObjParams = enum.auto()
    CmdSetCanMob = enum.auto()
    CmdTestSetDDR = enum.auto()
    CmdTestGetDDR = enum.auto()
    CmdTestSetPORT = enum.auto()
    CmdTestGetPORT = enum.auto()
    CmdTestSetPIN = enum.auto()
    CmdTestGetPIN = enum.auto()
    CmdStatus = ord('s')
    CmdInvalid = ord('X')


class NodeStage(enum.Enum):
    stage0boot = 0
    stage1 = 0b01000000
    stage2 = 0b10000000
    stage3run = 0b11000000


def canIdFromNodeId(nodeid):
    return canid_bcast | (nodeid << 2)


def nodeIfFromCanId(canid):
    return (canid & ~canid_bcast_mask) >> 2


def canIdCheck(canid):
    if (canid & canid_bcast_mask) == canid_bcast:
        idnet = canid & ~canid_bcast_mask
        nodeId = idnet >> 2
        hostid = idnet & 0b11
        return nodeId, hostid
    return None


def sendBlocking(canbus, messageTx):
    canbus.send(messageTx)
    timeout = 1
    abs_timeout = time.time() + timeout
    while timeout >= 0:
        messageRx = canbus.recv(timeout)
        if messageRx is None:
            raise Exception(f"send read back failed: {messageTx}")
        if messageTx.arbitration_id == messageRx.arbitration_id:
            break
        timeout = abs_timeout - time.time()


class NodeBus:
    def __init__(self, canbus, nodeid=canid_bcast):
        self.bus = canbus
        self.canid = canIdFromNodeId(nodeid)
        self.nodeid = nodeid

    def svcTransfer(self, cmd, data=None):
        if self.nodeid == canid_bcast:
            return self.svcTransferBroadcast(cmd, data)
        txdata = [cmd.value] if data is None else [cmd.value, *data, ]
        messageTx = can.Message(arbitration_id=self.canid, is_extended_id=True, data=txdata)
        # print(f"can send {cmd.name} {txdata} {messageTx}")
        sendBlocking(self.bus, messageTx)
        timeout = 1.5
        abs_timeout = time.time() + timeout
        count = 20
        while count and timeout >= 0:
            count -= 1
            messagein = self.bus.recv(timeout)
            if messagein is None:
                raise Exception(f"no response nodeid:{self.nodeid} canid:{self.canid:X}")
            elif messagein.arbitration_id == self.canid + 1:
                return messagein.data
            timeout = abs_timeout - time.time()
        raise Exception(f"no response (or bus flooded) nodeid:{self.nodeid} canid:{self.canid:X}")

    def svcTransferBroadcast(self, cmd, data=None):
        txdata = [cmd.value] if data is None else [cmd.value, *data, ]
        messageTx = can.Message(arbitration_id=canid_bcast, is_extended_id=True, data=txdata)
        sendBlocking(self.bus, messageTx)
        timeout = 1.5
        abs_timeout = time.time() + timeout
        received = []
        while timeout >= 0:
            msgRx = bus.recv(timeout)
            if msgRx is None:
                if received:
                    break
                raise Exception(f"no response on broadcast")
            elif canIdCheck(msgRx.arbitration_id):
                n, h = canIdCheck(msgRx.arbitration_id)
                if h == 1:
                    received.append([n, msgRx.data])
            timeout = abs_timeout - time.time()
        if received is None: raise Exception(f"no response (or bus flooded) nodeid:{self.nodeid} canid:{self.canid:X}")
        return received


bus = None


def openBus():
    return can.Bus(interface='socketcan', channel=args.candev, receive_own_messages=True)


if args.stat or args.config or args.fw_upload:
    bus = openBus()


class ClassInfo:
    def __init__(self, node):
        self.node = node

    def info(self):
        count = len(self.node.keys())
        ids = set()
        for i in self.node.keys():
            ids.add(self.node[i]['addr'][0])
        return {'ids': ids, 'count': count}

    def initNodeObject(self, nodebus, mobs, mobSize):
        pass


def pinStr2Num(pin):
    pin = pin.upper()
    if len(pin) != 3 or pin[0] != 'P':
        raise Exception(f"invalid pin {pin}")
    bitidx = int(pin[2])
    portidx = ord(pin[1]) - ord('A')
    if portidx > 6 or bitidx > 7:
        raise Exception(f"invalid pin {pin} out of range")
    return portidx * 8 + bitidx


class ClassDigInOutInfo(ClassInfo):
    def __init__(self, node, initCmd):
        super().__init__(node)
        self.initObjCmd = initCmd

    def info(self):
        inf = ClassInfo.info(self)
        items = {}
        for i in self.node.keys():
            canid = self.node[i]['addr'][0]
            if canid not in items:
                items[canid] = []
            byte = self.node[i]['addr'][1]
            bit = self.node[i]['addr'][2]
            items[canid].append([8 * byte + bit, 1])
        inf['items'] = items
        return inf

    def initNodeObject(self, nodebus, mobs, mobSize):
        idx = 0
        for i in self.node.keys():
            canid = self.node[i]['addr'][0]
            mobidx = mobs.index(canid)
            byte = self.node[i]['addr'][1] + mobSize[canid]['start']
            mask = 1 << self.node[i]['addr'][2]
            pin = pinStr2Num(self.node[i]['pin'])
            nodebus.svcTransfer(self.initObjCmd, [idx, mobidx, byte, mask, pin])
            idx += 1


class ClassOwtInfo(ClassInfo):
    def info(self):
        inf = ClassInfo.info(self)
        items = {}
        for i in self.node.keys():
            canid = self.node[i]['addr'][0]
            if canid not in items:
                items[canid] = []
            byte = self.node[i]['addr'][1]
            items[canid].append([8 * byte, 16])
        inf['items'] = items
        return inf

    def initNodeObject(self, nodebus, mobs, mobSize):
        idx = 0
        for i in self.node.keys():
            canid = self.node[i]['addr'][0]
            mobidx = mobs.index(canid)
            byte = self.node[i]['addr'][1] + mobSize[canid]['start']
            nodebus.svcTransfer(SvcProtocol.CmdSetOwObjParams, [idx, mobidx, byte])
            if len(self.node.keys()) > 1:
                rc = self.node[i]['owRomCode']
                print(f"ToDo send romcode {rc}")
                nodebus.svcTransfer(SvcProtocol.CmdSetOwObjRomCode, [idx, 0])
            idx += 1


class ClassInfoUniq(ClassInfo):
    def info(self):
        return {'ids': {self.node['addr']}, 'count': 1, 'items': {self.node['addr']: [[0, 4 * 8]]}}


def buildClassInfo(trieda, node):
    if trieda == 'DigIN':
        return ClassDigInOutInfo(node, SvcProtocol.CmdSetDigINObjParams)
    elif trieda == 'DigOUT':
        return ClassDigInOutInfo(node, SvcProtocol.CmdSetDigOUTObjParams)
    elif trieda == 'OwT':
        return ClassOwtInfo(node)
    else:
        return ClassInfoUniq(node)


def configure_node(node):
    nodeId = node['id']
    outputCanIds = set()
    inputCanIds = set()
    triedyNr = dict()
    triedyInfo = dict()
    triedy = []

    for trieda in ('DigIN', 'OwT', 'SensorionCO2', 'Vlhkomer'):
        if trieda in node:
            triedaInfo = buildClassInfo(trieda, node[trieda])
            info = triedaInfo.info()
            outputCanIds.update(info['ids'])
            triedyNr[trieda] = info['count']
            triedyInfo[trieda] = info
            triedy.append(triedaInfo)
        else:
            triedyNr[trieda] = 0

    for trieda in ('DigOUT',):
        if trieda in node:
            triedaInfo = buildClassInfo(trieda, node[trieda])
            info = triedaInfo.info()
            inputCanIds.update(info['ids'])
            triedyNr[trieda] = info['count']
            triedyInfo[trieda] = info
            triedy.append(triedaInfo)
        else:
            triedyNr[trieda] = 0

    print(f"nodeId:{nodeId} {triedyNr} outputCanIdNr:{len(outputCanIds)} inputCanIdNr:{len(inputCanIds)}")
    print(triedyInfo)
    canmobs = dict()
    for trieda in triedyInfo:
        for canid, items in triedyInfo[trieda]['items'].items():
            if canid not in canmobs:
                canmobs[canid] = items
            else:
                canmobs[canid].extend(items)

    canmob_size = dict()
    for id in canmobs:
        canmobs[id].sort(key=lambda e: e[0])
        prev_end = 0
        for i in canmobs[id]:
            if prev_end > i[0]:
                raise Exception(f"canid:{id:X} overlaps at bit {i[0]} items {canmobs[id]}")
            prev_end = i[0] + i[1]
        if prev_end > 8 * 8:
            raise Exception(f"canid:{id:X} exceeds 8B items:({canmobs[id]})")
        canmob_size[id] = (prev_end + 7) // 8

    print(canmob_size)

    if inputCanIds.intersection(outputCanIds):
        raise Exception(f"IN/OUT can ids mixed {inputCanIds} {outputCanIds}")

    canmobsList = sorted(inputCanIds)
    canmobsList.extend(sorted(outputCanIds))
    print(canmobsList)

    trans = NodeBus(bus, nodeId)
    trans.svcTransfer(SvcProtocol.CmdSetAllocCounts,
                      [triedyNr['DigIN'],
                       triedyNr['DigOUT'],
                       triedyNr['OwT'],
                       sum(canmob_size.values()),
                       len(inputCanIds),
                       len(canmobsList)])

    trans.svcTransfer(SvcProtocol.CmdSetStage, [NodeStage.stage2.value])

    startOffset = 0
    for mobcanid in canmobsList:
        size = canmob_size[mobcanid]
        canmob_size[mobcanid] = {'start': startOffset, 'size': size}
        startOffset += size

    print(canmob_size)

    for trieda in triedy:
        trieda.initNodeObject(trans, canmobsList, canmob_size)

    for idx, canid in enumerate(canmobsList):
        endIoIdx = canmob_size[canid]['start'] + canmob_size[canid]['size']
        data = pack('BBH', idx, endIoIdx, canid)
        trans.svcTransfer(SvcProtocol.CmdSetCanMob, data)


def configure_all():
    with open(args.yamlfile, "r") as stream:
        config = yaml.safe_load(stream)
        for node in config['NodeCAN']:
            print(f"node name: {node}")
            configure_node(config['NodeCAN'][node])


def upload_fw(nodeid):
    avrid1tx = canIdFromNodeId(nodeid)
    avrid1rx = avrid1tx + 1
    message = can.Message(arbitration_id=avrid1tx, is_extended_id=True, data=[ord('s')])
    sendBlocking(bus, message)
    messagein = bus.recv(1.5)
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
            sendBlocking(bus, can.Message(arbitration_id=avrid1tx, is_extended_id=True, data=[ord('d')]))
            while page_bytes_in < pgsize:
                msgdata = pgdata[page_bytes_in:page_bytes_in + 8].ljust(8, bytes.fromhex('ff'))
                for i in msgdata:
                    pgxor ^= i
                sendBlocking(bus, can.Message(arbitration_id=avrid1tx, is_extended_id=True, data=msgdata))
                page_bytes_in += 8
            rsp = bus.recv()
            if rsp.dlc != 2:
                raise 'invalid size'
            if rsp.data[1] != pgxor:
                raise 'xorsum mismatch'
            pgaddr = page * pgsize
            sendBlocking(bus,
                         can.Message(arbitration_id=avrid1tx, is_extended_id=True, data=pack('<BI', ord('f'), pgaddr)))
            rsp = bus.recv()
            if rsp.dlc == 1 and rsp.data[0] == ord('f'):
                print(f"flash page:{page} pageaddr:{pgaddr} OK")
            else:
                raise f"flash page {page} error"
        print(f"Upload fw nodeid:{nodeid} finnished Ok.")


def statusRsp(rsp):
    if SvcProtocol.CmdStatus == SvcProtocol(rsp[0]):
        stage = rsp[3] >> 6
        stat = {'canTxErr': rsp[1], 'canRxErr': rsp[2], 'stage': stage}
        flags = []
        if rsp[3] & (1 << 4): flags.append('JTRF')
        if rsp[3] & (1 << 3): flags.append('WDRF')
        if rsp[3] & (1 << 2): flags.append('BORF')
        if rsp[3] & (1 << 1): flags.append('EXTRF')
        if rsp[3] & (1 << 0): flags.append('PORF')
        return f"status {stat} {flags}"
    else:
        return f"unknown: {messagein}"


if args.stat:
    if args.all:
        tran = NodeBus(bus)
        rsp = tran.svcTransferBroadcast(SvcProtocol.CmdStatus)
        for node, data in rsp:
            print(f"node:{node} {statusRsp(data)}")
    else:
        tran = NodeBus(bus, args.id)
        rsp = tran.svcTransfer(SvcProtocol.CmdStatus)
        if rsp is None:
            print("No response")
        else:
            print(f"{statusRsp(rsp)}")

if args.fw_upload:
    upload_fw(args.id)

if args.config:
    configure_all()


def generateCppHeader():
    head = '''# pragma once
// GENERATED
namespace Svc { namespace Protocol {
    enum Cmd  {
'''
    foot = ''' };
}}'''

    with open("avr/fw/SvcProtocol_generated.h", 'w') as f:
        f.write(head)
        for i in SvcProtocol:
            f.write(f"{i.name} = {i.value},\n")
        f.write(foot)


if args.generate:
    generateCppHeader()

if args.cmd:
    trans = NodeBus(openBus(), args.id)
    data = [int(v, 0) for v in args.msg]
    rsp = trans.svcTransfer(SvcProtocol[args.cmd], data)
    if SvcProtocol(rsp[0]) in (SvcProtocol.CmdTestGetPIN, SvcProtocol.CmdTestGetPORT, SvcProtocol.CmdTestGetDDR):
        out = [f"{v:08b}" for v in rsp[1:]]
        print(f"{out}")

if args.exit_bootloader:
    id = canid_bcast
    if not args.all:
        id = canIdFromNodeId(args.id)
    msg = can.Message(arbitration_id=id, is_extended_id=True, data=[ord('c')])
    sendBlocking(openBus(), msg)
