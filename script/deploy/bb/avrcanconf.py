#!/usr/bin/python3
import struct

import can
import os
from math import ceil
from struct import pack, unpack
import argparse
import time
import yaml
import enum
from struct import pack
from copy import deepcopy
import datetime

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
parser.add_argument('--simulator', action='store_true', help='avr can node simulator')
parser.add_argument('--sniffer', action='store_true')
parser.add_argument('--test_gpios', action='store_true')
parser.add_argument('--ow_search', action='store_true')
parser.add_argument('--ow_read_temp_single', action='store_true')
parser.add_argument('--ow_read_all', action='store_true', help='search ow sensors and read out temperatures')

args = parser.parse_args()
canid_bcast = 0xEC33F000 >> 3
canid_bcast_mask = ((1 << 20) - 1) << 9

fw_build_epoch_base = 1675604744   # 5.feb 20231 14:45:44 CET

class SvcProtocol(enum.IntEnum):
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
    CmdGetGitVersion = enum.auto()
    CmdOwInit = enum.auto()
    CmdOwReadBits = enum.auto()
    CmdOwWriteBits = enum.auto()
    CmdOwSearch = enum.auto()
    CmdOwGetReplyCode = enum.auto()
    CmdOwGetData = enum.auto()
    CmdStatus = ord('s')
    CmdInvalid = ord('X')


class OwResponseCode(enum.IntEnum):
    eBusy = enum.auto()
    eRspError = enum.auto()
    eOwPresenceOk = enum.auto()
    eOwBusFailure0 = enum.auto()
    eOwBusFailure1 = enum.auto()
    eOwNoPresence = enum.auto()
    eOwBusFailure3 = enum.auto()
    eOwReadBitsOk = enum.auto()
    eOwReadBitsFailure = enum.auto()
    eOwWriteBitsOk = enum.auto()
    eOwWriteBitsFailure = enum.auto()
    eOwSearchResult0 = enum.auto()
    eOwSearchResult1 = enum.auto()
    eOwSearchResult11 = enum.auto()
    eOwSearchResult00 = enum.auto()


class OwCmd(enum.Enum):
    READ_ROM = 0x33
    CONVERT = 0x44
    MATCH_ROM = 0x55
    READ_SCRATCHPAD = 0xBE
    SKIP_ROM = 0xCC
    SEARCH = 0xF0


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
    messageTx.dlc = len(messageTx.data)
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
                rc = bytearray.fromhex(self.node[i]['owRomCode'])
                if len(rc) != 8:
                    raise f'Invalid rc length {rc}'
                rsp = nodebus.svcTransfer(SvcProtocol.CmdSetOwObjRomCode, [idx, *rc[1:7]])
                if SvcProtocol(rsp[0]) != SvcProtocol.CmdSetOwObjRomCode:
                    raise f'Invalid response {rc} {rsp[1]}'
                if rsp[1] != rc[7]:
                    raise f'Inwalid crc {rc} {rsp[1]}'
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
    if messagein.dlc != 4 or messagein.data[0] != ord('s') or 0 != (messagein.data[3] & 0b11000000):
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
        rest = [f"{v:02X}" for v in rsp[3:]]
        return f"status {stat} {flags} {rest}"
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
    head1 = '''# pragma once
// GENERATED
namespace Svc {
    constexpr uint32_t fw_build_epoch_base = 1675604744; // Ne  5. február 2023, 14:45:44 CET
namespace Protocol {
    enum Cmd  {
'''
    foot1 = ''' };
}}'''

    def write_enums(file, ename):
        for item in ename:
            file.write(f"{item.name} = {item.value},\n")

    def path(ename): return f"avr/fw/{ename}_generated.h"

    def write_header(head, enuminst, foot):
        with open(path(enuminst.__name__), 'w') as f:
            f.write(head)
            write_enums(f, enuminst)
            f.write(foot)

    head2 = '''# pragma once
// GENERATED
namespace ow {
   constexpr int16_t cInvalidValue = -21931; 
   enum ResponseCode {
'''
    write_header(head1, SvcProtocol, foot1)
    write_header(head2, OwResponseCode, foot1[:-1])


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

if args.simulator:
    bus = openBus()
    stage = 0b01000000
    while True:
        msg = bus.recv()
        print(msg)
        if msg.data[0] == SvcProtocol.CmdStatus:
            sendBlocking(bus, can.Message(arbitration_id=msg.arbitration_id + 1, is_extended_id=True,
                                          data=[msg.data[0], 0, 0, stage]))
            continue
        elif msg.data[0] == SvcProtocol.CmdSetStage:
            stage = msg.data[1]
        sendBlocking(bus, can.Message(arbitration_id=msg.arbitration_id + 1, is_extended_id=True, data=[msg.data[0]]))


def sniff():
    bus = openBus()
    while True:
        msg = bus.recv()
        data = list(msg.data)
        if canIdCheck(msg.arbitration_id):
            nodeid, minor = canIdCheck(msg.arbitration_id)
            if minor == 0b11:
                if len(data) == 8:
                    CANSIT, CANEN = struct.unpack('HH', msg.data[0:4])
                    rest = [f"{v:02X}" for v in data[4:6]]
                    tmp, = struct.unpack('H', msg.data[6:8])
                    print(f"debug({nodeid},{minor}) {CANSIT:04X} {CANEN:04X} {rest} {tmp:04X}")
                else:
                    rest = [f"{v:02X}" for v in data]
                    print(f"debug({nodeid},{minor}) {rest}")

            else:
                type = 'req'
                if minor == 1: type = 'rsp'

                if data[0] in iter(SvcProtocol):
                    rest = [f"{v:02X}" for v in data[1:]]
                    if SvcProtocol(data[0]) == SvcProtocol.CmdOwWriteBits:
                        if type == 'req':
                            if len(data) > 2:
                                pwstr = data[1] != 0
                                owdata = [f"{v:02X}" for v in data[2:]]
                                rest = f"pwstr:{pwstr} len:{len(data) - 2} {owdata}"
                    elif SvcProtocol(data[0]) == SvcProtocol.CmdOwReadBits:
                        if type == 'req':
                            count = -1
                            if len(data) > 1:
                                count = data[1]
                            rest = f"bitcount {count}"
                    elif SvcProtocol(data[0]) == SvcProtocol.CmdOwGetReplyCode:
                        if type == 'rsp':
                            if len(data) > 2:
                                rest = f"{OwResponseCode(data[1]).name} bitcount:{data[2]}"
                    elif SvcProtocol(data[0]) == SvcProtocol.CmdGetGitVersion and type == 'rsp':
                        if len(data) == 8:
                            data.append(0)
                            git_ver32bit, build_time = struct.unpack('II', bytearray(data[1:]))
                            dirty = build_time & 1 == 1
                            build_epoch = fw_build_epoch_base + (build_time >> 1)*60
                            build_datetime = datetime.datetime.fromtimestamp(build_epoch)
                            rest = f'build time:{build_datetime} git version:{git_ver32bit:08x}'
                            if dirty:
                                rest += '-DIRTY'

                    print(f"({nodeid},{minor}) {type} {SvcProtocol(data[0]).name} {rest}")
                else:
                    rest = [f"{v:02X}" for v in data]
                    print(f"({nodeid},{minor}) ? {rest}")
        else:
            print(msg)


if args.sniffer:
    sniff()


def dallas_crc8(data):
    crc = 0
    for c in data:
        for i in range(0, 8):
            b = (crc & 1) ^ (((int(c) & (1 << i))) >> i)
            crc = (crc ^ (b * 0x118)) >> 1
    return crc


def owRequest(nodebus):
    def req(cmd, candata=None):
        nodebus.svcTransfer(cmd, candata)
        trsp = nodebus.svcTransfer(SvcProtocol.CmdOwGetReplyCode)
        while OwResponseCode(trsp[1]) == OwResponseCode.eBusy:
            trsp = nodebus.svcTransfer(SvcProtocol.CmdOwGetReplyCode)
        return trsp

    return req


def owSearchRom(req):
    sensors = list()
    last_branch = -1
    sensor = bytearray(8)
    while True:
        resp = req(SvcProtocol.CmdOwInit)
        if OwResponseCode(resp[1]) != OwResponseCode.eOwPresenceOk:
            if OwResponseCode(resp[1]) != OwResponseCode.eOwNoPresence:
                print(OwResponseCode(resp[1]).name)
                break
            else:
                print(f"ow init failed {OwResponseCode(resp[1]).name}")
                raise "ow failed"

        resp = req(SvcProtocol.CmdOwWriteBits, [0, OwCmd.SEARCH.value])
        if OwResponseCode(resp[1]) != OwResponseCode.eOwWriteBitsOk:
            print(f"ow start search failed {OwResponseCode(resp[1]).name} {resp}")
            raise "ow start search failed"

        time.sleep(3)

        last_zero = -1
        for bitidx in range(8 * 8):
            byte, bit = bitidx // 8, bitidx % 8
            smer = False
            expect_b00 = False
            if bitidx == last_branch:
                smer = True
                expect_b00 = True
            elif bitidx < last_branch:
                smer = sensor[byte] & (1 << bit)

            resp = req(SvcProtocol.CmdOwSearch, [int(smer)])
            res = OwResponseCode(resp[1])
            # print(f"bit:{bitidx} {res.name}")
            if expect_b00 and res != OwResponseCode.eOwSearchResult00:
                raise 'unexpected eOwSearchResult00'

            newval = smer
            if res == OwResponseCode.eOwSearchResult00:
                if not newval:
                    last_zero = bitidx
            elif res == OwResponseCode.eOwSearchResult11:
                raise "search failed b11"
            else:
                newval = res == OwResponseCode.eOwSearchResult0

            if newval:
                sensor[byte] |= (1 << bit)
            else:
                sensor[byte] &= ~(1 << bit)
        last_branch = last_zero

        if dallas_crc8(sensor[:-1]) != sensor[-1]:
            raise 'invalid CRC'

        print(f"new sensor {sensor.hex()}")
        sensors.append(deepcopy(sensor))
        if last_branch < 0:
            break
    return sensors


def owSkiprom(req):
    rsp = req(SvcProtocol.CmdOwWriteBits, [0, OwCmd.SKIP_ROM.value])
    if (OwResponseCode(rsp[1]) != OwResponseCode.eOwWriteBitsOk):
        print(f"ow skiprom failed {OwResponseCode(rsp[1]).name}")
        raise "ow failed"
    print(f'ow skip rom ok {OwResponseCode(rsp[1]).name}')


def owConvert(req):
    resp = req(SvcProtocol.CmdOwInit)
    if (OwResponseCode(resp[1]) != OwResponseCode.eOwPresenceOk):
        print(f"ow init failed {OwResponseCode(resp[1]).name}")
        raise "ow failed"
    print(f"ow init ok {OwResponseCode(resp[1]).name}")
    owSkiprom(req)
    resp = req(SvcProtocol.CmdOwWriteBits, [1, OwCmd.CONVERT.value])
    if (OwResponseCode(resp[1]) != OwResponseCode.eOwWriteBitsOk):
        print(f"ow failed {OwResponseCode(resp[1]).name}")
        raise "ow failed"
    time.sleep(1)
    print(f'ow convert ok {OwResponseCode(resp[1]).name}')


def owReadScratchPad(req, t):
    resp = req(SvcProtocol.CmdOwWriteBits, [0, OwCmd.READ_SCRATCHPAD.value])
    if (OwResponseCode(resp[1]) != OwResponseCode.eOwWriteBitsOk):
        print(f"ow read rom failed {OwResponseCode(resp[1]).name} {resp}")
        raise "ow read rome failed"

    resp = req(SvcProtocol.CmdOwReadBits, [8 * 9])
    if (OwResponseCode(resp[1]) != OwResponseCode.eOwReadBitsOk):
        print(f"ow read out failed {OwResponseCode(resp[1]).name} {resp}")
        raise "ow failed"

    read_len = resp[2]
    data1 = t.svcTransfer(SvcProtocol.CmdOwGetData)
    data2 = t.svcTransfer(SvcProtocol.CmdOwGetData, [7])
    ow_scratchpad = data1[1:] + data2[1:]

    if dallas_crc8(ow_scratchpad[:-1]) != ow_scratchpad[-1]:
        raise "CRC error"
    return ow_scratchpad


def owReadTempSingle():
    t = NodeBus(openBus(), args.id)
    req = owRequest(t)

    owConvert(req)

    resp = req(SvcProtocol.CmdOwInit)
    if (OwResponseCode(resp[1]) != OwResponseCode.eOwPresenceOk):
        print(f"ow init failed {OwResponseCode(resp[1]).name}")
        raise "ow failed"

    owSkiprom(req)
    ow_scratchpad = owReadScratchPad(req, t)
    v16, = struct.unpack('h', ow_scratchpad[:2])
    print(f"{v16:02X} teplota: {v16 / 16}")


if args.ow_search:
    t = NodeBus(openBus(), args.id)
    req = owRequest(t)
    owSearchRom(req)

if args.ow_read_temp_single:
    owReadTempSingle()


def owReadTempAll():
    pass


def owMatchRom(req, t, sens):
    resp = req(SvcProtocol.CmdOwInit)
    if (OwResponseCode(resp[1]) != OwResponseCode.eOwPresenceOk):
        print(f"ow init failed {OwResponseCode(resp[1]).name}")
        raise "ow failed"

    rsp = req(SvcProtocol.CmdOwWriteBits, [0, OwCmd.MATCH_ROM.value])
    if (OwResponseCode(rsp[1]) != OwResponseCode.eOwWriteBitsOk):
        print(f"ow match rom failed {OwResponseCode(rsp[1]).name}")
        raise "ow failed"

    ba1 = sens[:6]
    resp = req(SvcProtocol.CmdOwWriteBits, [0, *ba1, ])
    if (OwResponseCode(resp[1]) != OwResponseCode.eOwWriteBitsOk):
        print(f"ow write down failed {OwResponseCode(resp[1]).name} {resp}")
        raise "ow failed"
    ba2 = sens[6:]
    resp = req(SvcProtocol.CmdOwWriteBits, [0, *ba2, ])
    if (OwResponseCode(resp[1]) != OwResponseCode.eOwWriteBitsOk):
        print(f"ow write down failed {OwResponseCode(resp[1]).name} {resp}")
        raise "ow failed"


if args.ow_read_all:
    t = NodeBus(openBus(), args.id)
    req = owRequest(t)
    sensors = owSearchRom(req)
    owConvert(req)
    for sens in sensors:
        owMatchRom(req, t, sens)
        ow_scratchpad = owReadScratchPad(req, t)
        v16, tH, tL, conf, = struct.unpack('hBBB', ow_scratchpad[:5])
        factor = 1 / 16
        if conf == 0xFF: factor = 1 / 2
        print(f"{sens.hex()} teplota: {v16 * factor}deg  (0x{v16:04X} tH:0x{tH:02X} tL:0x{tL:02X} conf:0b{conf:08b})")


def pinId2Str(id):
    if 0 <= id < 6 * 8 + 5:
        byte = id // 8
        bit = id % 8
        p = ord('A') + byte
        return 'P' + chr(p) + f"{bit}"
    raise "pin id out fo range"


def pinStr2Id(str):
    if len(str) != 3 or str[0] != 'P':
        raise f"invalid pin {str}"

    byte = ord(str[1]) - ord('A')
    bit = int(str[2])
    id = 8 * byte + bit
    if 0 <= id < 6 * 8 + 5:
        return id
    raise f"pin {str} out fo range"


def setByteArrayBit(data, id, value):
    byte = id // 8
    mask = 1 << (id % 8)
    if value:
        data[byte] |= mask
    else:
        data[byte] &= ~mask


def test_gpios():
    Okej = True

    def sayPort(desc, data):
        out = [f'{v:08b}' for v in data]
        print(f"{desc}: {out}")

    trans = NodeBus(openBus(), args.id)

    def setIoData(portcmd, data):
        rsp = trans.svcTransfer(portcmd, data)
        if SvcProtocol(rsp[0]) != portcmd:
            raise f"returned {SvcProtocol(rsp[0]).name}"

    def getIoData(portcmd):
        rsp = trans.svcTransfer(portcmd)
        if SvcProtocol(rsp[0]) != portcmd or len(rsp) != 8:
            raise f"returned get port command {SvcProtocol(rsp[0]).name} {len(rsp)}"
        return rsp[1:]

    ddr = getIoData(SvcProtocol.CmdTestGetDDR)
    port = getIoData(SvcProtocol.CmdTestGetPORT)
    sayPort('DDR', ddr)
    sayPort('PORT', port)

    pinlist = list(range(0, pinStr2Id('PG4') + 1))
    pinlist.remove(pinStr2Id('PD5'))
    pinlist.remove(pinStr2Id('PD6'))

    expectedPin = bytearray(7)

    def compareBits(actual, expected):
        for i in pinlist:
            byte = i // 8
            mask = 1 << (i % 8)
            if not ((actual[byte] & mask) == (expected[byte] & mask)):
                return False
        return True

    # pull up
    for i in pinlist:
        setByteArrayBit(port, i, True)
        setByteArrayBit(expectedPin, i, True)

    setIoData(SvcProtocol.CmdTestSetPORT, port)

    pin = getIoData(SvcProtocol.CmdTestGetPIN)
    sayPort('PIN', pin)
    if not compareBits(pin, expectedPin):
        Okej = False
        print(f"error afer pull up")

    for i in pinlist:
        setByteArrayBit(port, i, False)
        setByteArrayBit(expectedPin, i, False)
        setByteArrayBit(ddr, i, True)
        setIoData(SvcProtocol.CmdTestSetPORT, port)
        setIoData(SvcProtocol.CmdTestSetDDR, ddr)
        pin = getIoData(SvcProtocol.CmdTestGetPIN)
        sayPort(f'test PIN {i} {pinId2Str(i)}', pin)
        if not compareBits(pin, expectedPin):
            Okej = False
            print(f"error at {i} {pinId2Str(i)}")
        setByteArrayBit(port, i, True)
        setByteArrayBit(expectedPin, i, True)
        setByteArrayBit(ddr, i, False)
        setIoData(SvcProtocol.CmdTestSetDDR, ddr)
        setIoData(SvcProtocol.CmdTestSetPORT, port)

    if not Okej:
        print(f'node{args.id} Gpio test FAILED check for shorts')
        raise 'FAILED'
    print(f'node{args.id} Gpio test OK')


if args.test_gpios:
    test_gpios()
