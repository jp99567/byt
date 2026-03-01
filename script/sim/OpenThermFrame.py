"""OpenTherm frame handling — Python reimplementation of OtFrame.h/cpp
and anonymous-namespace helpers from OpenTherm.cpp.
"""

from __future__ import annotations

import enum
import math


# ---------------------------------------------------------------------------
# opentherm::msg::type  (OtFrame.h)
# ---------------------------------------------------------------------------

class MsgType(enum.IntEnum):
    """OpenTherm message types (3-bit field stored in bits [26:24])."""
    Mrd       = 0b000_0000
    Mwr       = 0b001_0000
    Minvalid  = 0b010_0000
    Mreserved = 0b011_0000
    Srdack    = 0b100_0000
    Swrack    = 0b101_0000
    Sinvalid  = 0b110_0000
    Sunknown  = 0b111_0000
    Mwr2      = 0b001_1000


_MSG_NAMES: dict[MsgType, str] = {
    MsgType.Mrd:       "Mrd",
    MsgType.Mwr:       "Mwr",
    MsgType.Minvalid:  "Minvalid",
    MsgType.Mreserved: "Mreserved",
    MsgType.Srdack:    "Srdack",
    MsgType.Swrack:    "Swrack",
    MsgType.Sinvalid:  "Sinvalid",
    MsgType.Sunknown:  "Sunknown",
    MsgType.Mwr2:      "Mwr2",
}

_MASTER_TYPES = frozenset((
    MsgType.Mrd,
    MsgType.Mwr,
    MsgType.Minvalid,
    MsgType.Mreserved,
    MsgType.Mwr2,
))


def msg_to_str(m: MsgType) -> str:
    """Return a short human-readable label for a message type."""
    return _MSG_NAMES.get(m, "---")


def is_master(t: MsgType) -> bool:
    """Return *True* if *t* is a master-originated message type."""
    return t in _MASTER_TYPES


# ---------------------------------------------------------------------------
# opentherm::Frame  (OtFrame.h / OtFrame.cpp)
# ---------------------------------------------------------------------------

class Frame:
    """A 32-bit OpenTherm frame.

    Layout (MSB → LSB):
        [31]    parity
        [30:28] spare (always 0 in a valid frame)
        [27:24] message type  (upper nibble of byte 3)
        [23:16] data-id
        [15:0]  data-value (u16 or f8.8)
    """

    INVALID: int = 0x7FFF7BAD

    __slots__ = ("data",)

    def __init__(self, data_or_type: int | MsgType = INVALID,
                 id_: int | None = None,
                 val: int = 0) -> None:
        if id_ is not None:
            # Three-argument form: Frame(type, id, val)
            self.data: int = 0
            self.set_type(MsgType(data_or_type))
            self.set_id(id_)
            self.set_v(val)
        else:
            # Single-argument form: Frame(raw_u32)  or  Frame()
            self.data = data_or_type & 0xFFFFFFFF

    # -- type (bits 30:24, masked to 7 bits) --------------------------------

    def set_type(self, t: MsgType) -> None:
        self.data = (self.data & ~(0xFF << 24)) | ((int(t) & 0xFF) << 24)

    def get_type(self) -> MsgType:
        return MsgType((self.data >> 24) & 0x7F)

    # -- id (bits 23:16) ----------------------------------------------------

    def set_id(self, id_: int) -> None:
        self.data = (self.data & ~(0xFF << 16)) | ((id_ & 0xFF) << 16)

    def get_id(self) -> int:
        return (self.data >> 16) & 0xFF

    # -- value (bits 15:0) --------------------------------------------------

    def set_v(self, v: int) -> None:
        self.data = (self.data & 0xFFFF_0000) | (v & 0xFFFF)

    def get_v(self) -> int:
        return self.data & 0xFFFF

    # -- validity -----------------------------------------------------------

    def is_valid(self) -> bool:
        return self.data != self.INVALID

    # -- convenience --------------------------------------------------------

    def __repr__(self) -> str:
        return f"Frame(0x{self.data:08X})"

    def __eq__(self, other: object) -> bool:
        if isinstance(other, Frame):
            return self.data == other.data
        return NotImplemented


# ---------------------------------------------------------------------------
# Anonymous-namespace helpers from OpenTherm.cpp
# ---------------------------------------------------------------------------

def parity(v: int) -> int:
    """Compute even-parity over bits [30:0] and set/clear bit 31.

    Returns *v* with bit 31 adjusted so the total number of ``1`` bits
    across all 32 bits is even.
    """
    par = bin(v & 0x7FFF_FFFF).count("1")
    if par & 1:
        return v | (1 << 31)
    return v & ~(1 << 31)


def float2f88(v: float) -> int:
    """Convert a float to unsigned OpenTherm f8.8 fixed-point (uint16)."""
    return round(v * 256) & 0xFFFF


def float_from_f88(v: int) -> float:
    """Convert an unsigned f8.8 fixed-point value to float."""
    return v / 256.0


def frame_to_str(f: Frame) -> str:
    """Return a human-readable string for a frame (same format as C++)."""
    return (f"({msg_to_str(f.get_type())}"
            f"-{f.get_id()}"
            f"-/{float_from_f88(f.get_v())}"
            f"/0x{f.get_v():04X})")


# ---------------------------------------------------------------------------
# Sanity check (mirrors the C++ static_assert)
# ---------------------------------------------------------------------------

assert parity(Frame.INVALID) == Frame.INVALID, "Bad INVALID constant choice"
