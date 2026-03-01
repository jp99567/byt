"""OpenThermBoiler — simulated OpenTherm gas boiler.

Contains the OtGasBoiler class.
"""

from __future__ import annotations

import enum
import logging
import struct
from typing import Callable
from . import OpenThermFrame

logger = logging.getLogger("cansim")

MQTT_PREFIX_STAT = "cansim/stat/"
MQTT_PREFIX_CTRL = "cansim/ctrl/"

PRU_RSP_OT_NO_RESPONSE = 14
PRU_RSP_OT_FRAME_ERROR = 15
PRU_RSP_OT_BUS_ERROR = 16
PRU_RSP_OT_OK = 17


class BoilerMode(enum.IntEnum):
    Off = 0
    Leto = 2
    Zima = 3

    
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
        self._mode: BoilerMode = BoilerMode.Off

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

    @property
    def mode(self) -> BoilerMode:
        return self._mode

    @mode.setter
    def mode(self, value: BoilerMode) -> None:
        if self._mode != value:
            self._mode = value
            logger.debug("OtBoiler: mode = %s", value.name)
            self._publish(f"{MQTT_PREFIX_STAT}{self._PREFIX}mode", value.name)

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

        req_raw = struct.unpack_from("<I", data, offset=4)[0]
        req = OpenThermFrame.Frame(req_raw)

        # --- validity & parity checks (mirrors OpenTherm::transmit) ---------
        if not req.is_valid():
            logger.warning("OtBoiler: invalid frame 0x%08X", req_raw)
            return struct.pack("<I", PRU_RSP_OT_FRAME_ERROR)

        if req_raw != OpenThermFrame.parity(req_raw):
            logger.warning("OtBoiler: parity error on frame 0x%08X", req_raw)
            return struct.pack("<I", PRU_RSP_OT_FRAME_ERROR)

        # --- build response -------------------------------------------------
        rsp = OpenThermFrame.Frame(0)
        rsp.set_id(req.get_id())

        msg_type = req.get_type()

        if msg_type == OpenThermFrame.MsgType.Mrd:
            if req.get_id() == 25:      # tCH (boiler flow temperature)
                rsp.set_type(OpenThermFrame.MsgType.Srdack)
                rsp.set_v(OpenThermFrame.float2f88(self.tCH))
            elif req.get_id() == 26:    # tDHW (DHW temperature)
                rsp.set_type(OpenThermFrame.MsgType.Srdack)
                rsp.set_v(OpenThermFrame.float2f88(self.tDHW))
            elif req.get_id() == 0:     # status
                rsp.set_type(OpenThermFrame.MsgType.Srdack)
                status = 0
                if self.flame:
                    status |= 1 << 0
                if self.ch_en:
                    status |= 1 << 1
                if self.dhw_en:
                    status |= 1 << 2
                rsp.set_v(status)
                self.mode = BoilerMode(req.get_v() >> 8)
            else:
                rsp.set_type(OpenThermFrame.MsgType.Sunknown)

        elif msg_type in (OpenThermFrame.MsgType.Mwr,
                          OpenThermFrame.MsgType.Mwr2):
            if req.get_id() == 1:       # CH setpoint
                self.spCH = OpenThermFrame.float_from_f88(req.get_v())
                rsp.set_type(OpenThermFrame.MsgType.Swrack)
                rsp.set_v(req.get_v())
            elif req.get_id() == 56:    # DHW setpoint
                self.spDHW = OpenThermFrame.float_from_f88(req.get_v())
                rsp.set_type(OpenThermFrame.MsgType.Swrack)
                rsp.set_v(req.get_v())
            else:
                rsp.set_type(OpenThermFrame.MsgType.Sunknown)

        else:
            rsp.set_type(OpenThermFrame.MsgType.Sunknown)

        rsp.data = OpenThermFrame.parity(rsp.data)
        logger.debug("OtBoiler: req %s, rsp %s mode=%s", OpenThermFrame.frame_to_str(req), OpenThermFrame.frame_to_str(rsp), self.mode.name)
        return struct.pack("<II", PRU_RSP_OT_OK, rsp.data)
