import can
import time

canid_bcast = 0xEC33F000 >> 3
canid_bcast_mask = ((1 << 20) - 1) << 9

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
    

def openBus(candev):
    return can.Bus(interface='socketcan', channel=candev, receive_own_messages=True)

