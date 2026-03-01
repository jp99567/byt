"""PruOneWire — simulated 1-Wire bus driven by PRU commands.

Contains PRU enum constants, OwTemperatureSensor, and OwBus.
"""

from __future__ import annotations

import datetime
import enum
import logging
import struct
import bitarray

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

def dallas_crc8(data):
    crc = 0
    for c in data:
        for i in range(0, 8):
            b = (crc & 1) ^ ((int(c) & (1 << i)) >> i)
            crc = (crc ^ (b * 0x118)) >> 1
    return crc


# ---------------------------------------------------------------------------
# OwTemperatureSensor
# ---------------------------------------------------------------------------

class OwSensorState(enum.Enum):
    IDLE = 0
    IGNORING = 1
    SELECTED = 2
    MATCH_ROM = 3
    SEARCH = 4
    CONVERTING = 5


class OwThermCmd(enum.IntEnum):
    """1-Wire thermometer commands (ow::OwThermNet::Cmd from OwThermNet.h)."""
    READ_ROM        = 0x33
    CONVERT         = 0x44
    MATCH_ROM       = 0x55
    READ_SCRATCHPAD = 0xBE
    SKIP_ROM        = 0xCC
    SEARCH          = 0xF0


class OwTemperatureSensor:
    """Simulated 1-Wire temperature sensor.

    Holds the 8-byte ROM code and a current temperature value.
    """

    def __init__(self, name: str, rom_code: bytes) -> None:
        if len(rom_code) != 8:
            raise ValueError(f"ROM code must be 8 bytes, got {len(rom_code)}")
        self.name = name
        self.rom_code = rom_code  # 8 bytes: family(1) + serial(6) + crc(1)
        self.temperature: float = -12.3456  # °C, set via MQTT later
        self.measured_temp = 85 << 4  # DS18B20 power-on reset value, 4 fractional bits (1 LSB = 1/16 °C)
        self.state = OwSensorState.IDLE
        self.bits_in = bitarray.bitarray(endian="little")  # bits received from master, for current command
        self.bits_out = bitarray.bitarray(endian="little")  # bits to send to master, for current command
        self.search_bit_idx = 0  # for SEARCH command

    def reset(self) -> None:
        """Reset sensor to initial state (called on eCmdOwInit)."""
        self.bits_in.clear()
        self.bits_out.clear()
        self.search_bit_idx = 0
        if self.state == OwSensorState.CONVERTING:
            if datetime.datetime.now() - self.convert_start_time >= datetime.timedelta(seconds=0.75):
                if not self.temperature == self.temperature:  # NaN check
                    logger.warning("OwSensor %s: CONVERT completed but temperature is NaN", self.name)
                else:
                    self.measured_temp = int(self.temperature * (1<<4))  # convert °C to raw value with 4 fractional bits
        self.state = OwSensorState.IDLE

    def __repr__(self) -> str:
        return f"OwTemperatureSensor({self.name!r}, rc={self.rom_code.hex()})"
    
    def read(self, num_bits: int) -> bitarray.bitarray:
        """Read bits to send to master (called on eCmdOwRead).

        Returns up to *num_bits* bits from the current response buffer.
        """

        if self.state == OwSensorState.IGNORING:
            self.bits_out.clear()  # discard any pending response bits
        
        if num_bits > len(self.bits_out):
            padding = bitarray.bitarray(num_bits - len(self.bits_out), endian="little")
            padding.setall(1)
            self.bits_out.extend(padding)
        bits = self.bits_out[:num_bits]
        self.bits_out = self.bits_out[num_bits:]
        return bits
        
    
    def write(self, bits: bitarray.bitarray, power: bool) -> None:
        """Handle bits written to the bus (called on eCmdOwWrite / eCmdOwWritePower).

        *bits* are the bits written by the master, in little-endian order.
        *power* is ``True`` if the bus is powered during the write (for
        parasitically powered sensors).
        """

        self.bits_in.extend(bits)
        while self.bits_in:
            wait_for_more = self.process(power)
            if wait_for_more:
                break

    def process(self, power: bool) -> bool:
        wait_for_more_bits = False  # if True, master should wait for more bits before next process() call
        if self.state == OwSensorState.IDLE:
            if len(self.bits_in) >= 8:
                cmd = int.from_bytes(self.bits_in[0:8].tobytes(), "little")
                self.bits_in = self.bits_in[8:]
                if cmd == OwThermCmd.SKIP_ROM:
                    self.state = OwSensorState.SELECTED
                elif cmd == OwThermCmd.MATCH_ROM:
                    self.state = OwSensorState.MATCH_ROM
                elif cmd == OwThermCmd.SEARCH:
                    self.state = OwSensorState.SEARCH
                    if len(self.bits_in) > 0:
                        logger.warning("OwSensor %s: received extra bits after SEARCH command — ignoring", self.name)
                        self.state = OwSensorState.IGNORING
                    else:
                        # Prepare for SEARCH command by setting search_bit_idx to 0
                        self.search_bit_idx = 0
                        rom_code_bits = bitarray.bitarray(endian="little")
                        rom_code_bits.frombytes(self.rom_code)
                        self.bits_out = bitarray.bitarray((rom_code_bits[self.search_bit_idx],
                                                           (~rom_code_bits)[self.search_bit_idx]),
                                                           endian="little")
                else:
                    self.state = OwSensorState.IGNORING
            else:
                wait_for_more_bits = True
        elif self.state == OwSensorState.MATCH_ROM:
            if len(self.bits_in) >= 64:
                rom_code = self.bits_in[0:64].tobytes()
                self.bits_in = self.bits_in[64:]
                if rom_code == self.rom_code:
                    self.state = OwSensorState.SELECTED
                else:
                    self.state = OwSensorState.IGNORING
            else:
                wait_for_more_bits = True   
        elif self.state == OwSensorState.SELECTED:
            if len(self.bits_in) >= 8:
                cmd = int.from_bytes(self.bits_in[0:8].tobytes(), "little")
                self.bits_in = self.bits_in[8:]
                if cmd == OwThermCmd.CONVERT:
                    self.measured_temp = 85 << 4  # DS18B20 default value during CONVERT
                    self.convert_start_time = datetime.datetime.now()
                    if not power:
                        logger.warning("OwSensor %s: CONVERT command received without power — ignoring", self.name)
                        self.state = OwSensorState.IGNORING
                    elif len(self.bits_in) > 0:
                        logger.warning("OwSensor %s: received extra bits after CONVERT command — ignoring", self.name)
                        self.state = OwSensorState.IGNORING
                    else:
                        self.state = OwSensorState.CONVERTING
                elif cmd == OwThermCmd.READ_SCRATCHPAD:
                    if len(self.bits_in) > 0:
                        logger.warning("OwSensor %s: received extra bits after READ_SCRATCHPAD command — ignoring", self.name)
                        self.state = OwSensorState.IGNORING
                    else:
                        # ThermScratchpad: int16_t temp | int8_t alarmH | int8_t alarmL | uint8_t conf | char[3] reserved | uint8_t crc
                        scratchpad = bytearray(struct.pack("<hbbB3sB",
                            self.measured_temp,  # temperature (raw, LSB = 1/16 °C)
                            0,                   # alarmH
                            0,                   # alarmL
                            0x7F,                # conf (12-bit resolution)
                            b'\xFF\xFF\x10',     # reserved
                            0,                   # crc (not verified by bytd sim)
                        ))
                        scratchpad[-1] = dallas_crc8(scratchpad[:-1])
                        ba = bitarray.bitarray(endian="little")
                        ba.frombytes(bytes(scratchpad))
                        self.bits_out.extend(ba)
                else:
                    logger.warning("OwSensor %s: unrecognized command 0x%02X — ignoring", self.name, cmd)
                    self.state = OwSensorState.IGNORING
        elif self.state == OwSensorState.IGNORING:
            self.bits_in.clear()  # discard bits until next command
        elif self.state == OwSensorState.SEARCH:
            if len(self.bits_in) == 1:
                rom_code_bits = bitarray.bitarray(endian="little")
                rom_code_bits.frombytes(self.rom_code)
                if rom_code_bits[self.search_bit_idx] == self.bits_in[0]:
                        rom_code_bits = bitarray.bitarray(endian="little")
                        rom_code_bits.frombytes(self.rom_code)
                        self.bits_out = bitarray.bitarray((rom_code_bits[self.search_bit_idx],
                                                           (~rom_code_bits)[self.search_bit_idx]),
                                                           endian="little")
                        self.search_bit_idx += 1
                        if self.search_bit_idx >= 64:
                            self.state = OwSensorState.IGNORING
                            logger.warning("OwSensor %s: completed SEARCH response but master sent extra bits — ignoring", self.name)
                else:
                    self.state = OwSensorState.IGNORING
            else:
                logger.warning("OwSensor %s: expected 1 bit for SEARCH response, got %d — ignoring", self.name, len(self.bits_in))
                self.state = OwSensorState.IGNORING
            self.bits_in.clear()
        elif self.state == OwSensorState.CONVERTING:
            if datetime.datetime.now() - self.convert_start_time < datetime.timedelta(seconds=0.75):
                logger.warning("OwSensor %s: received write during CONVERT — ignoring", self.name)
                self.state = OwSensorState.IGNORING
            self.bits_in.clear()
        return wait_for_more_bits
        
            

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
        if cmd in (PRU_CMD_OW_WRITE, PRU_CMD_OW_WRITE_POWER):
            num_bits = struct.unpack_from("<I", data, 4)[0]
            self._handle_write(num_bits, data[8:], power=(cmd == PRU_CMD_OW_WRITE_POWER))
            return struct.pack("<I", PRU_RSP_OW_WRITE_BITS_OK)
        if cmd == PRU_CMD_OW_READ:
            num_bits = struct.unpack_from("<I", data, 4)[0]
            return struct.pack("<I", PRU_RSP_OW_READ_BITS_OK) + self._handle_read(num_bits).tobytes()
        if cmd == PRU_CMD_OW_SEARCH_DIR0:
            return self._handle_search(direction=0)
        if cmd == PRU_CMD_OW_SEARCH_DIR1:
            return self._handle_search(direction=1)

        logger.debug("OwBus: unhandled cmd=%d  len=%d  data=%s", cmd, len(data), data.hex())
        return struct.pack("<I", PRU_RSP_ERROR)

    def _handle_read(self, num_bits: int) -> bitarray.bitarray:
        """Handle eCmdOwRead.

        Payload is 4 bytes: number of bits to read (u32).  Response is the
        bits read from the bus, in little-endian order.
        """

        logger.debug(f"OwBus: read {num_bits=} bits")
        bits = bitarray.bitarray(num_bits, endian="little")
        bits.setall(1)  # default to 1 (bus idle) if no sensor drives it low
        for sensor in self.sensors:
            bits &= sensor.read(num_bits)
        return bits
    
    def _handle_write(self, bitsize: int, payload: bytes, power: bool) -> None:
        """Handle eCmdOwWrite / eCmdOwWritePower.

        Payload is the bytes to write to the bus.  If *power* is ``True``, the
        bus is also powered during the write (for parasitically powered sensors).
        """

        bits = bitarray.bitarray(endian="little")
        bits.frombytes(payload)
        bits = bits[:bitsize]
        logger.debug(f"OwBus: write {bitsize=} {power=}  {bits=}")
        for sensor in self.sensors:
            sensor.write(bits, power)
        
    def _handle_init(self) -> bytes:
        """Respond to eCmdOwInit (presence detect).

        Resets all sensors and search state.  Response is 8 bytes:
        [ResponseCode(i32), param(i32)] as bytd expects for presence replies.
        """
        for sensor in self.sensors:
            sensor.reset()

        if self.sensors:
            logger.debug("OwBus: init → eOwPresenceOk (%d sensors)", len(self.sensors))
            return struct.pack("<Ii", PRU_RSP_OW_PRESENCE_OK, 0)
        else:
            logger.debug("OwBus: init → eOwNoPresence (no sensors)")
            return struct.pack("<Ii", PRU_RSP_OW_NO_PRESENCE, 0)

    def _handle_search(self, direction: int) -> bytes:
        """Respond to eCmdOwSearchDir0 / eCmdOwSearchDir1.

        - ``0b00``  both 0 and 1 present (conflict) → *direction* selects
        - ``0b01``  all sensors have 1
        - ``0b10``  all sensors have 0
        - ``0b11``  no sensor on bus
        """

        code_bits = self._handle_read(2)

        if code_bits == bitarray.bitarray("00", endian="little"):
            # v=0b00: conflict — master chooses direction
            rsp_code = PRU_RSP_OW_SEARCH_RESULT_00
        elif code_bits == bitarray.bitarray("01", endian="little"):
            # v=0b01: all participating sensors have 1
            rsp_code = PRU_RSP_OW_SEARCH_RESULT_0
        elif code_bits == bitarray.bitarray("10", endian="little"):
            # v=0b10: all participating sensors have 0
            rsp_code = PRU_RSP_OW_SEARCH_RESULT_1
        else:
            # should not happen
            return struct.pack("<I", PRU_RSP_OW_SEARCH_RESULT_11)

        dirbit = bitarray.bitarray("1" if direction else "0", endian="little")
        for sensor in self.sensors:
            sensor.write(dirbit, power=False)

        return struct.pack("<I", rsp_code)
