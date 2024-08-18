from builtins import exec

import aiomqtt
import anyio
from anyio.abc import CancelScope
from anyio import to_thread
import signal
import selectors
import time
import os
from datetime import datetime
import can

class ScopeExit:
    def __init__(self, fn):
        self.on_exit = fn

    def __enter__(self):
        pass

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.on_exit()


async def move_to_position(targetStr, mqtt):
    topic_position = "ventil4w/position"
    await mqtt.publish(topic_position, "moving", 0, True)
    with ScopeExit(cleanup):
        ok = False
        if not vctrl.inPosition:
            ok = await to_thread.run_sync(vctrl.homePos)


async def signal_handler(scope: CancelScope):
    with anyio.open_signal_receiver(signal.SIGINT, signal.SIGTERM) as signals:
        async for signum in signals:
            if signum == signal.SIGINT:
                print("\nCtrl+C pressed!")
            else:
                print('Terminated!')


            if not vctrl.moving and vctrl.inPosition:
                with open(last_position_file, "w") as f:
                    f.write(porty[vctrl.curPort] + "\n")
                    print(f"stored last position {porty[vctrl.curPort]}")

            scope.cancel()
            return


canbus = can.Bus(interface='socketcan', channel='vcan0')


def todo():
    data = canbus.recv()
    print(f"recv {data}")
    pass


def do_sync_io_multiplex():
    canfd = canbus.fileno()
    sel = selectors.DefaultSelector()
    print(f"can  thread {canbus}")
    sel.register(canfd, selectors.EVENT_READ, todo)
    # sel.register(fd2, selectors.EVENT_READ, self.read_pin2)
    while True:
        events = sel.select()
        if events:
            print(f"select events {events}")
            for key, mask in events:
                key.data()


async def start_sync_io_multiplex():
    await to_thread.run_sync(do_sync_io_multiplex)
    pass


pru_conn = None
async def pru_handle(client):
    async with client:
        print("pru connected")
        global pru_conn
        if pru_conn is None:
            pru_conn = client
            while True:
                data = await client.receive(64)
                print(f"pru received: {len(data)} {data}")


pru_sim_socket_path = '/tmp/pru_sim_socket'
async def start_pru_sim_listener():
    listener = await anyio.create_unix_listener(pru_sim_socket_path)
    print("listening")
    await listener.serve(pru_handle)


async def start_mqtt(tg):
    async with aiomqtt.Client("localhost", identifier="test1258") as client:
        await client.subscribe("ha_sim/#")
        async for msg in client.messages:
            print(f"mqtt msg {msg.payload.decode()}")


async def main() -> None:
    async with anyio.create_task_group() as tg:
        tg.start_soon(start_mqtt, tg)
        tg.start_soon(signal_handler, tg.cancel_scope)
        tg.start_soon(start_pru_sim_listener)
        tg.start_soon(start_sync_io_multiplex)


anyio.run(main)
os.remove(pru_sim_socket_path)
