#!/usr/bin/env python3
"""CAN Node Simulator

Simulates CAN nodes described in config.yaml for communication with bytd.
Uses anyio with Trio backend, SocketCAN (raw Linux socket), and MQTT (paho).

CAN items from config are categorised into:
  - Items sent TO bytd (DigIN, OwT, SensorionSHT11, SensorionSCD41):
      Simulator subscribes to MQTT topic ``cansim/ctrl/<itemName>`` to
      receive physical values, converts them to raw CAN representation
      and periodically transmits the corresponding CAN frames.
  - Items received FROM bytd (DigOUT, Pwm16At90):
      Simulator decodes received CAN frames and publishes decoded values
      to MQTT topic ``cansim/stat/<itemName>``.

Usage:
    python can_nodes_sim.py [--can-if vcan0] [--mqtt-host localhost] [--mqtt-port 1883]
"""

from __future__ import annotations

import argparse
import logging
import math
import queue
import signal
import socket
import struct
import sys
from dataclasses import dataclass, field
from typing import Any, Callable

import anyio
import paho.mqtt.client as paho_mqtt
import yaml

logger = logging.getLogger("cansim")

# ---------------------------------------------------------------------------
# SocketCAN helpers
# ---------------------------------------------------------------------------
# struct can_frame { canid_t can_id; __u8 can_dlc; __u8 __pad, __res0, __res1; __u8 data[8]; };
CAN_FRAME_FMT = "=IB3x8s"
CAN_FRAME_SIZE = struct.calcsize(CAN_FRAME_FMT)

MQTT_PREFIX_STAT = "cansim/stat/"
MQTT_PREFIX_CTRL = "cansim/ctrl/"

# Periodic TX interval in seconds
TX_INTERVAL = 1.0


def pack_can_frame(can_id: int, data: bytes | bytearray, dlc: int | None = None) -> bytes:
    if dlc is None:
        dlc = min(len(data), 8)
    padded = bytes(data[:8]).ljust(8, b"\x00")
    return struct.pack(CAN_FRAME_FMT, can_id, dlc, padded)


def unpack_can_frame(raw: bytes) -> tuple[int, bytes, int]:
    can_id, dlc, data = struct.unpack(CAN_FRAME_FMT, raw)
    return can_id, data[:dlc], dlc


# ---------------------------------------------------------------------------
# Reverse-conversion helpers  (physical value → raw CAN int16/uint16)
# ---------------------------------------------------------------------------

def owt_to_raw(temp_celsius: float, factor: float) -> int:
    """Convert temperature in °C to int16 raw value for 1-Wire sensors."""
    raw = int(round(temp_celsius * factor))
    return max(-32768, min(32767, raw))


def sht11_t_to_raw(temp_celsius: float) -> int:
    """Reverse of convSHT11_T: adc = (T − d1) / d2"""
    d1, d2 = -39.6, 0.01
    adc = int(round((temp_celsius - d1) / d2))
    return max(0, min(65535, adc))


def sht11_rh_to_raw(rh_percent: float) -> int:
    """Reverse of convSHT11_RH (linear approx): adc ≈ (RH − c1) / c2"""
    c1, c2 = -2.0468, 0.0367
    adc = int(round((rh_percent - c1) / c2))
    return max(0, min(65535, adc))


def scd41_t_to_raw(temp_celsius: float) -> int:
    """Reverse of convSCD41_T: adc = (T + 45) / 175 * 65535"""
    adc = int(round((temp_celsius + 45) / 175 * 65535))
    return max(0, min(65535, adc))


def scd41_rh_to_raw(rh_percent: float) -> int:
    """Reverse of convSCD41_RH: adc = RH / 100 * 65535"""
    adc = int(round(rh_percent / 100 * 65535))
    return max(0, min(65535, adc))


def scd41_co2_to_raw(co2_ppm: float) -> int:
    """Reverse of convSCD41_CO2: adc = CO2 (identity)"""
    return max(0, min(65535, int(round(co2_ppm))))


# ---------------------------------------------------------------------------
# TX buffer — aggregates CAN data for frames the simulator transmits
# ---------------------------------------------------------------------------

class TxBuffer:
    """Maintains per-CAN-ID data buffers for outgoing frames."""

    def __init__(self) -> None:
        self.frames: dict[int, bytearray] = {}
        self.sizes: dict[int, int] = {}

    def ensure(self, can_id: int, min_size: int) -> None:
        if can_id not in self.frames:
            self.frames[can_id] = bytearray(8)
            self.sizes[can_id] = min_size
        else:
            self.sizes[can_id] = max(self.sizes[can_id], min_size)

    def set_bit(self, can_id: int, offset: int, bit: int, value: bool) -> None:
        mask = 1 << bit
        if value:
            self.frames[can_id][offset] |= mask
        else:
            self.frames[can_id][offset] &= ~mask

    def set_int16(self, can_id: int, offset: int, value: int) -> None:
        struct.pack_into("<h", self.frames[can_id], offset, value)

    def set_uint16(self, can_id: int, offset: int, value: int) -> None:
        struct.pack_into("<H", self.frames[can_id], offset, value)


# ---------------------------------------------------------------------------
# RX handler — decodes CAN frames from bytd and publishes to MQTT
# ---------------------------------------------------------------------------

class RxDispatcher:
    """Decodes received CAN frames (output items from bytd) and publishes state to MQTT."""

    def __init__(self, mqtt_client: paho_mqtt.Client) -> None:
        self.mqtt = mqtt_client
        self._decoders: dict[int, list[Callable[[bytes, int], None]]] = {}

    def add_digi_out(self, can_id: int, offset: int, bit: int, name: str) -> None:
        mask = 1 << bit

        def decode(data: bytes, dlc: int) -> None:
            if offset < dlc:
                val = 1 if (data[offset] & mask) else 0
                self.mqtt.publish(f"{MQTT_PREFIX_STAT}{name}", str(val), retain=True)
                logger.debug("RX DigOUT %s = %d", name, val)

        self._decoders.setdefault(can_id, []).append(decode)

    def add_pwm16(self, can_id: int, offset: int, name: str) -> None:
        def decode(data: bytes, dlc: int) -> None:
            if offset + 2 <= dlc:
                val = struct.unpack_from("<H", data, offset)[0]
                self.mqtt.publish(f"{MQTT_PREFIX_STAT}{name}", str(val), retain=True)
                logger.debug("RX Pwm16 %s = %d", name, val)

        self._decoders.setdefault(can_id, []).append(decode)

    def dispatch(self, can_id: int, data: bytes, dlc: int) -> None:
        decoders = self._decoders.get(can_id)
        if decoders:
            for dec in decoders:
                dec(data, dlc)


# ---------------------------------------------------------------------------
# MQTT → TX buffer binding (input items: DigIN, OwT, Sensorion)
# ---------------------------------------------------------------------------

class MqttInputBinder:
    """Binds MQTT control topics to TX buffer updates for items sent to bytd."""

    def __init__(self, tx: TxBuffer) -> None:
        self.tx = tx
        self.handlers: dict[str, Callable[[str], None]] = {}

    # -- DigIN ---------------------------------------------------------------
    def add_digi_in(self, can_id: int, offset: int, bit: int, name: str) -> None:
        topic = f"{MQTT_PREFIX_CTRL}{name}"

        def handle(payload: str) -> None:
            val = bool(int(float(payload)))
            self.tx.set_bit(can_id, offset, bit, val)
            logger.debug("MQTT→TX DigIN %s = %s", name, val)

        self.handlers[topic] = handle

    # -- OwT -----------------------------------------------------------------
    def add_owt(self, can_id: int, offset: int, name: str, factor: float) -> None:
        topic = f"{MQTT_PREFIX_CTRL}{name}"

        def handle(payload: str) -> None:
            temp = float(payload)
            if math.isnan(temp):
                return
            raw = owt_to_raw(temp, factor)
            self.tx.set_int16(can_id, offset, raw)
            logger.debug("MQTT→TX OwT %s = %.2f (raw %d)", name, temp, raw)

        self.handlers[topic] = handle

    # -- SensorionSHT11 ------------------------------------------------------
    def add_sht11(self, can_id: int, name_t: str, name_rh: str) -> None:
        topic_t = f"{MQTT_PREFIX_CTRL}{name_t}"
        topic_rh = f"{MQTT_PREFIX_CTRL}{name_rh}"

        def handle_t(payload: str) -> None:
            temp = float(payload)
            raw = sht11_t_to_raw(temp)
            self.tx.set_uint16(can_id, 0, raw)
            logger.debug("MQTT→TX SHT11 T %s = %.2f (raw %d)", name_t, temp, raw)

        def handle_rh(payload: str) -> None:
            rh = float(payload)
            raw = sht11_rh_to_raw(rh)
            self.tx.set_uint16(can_id, 2, raw)
            logger.debug("MQTT→TX SHT11 RH %s = %.1f%% (raw %d)", name_rh, rh, raw)

        self.handlers[topic_t] = handle_t
        self.handlers[topic_rh] = handle_rh

    # -- SensorionSCD41 ------------------------------------------------------
    def add_scd41(self, can_id: int, name_t: str, name_rh: str, name_co2: str) -> None:
        topic_t = f"{MQTT_PREFIX_CTRL}{name_t}"
        topic_rh = f"{MQTT_PREFIX_CTRL}{name_rh}"
        topic_co2 = f"{MQTT_PREFIX_CTRL}{name_co2}"

        def handle_t(payload: str) -> None:
            temp = float(payload)
            raw = scd41_t_to_raw(temp)
            self.tx.set_uint16(can_id, 0, raw)
            logger.debug("MQTT→TX SCD41 T %s = %.2f (raw %d)", name_t, temp, raw)

        def handle_rh(payload: str) -> None:
            rh = float(payload)
            raw = scd41_rh_to_raw(rh)
            self.tx.set_uint16(can_id, 2, raw)
            logger.debug("MQTT→TX SCD41 RH %s = %.1f%% (raw %d)", name_rh, rh, raw)

        def handle_co2(payload: str) -> None:
            co2 = float(payload)
            raw = scd41_co2_to_raw(co2)
            self.tx.set_uint16(can_id, 4, raw)
            logger.debug("MQTT→TX SCD41 CO2 %s = %.0f ppm (raw %d)", name_co2, co2, raw)

        self.handlers[topic_t] = handle_t
        self.handlers[topic_rh] = handle_rh
        self.handlers[topic_co2] = handle_co2

    # -- dispatch incoming MQTT message --------------------------------------
    def on_message(self, topic: str, payload: str) -> None:
        handler = self.handlers.get(topic)
        if handler is not None:
            try:
                handler(payload)
            except Exception:
                logger.exception("Error handling MQTT message on %s", topic)


# ---------------------------------------------------------------------------
# Config parsing — build items from config.yaml
# ---------------------------------------------------------------------------

def build_items(config: dict, tx: TxBuffer, rx: RxDispatcher, binder: MqttInputBinder) -> list[str]:
    """Parse NodeCAN from *config* and register all items.

    Returns list of MQTT topics the simulator should subscribe to.
    """
    subscribe_topics: list[str] = []
    nodes = config.get("NodeCAN", {})

    for node_name, node in nodes.items():
        node_id = node.get("id")
        logger.info("Node %s  id=%s", node_name, node_id)

        # -- DigIN (sim → bytd) -----------------------------------------------
        if "DigIN" in node:
            for item_name, item in node["DigIN"].items():
                can_id = item["addr"][0]
                offset = item["addr"][1]
                bit = item["addr"][2]
                min_size = offset + 1
                tx.ensure(can_id, min_size)
                binder.add_digi_in(can_id, offset, bit, item_name)
                subscribe_topics.append(f"{MQTT_PREFIX_CTRL}{item_name}")
                logger.info("  DigIN  %-25s  CAN 0x%03X [%d].%d", item_name, can_id, offset, bit)

        # -- DigOUT (bytd → sim) -----------------------------------------------
        if "DigOUT" in node:
            for item_name, item in node["DigOUT"].items():
                can_id = item["addr"][0]
                offset = item["addr"][1]
                bit = item["addr"][2]
                rx.add_digi_out(can_id, offset, bit, item_name)
                logger.info("  DigOUT %-25s  CAN 0x%03X [%d].%d", item_name, can_id, offset, bit)

        # -- OwT (sim → bytd) -------------------------------------------------
        if "OwT" in node:
            for item_name, item in node["OwT"].items():
                can_id = item["addr"][0]
                offset = item["addr"][1]
                factor = 16.0
                if item.get("type") == "DS18S20":
                    factor = 2.0
                min_size = offset + 2
                tx.ensure(can_id, min_size)
                binder.add_owt(can_id, offset, item_name, factor)
                subscribe_topics.append(f"{MQTT_PREFIX_CTRL}{item_name}")
                logger.info("  OwT    %-25s  CAN 0x%03X [%d] factor=%.0f", item_name, can_id, offset, factor)

        # -- SensorionSHT11 (sim → bytd) --------------------------------------
        if "SensorionSHT11" in node:
            sht = node["SensorionSHT11"]
            can_id = sht["addr"]
            name_t = sht["nameT"]
            name_rh = sht["nameRH"]
            tx.ensure(can_id, 4)  # T(2B) + RH(2B)
            binder.add_sht11(can_id, name_t, name_rh)
            subscribe_topics.append(f"{MQTT_PREFIX_CTRL}{name_t}")
            subscribe_topics.append(f"{MQTT_PREFIX_CTRL}{name_rh}")
            logger.info("  SHT11  T=%-20s RH=%-20s CAN 0x%03X", name_t, name_rh, can_id)

        # -- SensorionSCD41 (sim → bytd) --------------------------------------
        if "SensorionSCD41" in node:
            scd = node["SensorionSCD41"]
            can_id = scd["addr"]
            name_t = scd["nameT"]
            name_rh = scd["nameRH"]
            name_co2 = scd["nameCO2"]
            tx.ensure(can_id, 6)  # T(2B) + RH(2B) + CO2(2B)
            binder.add_scd41(can_id, name_t, name_rh, name_co2)
            subscribe_topics.append(f"{MQTT_PREFIX_CTRL}{name_t}")
            subscribe_topics.append(f"{MQTT_PREFIX_CTRL}{name_rh}")
            subscribe_topics.append(f"{MQTT_PREFIX_CTRL}{name_co2}")
            logger.info("  SCD41  T=%-15s RH=%-15s CO2=%-15s CAN 0x%03X", name_t, name_rh, name_co2, can_id)

        # -- Pwm16At90 (bytd → sim) -------------------------------------------
        if "Pwm16At90" in node:
            pwm_node = node["Pwm16At90"]
            channels = pwm_node.get("channels", {})
            for ch_name, ch in channels.items():
                can_id = ch["addr"][0]
                offset = ch["addr"][1]
                rx.add_pwm16(can_id, offset, ch_name)
                logger.info("  Pwm16  %-25s  CAN 0x%03X [%d]", ch_name, can_id, offset)

    return subscribe_topics


# ---------------------------------------------------------------------------
# Async tasks
# ---------------------------------------------------------------------------

async def can_receive_loop(sock: socket.socket, rx: RxDispatcher) -> None:
    """Read CAN frames from the socket and dispatch to RxDispatcher."""
    while True:
        await anyio.wait_socket_readable(sock)
        try:
            raw = sock.recv(CAN_FRAME_SIZE)
        except OSError as exc:
            logger.error("CAN recv error: %s", exc)
            await anyio.sleep(0.1)
            continue
        if len(raw) < CAN_FRAME_SIZE:
            continue
        can_id, data, dlc = unpack_can_frame(raw)
        rx.dispatch(can_id, data, dlc)


async def can_transmit_loop(sock: socket.socket, tx: TxBuffer) -> None:
    """Periodically send all TX frames on the CAN bus."""
    while True:
        await anyio.sleep(TX_INTERVAL)
        for can_id, buf in tx.frames.items():
            dlc = tx.sizes.get(can_id, len(buf))
            frame = pack_can_frame(can_id, buf, dlc)
            try:
                sock.send(frame)
            except OSError as exc:
                logger.error("CAN send 0x%03X error: %s", can_id, exc)
                await anyio.sleep(0.1)


async def mqtt_receive_loop(
    msg_queue: queue.Queue[tuple[str, str]],
    binder: MqttInputBinder,
) -> None:
    """Bridge MQTT messages from the paho thread to the async world."""
    while True:
        try:
            topic, payload = await anyio.to_thread.run_sync(
                lambda: msg_queue.get(timeout=0.25),
                abandon_on_cancel=True,
            )
            binder.on_message(topic, payload)
        except queue.Empty:
            pass


async def signal_handler(scope: anyio.CancelScope) -> None:
    """Cancel the task group on SIGINT / SIGTERM."""
    with anyio.open_signal_receiver(signal.SIGINT, signal.SIGTERM) as signals:
        async for signum in signals:
            logger.info("Received signal %d, shutting down…", signum)
            scope.cancel()
            return


# ---------------------------------------------------------------------------
# MQTT client setup (paho, threaded loop — bridged via queue)
# ---------------------------------------------------------------------------

def make_mqtt_client(
    host: str,
    port: int,
    topics: list[str],
    msg_queue: queue.Queue[tuple[str, str]],
) -> paho_mqtt.Client:
    client = paho_mqtt.Client(paho_mqtt.CallbackAPIVersion.VERSION2, client_id="cansim")

    def on_connect(client: paho_mqtt.Client, _ud: Any, _flags: Any, rc: Any, _props: Any = None) -> None:
        logger.info("MQTT connected (rc=%s)", rc)
        for t in topics:
            client.subscribe(t, qos=0)
            logger.debug("MQTT subscribe %s", t)

    def on_message(_client: Any, _ud: Any, msg: paho_mqtt.MQTTMessage) -> None:
        try:
            msg_queue.put_nowait((msg.topic, msg.payload.decode()))
        except Exception:
            logger.exception("MQTT on_message error")

    client.on_connect = on_connect
    client.on_message = on_message
    client.connect_async(host, port)
    client.loop_start()
    return client


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

async def async_main(args: argparse.Namespace) -> None:
    # -- Load config ----------------------------------------------------------
    with open("config.yaml") as f:
        config = yaml.safe_load(f)

    # -- Build items ----------------------------------------------------------
    tx = TxBuffer()
    msg_queue: queue.Queue[tuple[str, str]] = queue.Queue()
    mqtt_client = make_mqtt_client(args.mqtt_host, args.mqtt_port, [], msg_queue)

    rx = RxDispatcher(mqtt_client)
    binder = MqttInputBinder(tx)

    topics = build_items(config, tx, rx, binder)

    # Re-subscribe now that we know the topics
    for t in topics:
        mqtt_client.subscribe(t, qos=0)
        logger.debug("MQTT subscribe %s", t)

    # -- CAN socket -----------------------------------------------------------
    can_sock = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    can_sock.setblocking(False)
    can_sock.bind((args.can_if,))
    logger.info("CAN socket bound to %s", args.can_if)

    logger.info(
        "Simulator running — TX frames: %d, MQTT subscriptions: %d",
        len(tx.frames),
        len(topics),
    )

    # -- Run ------------------------------------------------------------------
    async with anyio.create_task_group() as tg:
        tg.start_soon(signal_handler, tg.cancel_scope)
        tg.start_soon(can_receive_loop, can_sock, rx)
        tg.start_soon(can_transmit_loop, can_sock, tx)
        tg.start_soon(mqtt_receive_loop, msg_queue, binder)

    # -- Cleanup --------------------------------------------------------------
    mqtt_client.loop_stop()
    mqtt_client.disconnect()
    can_sock.close()
    logger.info("Simulator finished")


def main() -> None:
    parser = argparse.ArgumentParser(description="CAN node simulator")
    parser.add_argument("--can-if", default="vcan0", help="SocketCAN interface (default: vcan0)")
    parser.add_argument("--mqtt-host", default="localhost", help="MQTT broker host")
    parser.add_argument("--mqtt-port", type=int, default=1883, help="MQTT broker port")
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Enable debug logging"
    )
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(levelname)-5s %(name)s: %(message)s",
        datefmt="%H:%M:%S",
    )

    anyio.run(async_main, args, backend="trio")


if __name__ == "__main__":
    main()
