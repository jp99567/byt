#!/usr/bin/python3
import struct

import os
from math import ceil
from struct import pack
import argparse
import time
import enum
from copy import deepcopy
import datetime
import core.bytcan as bytcan
import can
from core.config import SvcProtocol
import core.config


parser = argparse.ArgumentParser(description='avr can bus nodes configuration')
parser.add_argument('--candev', default='vcan0')
parser.add_argument('--id', type=int, choices=list(range(1, 31)))
parser.add_argument('--all', action='store_true')
parser.add_argument('--fw_upload', action='store_true')
parser.add_argument('--stat', action='store_true')
parser.add_argument('--config', action='store_true')
parser.add_argument('--dry_run', action='store_true', help='do not send config to bus, check only')
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


fw_build_epoch_base = 1675604744   # 5.feb 20231 14:45:44 CET


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


def dallas_crc8(data):
    crc = 0
    for c in data:
        for i in range(0, 8):
            b = (crc & 1) ^ ((int(c) & (1 << i)) >> i)
            crc = (crc ^ (b * 0x118)) >> 1
    return crc

bus = None


if args.stat or args.config or args.fw_upload:
    bus = bytcan.openBus(args.candev)


def decode_git_version(data):
    if len(data) == 7:
        data.append(0)
        git_ver32bit, build_time = struct.unpack('II', bytearray(data))
        dirty = build_time & 1 == 1
        build_epoch = fw_build_epoch_base + (build_time >> 1) * 60
        build_datetime = datetime.datetime.fromtimestamp(build_epoch)
        return {'dirty': bool(dirty), 'build_epoch': build_epoch, 'build_time': build_datetime, 'rev': git_ver32bit}
    return None


def upload_fw(nodeid):
    avrid1tx = bytcan.canIdFromNodeId(nodeid)
    avrid1rx = avrid1tx + 1
    message = can.Message(arbitration_id=avrid1tx, is_extended_id=True, data=[ord('s')])
    bytcan.sendBlocking(bus, message)
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
            bytcan.sendBlocking(bus, can.Message(arbitration_id=avrid1tx, is_extended_id=True, data=[ord('d')]))
            while page_bytes_in < pgsize:
                msgdata = pgdata[page_bytes_in:page_bytes_in + 8].ljust(8, bytes.fromhex('ff'))
                for i in msgdata:
                    pgxor ^= i
                bytcan.sendBlocking(bus, can.Message(arbitration_id=avrid1tx, is_extended_id=True, data=msgdata))
                page_bytes_in += 8
            rsp = bus.recv()
            if rsp.dlc != 2:
                raise 'invalid size'
            if rsp.data[1] != pgxor:
                raise 'xorsum mismatch'
            pgaddr = page * pgsize
            bytcan.sendBlocking(bus,
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
        return f"unknown: {rsp[0]:X}"


def commandStatus():
    if args.all:
        tran = bytcan.NodeBus(bus, 0)
        rsp = tran.svcTransferBroadcast(SvcProtocol.CmdStatus)
        for node, data in rsp:
            print(f"node:{node} {statusRsp(data)}")
    else:
        tran = bytcan.NodeBus(bus, args.id)
        rsp = tran.svcTransfer(SvcProtocol.CmdStatus)
        if rsp is None:
            print("No response")
        else:
            print(f"{statusRsp(rsp)}")


if args.stat:
    commandStatus()

if args.fw_upload:
    upload_fw(args.id)

if args.config:
    if args.id is None or args.all:
        core.config.configure_all(bus=bus, yamlfile=args.yamlfile, dry_run=args.dry_run)
    else:
        core.config.configure_one(nodeTrans=bytcan.NodeBus(bus, args.id), yamlfile=args.yamlfile, dry_run=args.dry_run)



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

def commandResponse(id, rsp):
    if SvcProtocol(rsp[0]) in (SvcProtocol.CmdTestGetPIN, SvcProtocol.CmdTestGetPORT, SvcProtocol.CmdTestGetDDR):
        out = [f"{v:08b}" for v in rsp[1:]]
        print(f"{out}")
    elif SvcProtocol(rsp[0]) == SvcProtocol.CmdInvalid:
        out = [f"{v:02X}" for v in rsp[1:]]
        print(f"invalid response {out}")
    elif SvcProtocol(rsp[0]) == SvcProtocol.CmdGetGitVersion:
        rev = decode_git_version(rsp[1:])
        if rev is not None:
            rest = f"build time:{rev['build_time']} git version:{rev['rev']:08x}"
            if rev['dirty']:
                rest += '-DIRTY'
            print(f"ID:{id} {rest}")
        else:
            out = [f"{v:02X}" for v in rsp]
            print(f"ID:{id} corrupted git version {out}")
    else:
        out = [f"{v:02X}" for v in rsp]
        print(f"not handled yet: {out}")


if args.cmd:
    if args.all and not args.id:
        trans = bytcan.NodeBus(bytcan.openBus(args.candev), 0)
        data = [int(v, 0) for v in args.msg]
        rsp_list = trans.svcTransfer(SvcProtocol[args.cmd], data)
        for node_id, rsp in rsp_list:
            commandResponse(node_id, rsp)
    else:
        node_id = args.id
        trans = bytcan.NodeBus(bytcan.openBus(args.candev), node_id)
        data = [int(v, 0) for v in args.msg]
        rsp = trans.svcTransfer(SvcProtocol[args.cmd], data)
        commandResponse(node_id, rsp)


if args.exit_bootloader:
    id = bytcan.canid_bcast
    if not args.all:
        id = bytcan.canIdFromNodeId(args.id)
    msg = can.Message(arbitration_id=id, is_extended_id=True, data=[ord('c')])
    bytcan.sendBlocking(bytcan.openBus(args.candev), msg)


def devsimulator():
    mybus = bytcan.openBus(args.candev)
    stage = 0b01000000
    this_can_id = bytcan.canIdFromNodeId(args.id)
    rsp_can_id = this_can_id + 1
    while True:
        mymsg = mybus.recv()
        print(mymsg)
        can_id = arbitration_id=mymsg.arbitration_id
        if this_can_id != can_id and can_id != bytcan.canid_bcast:
            continue

        if mymsg.data[0] == SvcProtocol.CmdStatus:
            bytcan.sendBlocking(mybus, can.Message(arbitration_id=rsp_can_id, is_extended_id=True,
                                            data=[mymsg.data[0], 0, 0, stage]))
            continue
        elif mymsg.data[0] == SvcProtocol.CmdSetStage:
            stage = mymsg.data[1]
        elif mymsg.data[0] == SvcProtocol.CmdSetOwObjRomCode:
            if len(mymsg.data) == 8:
                crc = dallas_crc8([0x28, *mymsg.data[2:8]])
                print(f"rom code: {list(mymsg.data[2:8])} crc {crc:02X}")
                bytcan.sendBlocking(mybus, can.Message(arbitration_id=rsp_can_id, is_extended_id=True,
                                                data=[mymsg.data[0], crc]))
                continue
            else:
                print("invalid ow rom code")
                mymsg.data[0] = SvcProtocol.CmdInvalid

        bytcan.sendBlocking(mybus,
                     can.Message(arbitration_id=mymsg.rsp_can_id, is_extended_id=True, data=[mymsg.data[0]]))


if args.simulator:
    devsimulator()



def sniff():
    bus = bytcan.openBus(args.candev)
    while True:
        msg = bus.recv()
        data = list(msg.data)
        if bytcan.canIdCheck(msg.arbitration_id):
            nodeid, minor = bytcan.canIdCheck(msg.arbitration_id)
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
                        rev = decode_git_version(data[1:])
                        if rev is not None:
                            rest = f"build time:{rev['build_time']} git version:{rev['rev']:08x}"
                            if rev['dirty']:
                                rest += '-DIRTY'

                    print(f"({nodeid},{minor}) {type} {SvcProtocol(data[0]).name} {rest}")
                else:
                    rest = [f"{v:02X}" for v in data]
                    print(f"({nodeid},{minor}) ? {rest}")
        else:
            print(msg)


if args.sniffer:
    sniff()


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

        print(f"found romcode: {sensor.hex()}")
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
    t = bytcan.NodeBus(bytcan.openBus(args.candev), args.id)
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
    t = bytcan.NodeBus(bytcan.openBus(args.candev), args.id)
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
    t = bytcan.NodeBus(bytcan.openBus(args.candev), args.id)
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

    trans = bytcan.NodeBus(bytcan.openBus(args.candev), args.id)

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

    pinlist = list(range(0, pinStr2Num('PG4') + 1))
    pinlist.remove(pinStr2Num('PD5'))
    pinlist.remove(pinStr2Num('PD6'))

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
        sayPort(f'test PIN {i} {pinNum2Str(i)}', pin)
        if not compareBits(pin, expectedPin):
            Okej = False
            print(f"error at {i} {pinNum2Str(i)}")
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
