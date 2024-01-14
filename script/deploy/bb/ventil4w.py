from builtins import exec

import aiomqtt
import anyio
from anyio.abc import CancelScope
from anyio import to_thread
import signal
import gpiod
import selectors
import serial
import time
import os


class Spe6103:
    def __init__(self):
        try:
            self.ser = serial.Serial(port='/dev/ttyUSB0', baudrate=115200, timeout=0.5)
        except serial.SerialException as msg:
            print(msg)

    def readL(self):
        buf = bytearray()
        while True:
            byte = self.ser.read()
            if len(byte) < 1:
                print(f"invalid read1 {byte}")
                break

            if byte[0] == ord('\n'):
                break
            buf.extend(byte)
        return buf.decode()

    def reqrsp(self, req):
        n = self.ser.write(f"{req}\n".encode())
        if n != len(req) + 1:
            print(f"invalid write {n}")
        return self.readL()


    def cmdOneWay(self, cmd):
        n = self.ser.write(f"{cmd}\n".encode())
        if n != len(cmd) + 1:
            print(f"invalid write {n}")

    def flush(self):
        while True:
            unsol = self.readL()
            if len(unsol) < 1:
                print("spe6103 link clear")
                return
            print(f"garbage: {unsol}")

    def check(self):
        self.flush()
        rsp = self.reqrsp("MEAS:SCALAR:ALL:INFO?")
        valvec = rsp.split(',')
        if len(valvec) != 7:
            print(f"unexpected {rsp}")
            return False

        v = float(valvec[0])
        i = float(valvec[1])
        w = float(valvec[2])
        mode = int(valvec[6])
        modetxt = ('standby', 'CV', 'CC')
        rsp = self.reqrsp("OUTP?").strip()
        print(f"{v}V {i}A {w}W {modetxt[mode]} output:{rsp}")
        return rsp == "0" or rsp == "OFF"


porty = ('Kotol', 'StudenaVoda', 'Boiler', 'KotolBoiler')


def calcMotion(cur, dst):
    dist = dst - cur
    rev = dist > 0
    if abs(dist) > 2:
        rev = not rev
        dist = 1
    return rev, abs(dist)


# forward: AbsMArk -> K -> Voda -> B -> KB -> AbsMArk
def expectedAbsMark(curPos, fwDIR):
    if curPos == 3 and fwDIR:
        return True
    if curPos == 0 and not fwDIR:
        return True
    return False


class Vctrl:
    curPort = 0
    inPosition = False
    ignore = True
    ev1 = None
    ev2 = None
    moving = False

    def __init__(self):
        self.chip = gpiod.Chip('/dev/gpiochip3')
        print(f"chip: {self.chip}")
        self.inp1 = self.chip.get_line(15)
        self.inp2 = self.chip.get_line(16)
        self.outDir1 = self.chip.get_line(14)
        self.outDir2 = self.chip.get_line(17)
        self.inp1.request("t2", type=gpiod.LINE_REQ_EV_BOTH_EDGES)
        self.inp2.request("t2", type=gpiod.LINE_REQ_EV_BOTH_EDGES)
        self.outDir1.request("t2", type=gpiod.LINE_REQ_DIR_OUT)
        self.outDir1.set_value(False)
        self.outDir2.request("t2", type=gpiod.LINE_REQ_DIR_OUT)
        self.outDir2.set_value(False)
        self.sel = selectors.DefaultSelector()
        fd1 = self.inp1.event_get_fd()
        fd2 = self.inp2.event_get_fd()
        print(f"fd: {fd1} {fd2}")
        self.sel.register(fd1, selectors.EVENT_READ, self.read_pin1)
        self.sel.register(fd2, selectors.EVENT_READ, self.read_pin2)
        self.spe = Spe6103()

    def inPortPosition(self):
        return bool(self.inp1.get_value())
    def motorStop(self):
        self.outDir1.set_value(False)
        self.outDir2.set_value(False)
        self.spe.cmdOneWay("OUTP 0")
        print("motorStoped")

    def motorStart(self, forward):
        print(f"motorStart fw:{forward}")
        self.spe.cmdOneWay("OUTP 1")
        if forward:
            self.outDir1.set_value(True)
        else:
            self.outDir2.set_value(True)

    def read_pin1(self):
        ev = self.inp1.event_read()
        if self.ignore:
            print(f"unexpected gpio1 {ev}")
            return
        print(f"gpio1 {self.inp1} {ev}")
        self.ev1 = ev.type

    def read_pin2(self):
        ev = self.inp2.event_read()
        if self.ignore:
            print(f"unexpected gpio2 {ev}")
            return
        print(f"gpio2 {ev}")
        self.ev2 = ev.type

    def readgpio(self, to):
        events = self.sel.select(to)
        self.ev1, self.ev2 = None, None
        if events:
            for key, mask in events:
                key.data()
        else:
            print(f'poll gpio timeout {to}s')
            return False
        return True

    def homePos(self):
        print(f"move to home position Kotol")
        self.flush()

        if not self.spe.check():
            return False

        absMarkReached = bool(self.inp2.get_value())

        try:
            self.motorStart(True)

            dist = 0
            targetDistance = 1 if absMarkReached else 4
            while dist < targetDistance:
                if self.readgpio(3):
                    if self.ev1 == gpiod.LineEvent.RISING_EDGE:
                        dist += 1
                    if self.ev2 == gpiod.LineEvent.RISING_EDGE:
                        if not absMarkReached:
                            absMarkReached = True
                            targetDistance = dist + 1
                else:
                    print(f"process timeout {dist}")
                    break
            print(f"homepos {absMarkReached} {dist}")
        finally:
            self.motorStop()
        time.sleep(0.1)
        if self.inp1.get_value() and absMarkReached:
            self.inPosition = True
            self.curPort = 0
            return True
        return False

    def move(self, target):
        print(f"move to target {porty[self.curPort]} ==> {porty[target]}")
        self.flush()

        try:
            if not self.checkInitConditions():
                return False
        except:
            print("some exception")
            return False

        print("check1")
        forwardDirection, targetDist = calcMotion(self.curPort, target)
        inkrement = 1 if forwardDirection else -1
        posError = False
        print(f"check2 {targetDist} {forwardDirection} {inkrement} {self.curPort} {self.inPosition}")
        try:
            self.motorStart(forwardDirection)
            exitPosition = False
            if self.readgpio(0.5):
                if self.ev1 == gpiod.LineEvent.FALLING_EDGE:
                    exitPosition = True
                    self.inPosition = False
            dist = 0
            absMarkHit = bool(self.inp2.get_value())
            expectAbsMark = expectedAbsMark(self.curPort, forwardDirection)
            while exitPosition and dist < targetDist:
                if self.readgpio(3):
                    if self.ev1 == gpiod.LineEvent.RISING_EDGE:
                        dist += 1
                        self.curPort = (self.curPort + inkrement + 4) % 4
                        if expectAbsMark != absMarkHit:
                            posError = True
                            break
                        expectAbsMark = expectedAbsMark(self.curPort, forwardDirection)
                        absMarkHit = False
                    if self.ev2 == gpiod.LineEvent.RISING_EDGE:
                        absMarkHit = True
                else:
                    print(f"process timeout {dist}")
                    break
            print(f"end {exitPosition} {dist}")
        finally:
            self.motorStop()
        time.sleep(0.1)
        if posError:
            return False
        if not self.inp1.get_value():
            return False
        return self.curPort == target

    def checkInitConditions(self):
        print(f"test init cond {self.inp1.get_value()} {self.inp2.get_value()}")

        if not self.inp1.get_value():
            print("test init cond mimo portu")
            self.inPosition = False
            return False

        if self.inp2.get_value():
            print("test init Abs Mark")
            self.inPosition = False
            return False

        return self.spe.check()


    def flush(self):
        self.ignore = True
        while True:
            if not self.readgpio(0.1):
                break
        self.ignore = False


vctrl = Vctrl()
vctrl.flush()


class ScopeExit:
    def __init__(self, fn):
        self.on_exit = fn

    def __enter__(self):
        pass

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.on_exit()


async def move_to_position(targetStr, mqtt):

    try:
        target = porty.index(targetStr)
    except ValueError:
        try:
            target = int(targetStr)
            if not 0 <= target < len(porty):
                return
        except ValueError:
            return

    if vctrl.moving:
        return
    if target == vctrl.curPort:
        return
    vctrl.moving = True
    topic_position = "ventil4w/position"
    await mqtt.publish(topic_position, "moving")

    def cleanup():
        vctrl.moving = False

    with ScopeExit(cleanup):
        if not vctrl.inPosition:
            ok = await to_thread.run_sync(vctrl.homePos)
            if not ok:
                await mqtt.publish(topic_position, "error")
                return
        ok = await to_thread.run_sync(vctrl.move, target)
        print(f"move ok:{ok}")
        if not ok:
            await mqtt.publish(topic_position, "error")
            return
        await mqtt.publish(topic_position, porty[vctrl.curPort])


last_position_file = 'ventil4way-last-position'


async def signal_handler(scope: CancelScope):
    with anyio.open_signal_receiver(signal.SIGINT, signal.SIGTERM) as signals:
        async for signum in signals:
            if signum == signal.SIGINT:
                print("\nCtrl+C pressed!")
            else:
                print('Terminated!')

            print(f"check {vctrl.moving} {vctrl.inPosition} {vctrl.inPortPosition()}")
            if not vctrl.moving and vctrl.inPosition and vctrl.inPortPosition():
                with open(last_position_file, "w") as f:
                    f.write(porty[vctrl.curPort]+"\n")
                    print(f"stored last position {porty[vctrl.curPort]}")

            scope.cancel()
            return


async def start_mqtt(tg):
    async with aiomqtt.Client("linksys", client_id="test1258") as client:
        async with client.messages() as messages:
            await client.subscribe("ventil4w/target")
            async for msg in messages:
                tg.start_soon(move_to_position, msg.payload.decode(), client)


async def main() -> None:
    async with anyio.create_task_group() as tg:
        tg.start_soon(start_mqtt, tg)
        tg.start_soon(signal_handler, tg.cancel_scope)

try:
    with open(last_position_file) as f:
        postxt = f.readline().strip()
        pos = porty.index(postxt)
        if vctrl.inPortPosition():
            vctrl.curPort = pos
            vctrl.inPosition = True
            print(f"loaded last position {postxt}")
except FileNotFoundError:
    pass

try:
    os.remove(last_position_file)
except FileNotFoundError:
    pass


anyio.run(main)
