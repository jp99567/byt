#!/usr/bin/env python3
"""CAN Node Simulator

Simulates CAN nodes described in config.yaml for communication with bytd.
Uses anyio with asyncio backend, SocketCAN (raw Linux socket), and MQTT (aiomqtt).

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
import signal
import socket
import struct
import sys
from dataclasses import dataclass, field
from typing import Callable

import aiomqtt
import anyio
import yaml

from sim.PruOneWire import (
    PRU_RSP_ERROR,
    PRU_CMD_HALT,
    PRU_CMD_OT_TRANSMIT,
    PRU_RSP_OT_NO_RESPONSE,
    PRU_RSP_OT_OK,
    _PRU_OW_CMDS,
    OwBus,
)

logger = logging.getLogger("cansim")

PRU_SIM_SOCKET_PATH = "/tmp/pru_sim_socket"

# ---------------------------------------------------------------------------
# SocketCAN helpers
# ---------------------------------------------------------------------------
# struct can_frame { canid_t can_id; __u8 can_dlc; __u8 __pad, __res0, __res1; __u8 data[8]; };
CAN_FRAME_FMT = "=IB3x8s"
CAN_FRAME_SIZE = struct.calcsize(CAN_FRAME_FMT)

MQTT_PREFIX_STAT = "cansim/stat/"
MQTT_PREFIX_CTRL = "cansim/ctrl/"


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

    def __init__(self) -> None:
        self._decoders: dict[int, list[Callable[[bytes, int], tuple[str, str] | None]]] = {}

    def add_digi_out(self, can_id: int, offset: int, bit: int, name: str) -> None:
        mask = 1 << bit

        def decode(data: bytes, dlc: int) -> tuple[str, str] | None:
            if offset < dlc:
                val = 1 if (data[offset] & mask) else 0
                logger.debug("RX DigOUT %s = %d", name, val)
                return f"{MQTT_PREFIX_STAT}{name}", str(val)
            return None

        self._decoders.setdefault(can_id, []).append(decode)

    def add_pwm16(self, can_id: int, offset: int, name: str) -> None:
        def decode(data: bytes, dlc: int) -> tuple[str, str] | None:
            if offset + 2 <= dlc:
                val = struct.unpack_from("<H", data, offset)[0]
                logger.debug("RX Pwm16 %s = %d", name, val)
                return f"{MQTT_PREFIX_STAT}{name}", str(val)
            return None

        self._decoders.setdefault(can_id, []).append(decode)

    async def dispatch(self, can_id: int, data: bytes, dlc: int, mqtt: aiomqtt.Client) -> None:
        decoders = self._decoders.get(can_id)
        if decoders:
            for dec in decoders:
                result = dec(data, dlc)
                if result is not None:
                    topic, payload = result
                    await mqtt.publish(topic, payload, retain=True)


# ---------------------------------------------------------------------------
# MQTT → TX buffer binding (input items: DigIN, OwT, Sensorion)
# ---------------------------------------------------------------------------

class MqttInputBinder:
    """Binds MQTT control topics to TX buffer updates for items sent to bytd."""

    def __init__(self, tx: TxBuffer) -> None:
        self.tx = tx
        self.handlers: dict[str, Callable[[str], int | None]] = {}

    # -- DigIN ---------------------------------------------------------------
    def add_digi_in(self, can_id: int, offset: int, bit: int, name: str) -> None:
        topic = f"{MQTT_PREFIX_CTRL}{name}"

        def handle(payload: str) -> int:
            val = bool(int(float(payload)))
            self.tx.set_bit(can_id, offset, bit, val)
            logger.debug("MQTT→TX DigIN %s = %s", name, val)
            return can_id

        self.handlers[topic] = handle

    # -- OwT -----------------------------------------------------------------
    def add_owt(self, can_id: int, offset: int, name: str, factor: float) -> None:
        topic = f"{MQTT_PREFIX_CTRL}{name}"

        def handle(payload: str) -> int | None:
            temp = float(payload)
            if math.isnan(temp):
                return None
            raw = owt_to_raw(temp, factor)
            self.tx.set_int16(can_id, offset, raw)
            logger.debug("MQTT→TX OwT %s = %.2f (raw %d)", name, temp, raw)
            return can_id

        self.handlers[topic] = handle

    # -- SensorionSHT11 ------------------------------------------------------
    def add_sht11(self, can_id: int, name_t: str, name_rh: str) -> None:
        topic_t = f"{MQTT_PREFIX_CTRL}{name_t}"
        topic_rh = f"{MQTT_PREFIX_CTRL}{name_rh}"

        def handle_t(payload: str) -> int:
            temp = float(payload)
            raw = sht11_t_to_raw(temp)
            self.tx.set_uint16(can_id, 0, raw)
            logger.debug("MQTT→TX SHT11 T %s = %.2f (raw %d)", name_t, temp, raw)
            return can_id

        def handle_rh(payload: str) -> int:
            rh = float(payload)
            raw = sht11_rh_to_raw(rh)
            self.tx.set_uint16(can_id, 2, raw)
            logger.debug("MQTT→TX SHT11 RH %s = %.1f%% (raw %d)", name_rh, rh, raw)
            return can_id

        self.handlers[topic_t] = handle_t
        self.handlers[topic_rh] = handle_rh

    # -- SensorionSCD41 ------------------------------------------------------
    def add_scd41(self, can_id: int, name_t: str, name_rh: str, name_co2: str) -> None:
        topic_t = f"{MQTT_PREFIX_CTRL}{name_t}"
        topic_rh = f"{MQTT_PREFIX_CTRL}{name_rh}"
        topic_co2 = f"{MQTT_PREFIX_CTRL}{name_co2}"

        def handle_t(payload: str) -> int:
            temp = float(payload)
            raw = scd41_t_to_raw(temp)
            self.tx.set_uint16(can_id, 0, raw)
            logger.debug("MQTT→TX SCD41 T %s = %.2f (raw %d)", name_t, temp, raw)
            return can_id

        def handle_rh(payload: str) -> int:
            rh = float(payload)
            raw = scd41_rh_to_raw(rh)
            self.tx.set_uint16(can_id, 2, raw)
            logger.debug("MQTT→TX SCD41 RH %s = %.1f%% (raw %d)", name_rh, rh, raw)
            return can_id

        def handle_co2(payload: str) -> int:
            co2 = float(payload)
            raw = scd41_co2_to_raw(co2)
            self.tx.set_uint16(can_id, 4, raw)
            logger.debug("MQTT→TX SCD41 CO2 %s = %.0f ppm (raw %d)", name_co2, co2, raw)
            return can_id

        self.handlers[topic_t] = handle_t
        self.handlers[topic_rh] = handle_rh
        self.handlers[topic_co2] = handle_co2

    # -- dispatch incoming MQTT message --------------------------------------
    def on_message(self, topic: str, payload: str) -> int | None:
        """Process MQTT message and return affected CAN ID, or None."""
        handler = self.handlers.get(topic)
        if handler is not None:
            try:
                return handler(payload)
            except Exception:
                logger.exception("Error handling MQTT message on %s", topic)
        return None


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

async def can_receive_loop(sock: socket.socket, rx: RxDispatcher, mqtt: aiomqtt.Client) -> None:
    """Read CAN frames from the socket and dispatch to RxDispatcher."""
    while True:
        await anyio.wait_readable(sock)
        try:
            raw = sock.recv(CAN_FRAME_SIZE)
        except OSError as exc:
            logger.error("CAN recv error: %s", exc)
            await anyio.sleep(0.1)
            continue
        if len(raw) < CAN_FRAME_SIZE:
            continue

        can_id, data, dlc = unpack_can_frame(raw)
        await rx.dispatch(can_id, data, dlc, mqtt)


async def mqtt_message_loop(
    mqtt: aiomqtt.Client,
    binder: MqttInputBinder,
    can_sock: socket.socket,
    tx: TxBuffer,
    ow_bus: OwBus,
    ot_boiler: OtGasBoiler,
) -> None:
    """Receive MQTT messages, update TX buffer, and send affected CAN frames."""
    async for message in mqtt.messages:
        topic = message.topic.value
        payload = message.payload.decode() if isinstance(message.payload, bytes) else str(message.payload)
        ow_bus.on_mqtt_message(topic, payload)
        ot_boiler.on_mqtt_message(topic, payload)
        can_id = binder.on_message(topic, payload)
        if can_id is not None:
            buf = tx.frames.get(can_id)
            if buf is not None:
                dlc = tx.sizes.get(can_id, len(buf))
                frame = pack_can_frame(can_id, buf, dlc)
                try:
                    can_sock.send(frame)
                except OSError as exc:
                    logger.error("CAN send 0x%03X error: %s", can_id, exc)


async def signal_handler(scope: anyio.CancelScope) -> None:
    """Cancel the task group on SIGINT / SIGTERM."""
    with anyio.open_signal_receiver(signal.SIGINT, signal.SIGTERM) as signals:
        async for signum in signals:
            logger.info("Received signal %d, shutting down…", signum)
            scope.cancel()
            return


# ---------------------------------------------------------------------------
# PRU peripherals — OtGasBoiler
# ---------------------------------------------------------------------------

class OtGasBoiler:
    """Simulated OpenTherm gas boiler.

    **Output values** (controlled by the simulated environment via MQTT):
        flame (bool), ch_en (bool), dhw_en (bool), tDHW (float), tCH (float)
        Subscribed on ``cansim/ctrl/boiler_<name>``.

    **Input values** (decoded from OT frames sent by bytd, published via callback):
        spCH (float), spDHW (float)
        Published to ``cansim/stat/boiler_<name>`` when they change.
    """

    _PREFIX = "boiler_"

    def __init__(self) -> None:
        # Output values (environment → boiler, via MQTT ctrl topics)
        self.flame: bool = False
        self.ch_en: bool = False
        self.dhw_en: bool = False
        self.tDHW: float = 35.0
        self.tCH: float = 40.0

        # Input values (bytd → boiler → MQTT stat topics)
        self._spCH: float = 0.0
        self._spDHW: float = 0.0

        # Pending MQTT publications: list of (topic, payload) pairs
        # Drained by the async caller after each handle() invocation.
        self.pending_publishes: list[tuple[str, str]] = []

        # MQTT topic → handler mapping (built once)
        self._ctrl_handlers: dict[str, Callable[[str], None]] = {
            f"{MQTT_PREFIX_CTRL}{self._PREFIX}flame":  self._set_flame,
            f"{MQTT_PREFIX_CTRL}{self._PREFIX}ch_en":  self._set_ch_en,
            f"{MQTT_PREFIX_CTRL}{self._PREFIX}dhw_en": self._set_dhw_en,
            f"{MQTT_PREFIX_CTRL}{self._PREFIX}tDHW":   self._set_tDHW,
            f"{MQTT_PREFIX_CTRL}{self._PREFIX}tCH":    self._set_tCH,
        }

    # -- Output setters (from MQTT ctrl) -------------------------------------

    def _set_flame(self, payload: str) -> None:
        self.flame = bool(int(float(payload)))
        logger.debug("OtBoiler: flame = %s", self.flame)

    def _set_ch_en(self, payload: str) -> None:
        self.ch_en = bool(int(float(payload)))
        logger.debug("OtBoiler: ch_en = %s", self.ch_en)

    def _set_dhw_en(self, payload: str) -> None:
        self.dhw_en = bool(int(float(payload)))
        logger.debug("OtBoiler: dhw_en = %s", self.dhw_en)

    def _set_tDHW(self, payload: str) -> None:
        self.tDHW = float(payload)
        logger.debug("OtBoiler: tDHW = %.1f", self.tDHW)

    def _set_tCH(self, payload: str) -> None:
        self.tCH = float(payload)
        logger.debug("OtBoiler: tCH = %.1f", self.tCH)

    # -- Input properties (decoded from OT frames, published to MQTT stat) ---

    @property
    def spCH(self) -> float:
        return self._spCH

    @spCH.setter
    def spCH(self, value: float) -> None:
        if self._spCH != value:
            self._spCH = value
            logger.debug("OtBoiler: spCH = %.1f", value)
            self._publish(f"{MQTT_PREFIX_STAT}{self._PREFIX}spCH", f"{value:.1f}")

    @property
    def spDHW(self) -> float:
        return self._spDHW

    @spDHW.setter
    def spDHW(self, value: float) -> None:
        if self._spDHW != value:
            self._spDHW = value
            logger.debug("OtBoiler: spDHW = %.1f", value)
            self._publish(f"{MQTT_PREFIX_STAT}{self._PREFIX}spDHW", f"{value:.1f}")

    def _publish(self, topic: str, payload: str) -> None:
        """Queue a (topic, payload) pair for async publishing."""
        self.pending_publishes.append((topic, payload))

    # -- MQTT interface (same pattern as OwBus) ------------------------------

    @property
    def subscribe_topics(self) -> list[str]:
        """MQTT topics to subscribe to for output value control."""
        return list(self._ctrl_handlers)

    def on_mqtt_message(self, topic: str, payload: str) -> bool:
        """Handle an incoming MQTT control message.

        Returns ``True`` if the topic was handled.
        """
        handler = self._ctrl_handlers.get(topic)
        if handler is None:
            return False
        try:
            handler(payload)
        except Exception:
            logger.exception("OtBoiler: error handling MQTT %s", topic)
        return True

    # -- PRU handle (OT frame processing — to be implemented) ----------------

    def handle(self, data: bytes) -> bytes:
        """Process an OT transmit command and return a response datagram.

        *data* is the full raw datagram received from bytd (including the
        eCmdOtTransmit header and the 32-bit OT frame).  Frame decoding and
        input value extraction will be implemented in a later step.
        Returns bytes to send back.
        """
        logger.debug("OtGasBoiler: len=%d  data=%s", len(data), data.hex())
        return struct.pack("<I", PRU_RSP_OT_NO_RESPONSE)


# ---------------------------------------------------------------------------
# PRU simulator — Unix DGRAM socket server with command dispatching
# ---------------------------------------------------------------------------

async def pru_sim_server(
    scope: anyio.CancelScope,
    ow_bus: OwBus,
    ot_boiler: OtGasBoiler,
    mqtt: aiomqtt.Client,
) -> None:
    """Serve the PRU simulator Unix DGRAM socket.

    Binds to PRU_SIM_SOCKET_PATH and dispatches incoming datagrams based on
    the 32-bit command header (pru::Commands enum from rpm_iface.h):
      - eCmdHalt        → ignored (no response)
      - eCmdOw*         → forwarded to *ow_bus*
      - eCmdOtTransmit  → forwarded to *ot_boiler*
    """
    async with await anyio.create_unix_datagram_socket(
        local_path=PRU_SIM_SOCKET_PATH,
    ) as sock:
        logger.info("PRU sim socket listening on %s", PRU_SIM_SOCKET_PATH)

        while True:
            data, client_path = await sock.receive()
            if len(data) < 4:
                logger.warning("PRU sim rx too short (%d bytes)", len(data))
                continue

            cmd = struct.unpack_from("<I", data, 0)[0]
            logger.debug("PRU sim rx cmd=%d  len=%d  data=%s", cmd, len(data), data.hex())

            if cmd == PRU_CMD_HALT:
                logger.info("PRU sim: eCmdHalt — ignored")
                continue

            if cmd in _PRU_OW_CMDS:
                response = ow_bus.handle(cmd, data)
            elif cmd == PRU_CMD_OT_TRANSMIT:
                response = ot_boiler.handle(data)
            else:
                logger.warning("PRU sim: unknown command %d", cmd)
                response = struct.pack("<I", PRU_RSP_ERROR)

            await sock.send((response, client_path))

            # Drain any MQTT publications queued by ot_boiler.handle()
            for pub_topic, pub_payload in ot_boiler.pending_publishes:
                await mqtt.publish(pub_topic, pub_payload, retain=True)
            ot_boiler.pending_publishes.clear()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

async def async_main(args: argparse.Namespace) -> None:
    # -- Load config ----------------------------------------------------------
    with open("config.yaml") as f:
        config = yaml.safe_load(f)

    # -- Build items ----------------------------------------------------------
    tx = TxBuffer()
    rx = RxDispatcher()
    binder = MqttInputBinder(tx)

    topics = build_items(config, tx, rx, binder)

    # -- PRU peripherals -------------------------------------------------------
    ow_bus = OwBus(config)
    ot_boiler = OtGasBoiler()

    # -- CAN socket -----------------------------------------------------------
    can_sock = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    can_sock.setblocking(False)
    can_sock.bind((args.can_if,))
    logger.info("CAN socket bound to %s", args.can_if)

    # -- MQTT + Run -----------------------------------------------------------
    all_topics = topics + ow_bus.subscribe_topics + ot_boiler.subscribe_topics

    async with aiomqtt.Client(
        hostname=args.mqtt_host,
        port=args.mqtt_port,
        identifier="cansim",
    ) as mqtt:
        for t in all_topics:
            await mqtt.subscribe(t, qos=0)
            logger.debug("MQTT subscribe %s", t)

        logger.info(
            "Simulator running — TX frames: %d, MQTT subscriptions: %d",
            len(tx.frames),
            len(all_topics),
        )

        async with anyio.create_task_group() as tg:
            tg.start_soon(signal_handler, tg.cancel_scope)
            tg.start_soon(pru_sim_server, tg.cancel_scope, ow_bus, ot_boiler, mqtt)
            tg.start_soon(can_receive_loop, can_sock, rx, mqtt)
            tg.start_soon(mqtt_message_loop, mqtt, binder, can_sock, tx, ow_bus, ot_boiler)

    # -- Cleanup --------------------------------------------------------------
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

    anyio.run(async_main, args, backend="asyncio")


if __name__ == "__main__":
    main()
