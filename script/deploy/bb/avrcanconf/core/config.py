import core.bytcan
import yaml
import enum
from struct import pack


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
    CmdSetSCD41Params = enum.auto()
    CmdSetSHT11Params = enum.auto()
    CmdSCD41Test = enum.auto()
    CmdFlushAll = enum.auto()
    CmdStatus = ord('s')
    CmdInvalid = ord('X')


class NodeStage(enum.Enum):
    stage0boot = 0
    stage1 = 0b01000000
    stage2 = 0b10000000
    stage3run = 0b11000000

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
        errmsg = f"initNodeObject {self.node}"
        raise NotImplementedError(errmsg)

    def checkPinConflits(self, isSCD41, isSHT11, isOw):
        pass


def pinNum2Str(num):
    if 0 <= num < 6 * 8 + 5:
        byte = num // 8
        bit = num % 8
        p = ord('A') + byte
        return 'P' + chr(p) + f"{bit}"
    raise "pin id out fo range"


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

    def checkPinConflits(self, isSCD41=False, isSHT11=False, isOw=False):
        usedPins = []
        if isSCD41:
            usedPins.extend(['PD0', 'PD1'])
        if isSHT11:
            usedPins.extend(['PC3', 'PC5'])
        if isOw:
            usedPins.append('PA0')

        for i in self.node.keys():
            pin = self.node[i]['pin']
            if pin in usedPins:
                errmsg = f"pin conflict {pin}"
                raise RuntimeError(errmsg)


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
                    raise RuntimeError(f'Inwalid crc {list(rc)} {rc[7]} {rsp[1]}')
            idx += 1


class ClassSensorionInfo(ClassInfo):
    def initNodeObjectSensorion(self, nodebus, mobs, mobSize, cmd):
        canid = self.node['addr']
        mobidx = mobs.index(canid)
        byteidx = mobSize[canid]['start']
        nodebus.svcTransfer(cmd, [mobidx, byteidx])


class ClassSensorionSCD41Info(ClassSensorionInfo):
    def info(self):
        return {'ids': {self.node['addr']}, 'count': 1, 'items': {self.node['addr']: [[0, 3*16]]}}

    def initNodeObject(self, nodebus, mobs, mobSize):
        self.initNodeObjectSensorion(nodebus, mobs, mobSize, SvcProtocol.CmdSetSCD41Params)


class ClassSensorionSHT11Info(ClassSensorionInfo):
    def info(self):
        return {'ids': {self.node['addr']}, 'count': 1, 'items': {self.node['addr']: [[0, 4 * 8]]}}

    def initNodeObject(self, nodebus, mobs, mobSize):
        self.initNodeObjectSensorion(nodebus, mobs, mobSize, SvcProtocol.CmdSetSHT11Params)


def buildClassInfo(trieda, node):
    if trieda == 'DigIN':
        return ClassDigInOutInfo(node, SvcProtocol.CmdSetDigINObjParams)
    elif trieda == 'DigOUT':
        return ClassDigInOutInfo(node, SvcProtocol.CmdSetDigOUTObjParams)
    elif trieda == 'OwT':
        return ClassOwtInfo(node)
    elif trieda == 'SensorionSCD41':
        return ClassSensorionSCD41Info(node)
    elif trieda == 'SensorionSHT11':
        return ClassSensorionSHT11Info(node)
    else:
        return None


class ConfigNode:

    def __init__(self, node):
        self.canmobsList = set()
        self.canmobSize = dict()
        nodeId = node['id']
        outputCanIds = set()
        self.inputCanIds = set()
        self.triedyNr = dict()
        triedyInfo = dict()
        self.triedy = dict()

        for trieda in ('DigIN', 'OwT', 'SensorionSCD41', 'SensorionSHT11'):
            if trieda in node:
                triedaInfo = buildClassInfo(trieda, node[trieda])
                info = triedaInfo.info()
                outputCanIds.update(info['ids'])
                self.triedyNr[trieda] = info['count']
                triedyInfo[trieda] = info
                self.triedy[trieda] = triedaInfo
            else:
                self.triedyNr[trieda] = 0

        for trieda in ('DigOUT',):
            if trieda in node:
                triedaInfo = buildClassInfo(trieda, node[trieda])
                info = triedaInfo.info()
                self.inputCanIds.update(info['ids'])
                self.triedyNr[trieda] = info['count']
                triedyInfo[trieda] = info
                self.triedy[trieda] = triedaInfo
            else:
                self.triedyNr[trieda] = 0

        print(f"nodeId:{nodeId} {self.triedyNr} outputCanIdNr:{len(outputCanIds)} inputCanIdNr:{len(self.inputCanIds)}")
        print(triedyInfo)
        canmobs = dict()
        for trieda in triedyInfo:
            for canid, items in triedyInfo[trieda]['items'].items():
                if canid not in canmobs:
                    canmobs[canid] = items
                else:
                    canmobs[canid].extend(items)

        for id in canmobs:
            canmobs[id].sort(key=lambda e: e[0])
            prev_end = 0
            for i in canmobs[id]:
                if prev_end > i[0]:
                    raise Exception(f"canid:{id:X} overlaps at bit {i[0]} items {canmobs[id]}")
                prev_end = i[0] + i[1]
            if prev_end > 8 * 8:
                raise Exception(f"canid:{id:X} exceeds 8B items:({canmobs[id]})")
            self.canmobSize[id] = {'size' : (prev_end + 7) // 8}

        if not self.inputCanIds.isdisjoint(outputCanIds):
            raise Exception(f"IN/OUT can ids mixed {self.inputCanIds} {outputCanIds}")

        self.canmobsList = sorted(self.inputCanIds)
        self.canmobsList.extend(sorted(outputCanIds))
        print(self.canmobsList)

        startOffset = 0
        for mobcanid in self.canmobsList:
            size = self.canmobSize[mobcanid]['size']
            self.canmobSize[mobcanid]['start'] = startOffset
            startOffset += size

        print(self.canmobSize)

    def check(self):
        def checkIt(trieda):
            self.triedy[trieda].checkPinConflits(isSCD41=self.triedyNr['SensorionSCD41'],
                                                 isSHT11=self.triedyNr['SensorionSHT11'],
                                                 isOw=self.triedyNr['OwT'])

        if self.triedyNr['DigIN']:
            checkIt('DigIN')
        if self.triedyNr['DigOUT']:
            checkIt('DigOUT')


    def uploadNode(self, trans):
        features = 0
        if self.triedyNr['SensorionSCD41']:
            features |= 1
        if self.triedyNr['SensorionSHT11']:
            features |= 2
        trans.svcTransfer(SvcProtocol.CmdSetAllocCounts,
                          [self.triedyNr['DigIN'],
                           self.triedyNr['DigOUT'],
                           self.triedyNr['OwT'],
                           sum([self.canmobSize[i]['size'] for i in self.canmobSize]),
                           len(self.inputCanIds),
                           len(self.canmobsList),
                           features])

        trans.svcTransfer(SvcProtocol.CmdSetStage, [NodeStage.stage2.value])

        for trieda in self.triedy:
            self.triedy[trieda].initNodeObject(trans, self.canmobsList, self.canmobSize)

        for idx, canid in enumerate(self.canmobsList):
            endIoIdx = self.canmobSize[canid]['start'] + self.canmobSize[canid]['size']
            data = pack('BBH', idx, endIoIdx, canid)
            trans.svcTransfer(SvcProtocol.CmdSetCanMob, data)


def configure_one(nodeTrans, yamlfile, dry_run):
    node_id = core.bytcan.nodeIfFromCanId(nodeTrans.canid)
    if node_id == 0 :
        raise "broadcast not allowed"
    with open(yamlfile, "r") as stream:
        config = yaml.safe_load(stream)
        for node in config['NodeCAN']:
            if config['NodeCAN'][node]['id'] != node_id:
                continue
            print(f"node name: {node}")
            nodeConf = ConfigNode(config['NodeCAN'][node])
            nodeConf.check()

            if not dry_run:
                nodeConf.uploadNode(nodeTrans)
            return
    raise RuntimeError("node not configured")


def configure_all(bus, yamlfile, dry_run):
    with open(yamlfile, "r") as stream:
        config = yaml.safe_load(stream)
        nodesConfs = dict()
        allMobsIds = set()
        for node in config['NodeCAN']:
            print(f"node name: {node}")
            nodeConf = ConfigNode(config['NodeCAN'][node])
            nodeConf.check()
            nodesConfs[config['NodeCAN'][node]['id']] = nodeConf
            if not allMobsIds.isdisjoint(nodeConf.canmobsList):
                errmsg = f"duplicitne ids {nodeConf.canmobsList} v {node}"
                raise RuntimeError(errmsg)
            allMobsIds.update(nodeConf.canmobsList)

        if not dry_run:
            for nodeId in nodesConfs:
                nodeTrans = core.bytcan.NodeBus(bus, nodeId)
                nodesConfs[nodeId].uploadNode(nodeTrans)

