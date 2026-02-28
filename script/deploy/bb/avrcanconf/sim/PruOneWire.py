"""PruOneWire — simulated 1-Wire bus driven by PRU commands.

Contains PRU enum constants, OwTemperatureSensor, and OwBus.
"""

from __future__ import annotations

import logging
import struct

logger = logging.getLogger("cansim")

# ---------------------------------------------------------------------------
# PRU enum values (from pru/rpm_iface.h)
# ---------------------------------------------------------------------------

# ResponseCode
PRU_RSP_ERROR = 0
PRU_RSP_OW_PRESENCE_OK = 1
PRU_RSP_OW_BUS_FAILURE0 = 2
PRU_RSP_OW_BUS_FAILURE1 = 3
PRU_RSP_OW_NO_PRESENCE = 4
PRU_RSP_OW_BUS_FAILURE_TIMEOUT = 5
PRU_RSP_OW_READ_BITS_OK = 6
PRU_RSP_OW_READ_BITS_FAILURE = 7
PRU_RSP_OW_WRITE_BITS_OK = 8
PRU_RSP_OW_WRITE_BITS_FAILURE = 9
PRU_RSP_OW_SEARCH_RESULT_0 = 10
PRU_RSP_OW_SEARCH_RESULT_1 = 11
PRU_RSP_OW_SEARCH_RESULT_11 = 12
PRU_RSP_OW_SEARCH_RESULT_00 = 13
PRU_RSP_OT_NO_RESPONSE = 14
PRU_RSP_OT_FRAME_ERROR = 15
PRU_RSP_OT_BUS_ERROR = 16
PRU_RSP_OT_OK = 17

# Commands
PRU_CMD_HALT = 0
PRU_CMD_OW_INIT = 1
PRU_CMD_OW_SEARCH_DIR0 = 2
PRU_CMD_OW_SEARCH_DIR1 = 3
PRU_CMD_OW_WRITE = 4
PRU_CMD_OW_READ = 5
PRU_CMD_OW_WRITE_POWER = 6
PRU_CMD_OT_TRANSMIT = 7

_PRU_OW_CMDS = frozenset({
    PRU_CMD_OW_INIT,
    PRU_CMD_OW_SEARCH_DIR0,
    PRU_CMD_OW_SEARCH_DIR1,
    PRU_CMD_OW_WRITE,
    PRU_CMD_OW_READ,
    PRU_CMD_OW_WRITE_POWER,
})


# ---------------------------------------------------------------------------
# OwTemperatureSensor
# ---------------------------------------------------------------------------

class OwTemperatureSensor:
    """Simulated 1-Wire temperature sensor.

    Holds the 8-byte ROM code and a current temperature value.
    """

    def __init__(self, name: str, rom_code: bytes) -> None:
        if len(rom_code) != 8:
            raise ValueError(f"ROM code must be 8 bytes, got {len(rom_code)}")
        self.name = name
        self.rom_code = rom_code  # 8 bytes: family(1) + serial(6) + crc(1)
        self.temperature: float = float("nan")  # °C, set via MQTT later

    def reset(self) -> None:
        """Reset sensor to initial state (called on eCmdOwInit)."""
        pass

    def __repr__(self) -> str:
        return f"OwTemperatureSensor({self.name!r}, rc={self.rom_code.hex()})"


# ---------------------------------------------------------------------------
# OwBus
# ---------------------------------------------------------------------------

class OwBus:
    """Simulated 1-Wire bus.

    Loads sensor definitions from the BBOw section of *config* and
    handles PRU OW commands.
    """

    def __init__(self, config: dict) -> None:
        self.sensors: list[OwTemperatureSensor] = []
        bb_ow = config.get("BBOw", {})
        if bb_ow is None:
            bb_ow = {}
        for name, props in bb_ow.items():
            rc_hex = props["owRomCode"]
            rc_bytes = bytes.fromhex(rc_hex)
            sensor = OwTemperatureSensor(name, rc_bytes)
            self.sensors.append(sensor)
            logger.info("OwBus: loaded sensor %s  rc=%s", name, rc_hex)
        logger.info("OwBus: %d sensor(s) on bus", len(self.sensors))

        # Search state — reset on every eCmdOwInit
        self._search_participating: list[OwTemperatureSensor] = []
        self._search_bit_idx: int = 0

        # MQTT topic → sensor mapping
        self._topic_to_sensor: dict[str, OwTemperatureSensor] = {
            f"cansim/ctrl/{s.name}": s for s in self.sensors
        }

    @property
    def subscribe_topics(self) -> list[str]:
        """MQTT topics to subscribe to for temperature updates."""
        return list(self._topic_to_sensor)

    def on_mqtt_message(self, topic: str, payload: str) -> bool:
        """Handle an incoming MQTT message.

        If *topic* matches a sensor, updates its ``temperature`` and returns
        ``True``; otherwise returns ``False``.
        """
        sensor = self._topic_to_sensor.get(topic)
        if sensor is None:
            return False
        try:
            sensor.temperature = float(payload)
            logger.debug("MQTT→OwT %s = %.2f °C", sensor.name, sensor.temperature)
        except ValueError:
            logger.warning("OwBus: invalid temperature payload %r for %s", payload, sensor.name)
        return True

    def handle(self, cmd: int, data: bytes) -> bytes:
        """Process an OW command and return a response datagram.

        *cmd* is one of PRU_CMD_OW_* values.
        *data* is the full raw datagram received from bytd (including header).
        Returns bytes to send back.
        """
        if cmd == PRU_CMD_OW_INIT:
            return self._handle_init()
        if cmd == PRU_CMD_OW_SEARCH_DIR0:
            return self._handle_search(direction=0)
        if cmd == PRU_CMD_OW_SEARCH_DIR1:
            return self._handle_search(direction=1)

        logger.debug("OwBus: unhandled cmd=%d  len=%d  data=%s", cmd, len(data), data.hex())
        return struct.pack("<I", PRU_RSP_ERROR)

    def _handle_init(self) -> bytes:
        """Respond to eCmdOwInit (presence detect).

        Resets all sensors and search state.  Response is 8 bytes:
        [ResponseCode(i32), param(i32)] as bytd expects for presence replies.
        """
        for sensor in self.sensors:
            sensor.reset()
        self._search_participating = list(self.sensors)
        self._search_bit_idx = 0
        if self.sensors:
            logger.debug("OwBus: init → eOwPresenceOk (%d sensors)", len(self.sensors))
            return struct.pack("<Ii", PRU_RSP_OW_PRESENCE_OK, 0)
        else:
            logger.debug("OwBus: init → eOwNoPresence (no sensors)")
            return struct.pack("<Ii", PRU_RSP_OW_NO_PRESENCE, 0)

    def _handle_search(self, direction: int) -> bytes:
        """Respond to eCmdOwSearchDir0 / eCmdOwSearchDir1.

        Implements a single search-triplet step of the 1-Wire search algorithm.
        For the current bit position the wired-AND of all participating sensors
        is computed (actual bit and complement bit).  The combined two-bit value
        determines the response:

        - ``0b00``  both 0 and 1 present (conflict) → *direction* selects
        - ``0b01``  all sensors have 1
        - ``0b10``  all sensors have 0
        - ``0b11``  no sensor on bus

        Sensors whose bit does not match the selected direction are removed
        from the participating set.  The bit index is then advanced.
        """
        if not self._search_participating:
            logger.debug("OwBus: search bit %d → eOwSearchResult11 (no participants)",
                         self._search_bit_idx)
            return struct.pack("<I", PRU_RSP_OW_SEARCH_RESULT_11)

        byte_idx = self._search_bit_idx // 8
        bit_mask = 1 << (self._search_bit_idx % 8)

        has_one = False
        has_zero = False
        for s in self._search_participating:
            if s.rom_code[byte_idx] & bit_mask:
                has_one = True
            else:
                has_zero = True
            if has_one and has_zero:
                break  # conflict already determined

        if has_one and has_zero:
            # v=0b00: conflict — master chooses direction
            selected_bit = direction
            rsp_code = PRU_RSP_OW_SEARCH_RESULT_00
        elif has_one:
            # v=0b01: all participating sensors have 1
            selected_bit = 1
            rsp_code = PRU_RSP_OW_SEARCH_RESULT_0
        elif has_zero:
            # v=0b10: all participating sensors have 0
            selected_bit = 0
            rsp_code = PRU_RSP_OW_SEARCH_RESULT_1
        else:
            # should not happen
            return struct.pack("<I", PRU_RSP_OW_SEARCH_RESULT_11)

        # Eliminate sensors whose bit doesn't match the selected direction
        self._search_participating = [
            s for s in self._search_participating
            if bool(s.rom_code[byte_idx] & bit_mask) == bool(selected_bit)
        ]

        logger.debug(
            "OwBus: search bit %d  dir=%d  sel=%d  rsp=%d  remaining=%d",
            self._search_bit_idx, direction, selected_bit, rsp_code,
            len(self._search_participating),
        )

        self._search_bit_idx += 1
        return struct.pack("<I", rsp_code)
