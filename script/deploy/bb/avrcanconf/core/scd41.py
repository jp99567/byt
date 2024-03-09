import enum
import core.bytcan
import core.config
from core.config import SvcProtocol


def conv_t_offset(val_u16):
    return (175/(2**16-1))*val_u16


def conv_inv_t_offset(val_float):
    return int(val_float * (2**16-1)/175)


def sensirion_common_generate_crc(data):
    crc = 0xFF
    for cur_byte in data:
        crc ^= cur_byte
        for bit_idx in reversed(range(8)):
            if crc & 0x80:
                crc = (crc << 1) ^ 0x31
            else:
                crc <<= 1
            crc &= 0xff
    return crc


class ResponseCode(enum.IntEnum):
    Idle = enum.auto()
    Pending_IO = enum.auto()
    Error = enum.auto()


class SCD41_CMD(enum.IntEnum):
    start_periodic_measurement = 0x21b1
    read_measurement = 0xec05
    stop_periodic_measurement = 0x3f86
    set_temperature_offset = 0x241d
    get_temperature_offset = 0x2318
    set_sensor_altitude = 0x2427
    get_sensor_altitude = 0x2322
    perform_forced_recalibration = 0x362f
    set_automatic_self_calibration_enabled = 0x2416
    get_automatic_self_calibration_enabled = 0x2313
    get_data_ready_status = 0xe4b8
    persist_settings = 0x3615
    get_serial_number = 0x3682
    perform_self_test = 0x3639
    perform_factory_reset = 0x3632
    reinit = 0x3646


cmdDisableStateMachine = (0,0)
cmdEnableTwi = (0,1)
cmdEnableStateMachine = (0,2)


def cmdWriteCommand(cmd):
    hb = cmd >> 8
    lb = cmd & 0xFF
    return [1, hb, lb]


def cmdWriteWord(cmd, val):
    req = cmdWriteCommand(cmd)
    data = [ val >> 8, cmd & 0xFF ]
    req.extend(data)
    req.append(sensirion_common_generate_crc(data))
    return req


def cmdReadWord(cmd):
    req = cmdWriteCommand(cmd)
    req.append(3)
    return req


def requestI2C(nodebus):
    def req(cmd, candata=None):
        nodebus.svcTransfer(cmd, candata)
        trsp = nodebus.svcTransfer(SvcProtocol.CmdSCD41Test)
        while ResponseCode(trsp[1]) == ResponseCode.Pending_IO:
            trsp = nodebus.svcTransfer(SvcProtocol.CmdOwGetReplyCode)
        if ResponseCode(trsp[1]) == ResponseCode.Error:
            print(f"scd41 i2c {cmd} response with error {trsp}")
        return trsp
    return req


def check_word(data):
    crc = sensirion_common_generate_crc(data[0:2])
    if crc != data[2]:
        raise RuntimeError("crc does not match")
    return 256*data[0] + data[1]


class SCD41:
    def __init__(self, candev, id) -> None:
        self.node = core.bytcan.NodeBus(core.bytcan.openBus(candev), id)


    def read_params(self):
        self.node.svcTransfer(SvcProtocol.CmdSCD41Test, cmdDisableStateMachine)
        self.node.svcTransfer(SvcProtocol.CmdSCD41Test, cmdEnableTwi)
        i2creq = requestI2C(self.node)
        i2creq(cmdWriteCommand(SCD41_CMD.stop_periodic_measurement))
        temp_offset = conv_t_offset(check_word(i2creq(cmdReadWord(SCD41_CMD.get_temperature_offset))))
        altit = check_word(i2creq(cmdReadWord(SCD41_CMD.get_sensor_altitude)))
        print(f"temp_offset:{temp_offset} altit:{altit}")
