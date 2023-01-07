#!/usr/bin/python3

import numpy as np
import matplotlib.pyplot as plt
import os
import datetime
import paho.mqtt.client as mqtt
import argparse

parser = argparse.ArgumentParser(description='plot mqtt values over time')
parser.add_argument('--ymin', type=float, help='origin bottom limit of axis Y')
parser.add_argument('--ymax', type=float, help='origin top limit of axis Y')
parser.add_argument('--file', help='line oriented text file with list of topics')
parser.add_argument('--div', type=float, help='time values divisor', default=60)
parser.add_argument('--dur', type=float, help='duration - x axis length', default=120)
parser.add_argument('topics', nargs='+')
args = parser.parse_args()

time_t0 = datetime.datetime.now()
items = args.topics

if args.file is not None:
    print("file: {args.file} ToDo")

print(items)

div = args.div
class Showit:

    tmax = args.dur
    tOrigin = -1
    ymin = None
    ymax = None

    def __init__(self,items):
        n = len(items)
        plt.ion()
        fig, ax = plt.subplots()

        ax.set_xlim(-self.tmax, 0)
        ymininit, ymaxinit = 0, 100
        if args.ymin is not None:
            ymininit = args.ymin
            self.ymin = ymininit
        if args.ymax is not None:
            ymaxinit = args.ymax
            self.ymax = ymaxinit

        ax.set_ylim(ymininit, ymaxinit)
        ax.set_xlabel('h')
        ax.set_ylabel('teplota')
        ax.grid(True)

        e = np.empty((0, n))
        self.lines = ax.plot(e, e)
        print(len(self.lines))
        self.data = []
        for i, line in enumerate(self.lines):
            line.set_label(items[i])
            self.data.append(np.empty((0,2)))
        self.fig = fig
        self.ax = ax

    def show(self, idx, t, y):
        xmin = -self.tmax
        if self.tOrigin == -1:
            self.tOrigin = t

        self.data[idx] = np.append(self.data[idx], [[t, y]], axis=0)

        tmin = t - self.tmax
        if self.data[idx].T[0,0] < tmin:
            cutidx = 0
            for i, v in enumerate(self.data[idx].T[0]):
                if v < tmin:
                    cutidx = i
                else:
                    break
            self.data[idx] = self.data[idx][cutidx+1:]

        for i in range(len(self.lines)):
            trel = np.empty(self.data[i].T[0].size)
            for j in range(len(trel)):
                trel[j] = self.data[i].T[0,j]-t
            self.lines[i].set_xdata(trel)
            self.lines[i].set_ydata(self.data[i].T[1])

        if self.ymax is None:
            self.ymax = y+1
            self.ymin = y-1
        elif self.ymax < y:
            self.ymax = y + 1
        elif self.ymin > y:
            self.ymin = y - 1
        self.ax.set_ylim(self.ymin, self.ymax)
        self.fig.canvas.draw()


def on_connect(client, userdata, flags, rc):
    # This will be called once the client connects
    print(f"Connected with result code {rc}")
    # Subscribe here!
    for i in items:
        if i is not None:
            rv = client.subscribe(i)
            print(f"subscribed {i} with rv:{rv}")


def on_disconnect(client, userdata, rc):
    print(f"disconnected {rc}")


shw = Showit(items)

def on_message(client, userdata, msg):
    current_time = datetime.datetime.now()
    dt = current_time - time_t0
    h = dt.total_seconds()/div
    print(f"mqtt update {h} {msg.topic}({items.index(msg.topic)}): {msg.payload.decode('utf8')}")
    shw.show(items.index(msg.topic), h, float(msg.payload.decode('utf8')))

client = mqtt.Client("mqtt-test5")
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect
client.connect('176.17.53.1')
#connecting = True
#while client.is_connected() or connecting:
#     if client.is_connected():
#         connecting = False
#     client.loop(timeout=1)
#client.loop_forever()
client.loop_start()
try:
    shw.fig.canvas.start_event_loop()
except KeyboardInterrupt:
    print("KeyboardInterrupt")
client.disconnect()
client.loop_stop()
print('mqtt loop exited')
plt.ioff()
plt.show(block=True)
