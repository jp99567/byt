#!/usr/bin/python3

import can
import os
from math import ceil
from struct import pack, unpack
import argparse
import time
import yaml
import enum

parser = argparse.ArgumentParser(description='avr can node simulator')
parser.add_argument('--candev', default='vcan0')

args = parser.parse_args()
canconf_bcast = 0xEC33F000 >> 3

class SvcProtocol(enum.Enum):
    CmdSetAllocCounts = enum.auto()
    CmdGetAllocCounts = enum.auto()
    CmdSetStage = enum.auto()
    CmdStatus = ord('s')
    CmdInvalid = ord('X')


def canconfidfromnodeid(nodeid):
    return canconf_bcast | (nodeid << 2)

def parseStatusResponse(data):
    if len(data) < 2 or data[0] != SvcProtocol.CmdStatus:
        raise Exception(f"not a status response {data}")
    stage = (data[1] >> 6) & 0b11
    if stage == 0:
        print(f"stage bootloader")
    elif stage == 1:
        print(f"stage1")
    elif stage == 2:
        print(f"stage2")
    else:
        print(f"stage running")


class NodeBus:
    def __init__(self, canbus, nodeid):
        self.bus = canbus
        self.canid = canconfidfromnodeid(nodeid)
        self.nodeid = nodeid

    def svcTransfer(self, cmd, data):
        print(f"can send {cmd.name} {data}")
        #return
        message = can.Message(arbitration_id=self.canid, is_extended_id=True, data=[cmd.value, *data, ])
        self.bus.send(message)
        timeout = 1.5
        abs_timeout = time.time() + timeout
        max_count = 20
        while max_count or timeout <= 0:
            max_count -= 1
            messagein = bus.recv(timeout)
            if messagein is None:
                raise Exception(f"no response nodeid:{self.nodeid} canid:{self.canid:X}")
            if messagein.arbitration_id == self.canid+1:
                return messagein.data
            timeout = abs_timeout - time.time()
        raise Exception(f"no response (or bus flooded) nodeid:{self.nodeid} canid:{self.canid:X}")

bus = can.Bus(interface='socketcan', channel=args.candev, receive_own_messages=False)

stage = 1
while True:
    msg = bus.recv()
    print(msg)
    if msg.data[0] == SvcProtocol.CmdStatus:
        bus.send(can.Message(arbitration_id=msg.arbitration_id+1, is_extended_id=True, data=[msg.data[0], stage]))
        continue
    elif msg.data[0] == SvcProtocol.CmdSetStage:
        stage = msg.data[1]
    bus.send(can.Message(arbitration_id=msg.arbitration_id + 1, is_extended_id=True, data=[msg.data[0]]))

