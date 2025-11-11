import csv
import re
import sys

bytd_pins = ("P9_19", "P9_20", "P9_24", "P9_26", "P9_28", "P9_27", "P9_29", "P9_31", "P9_25", "P8_16", "P8_12", "P9_11", "P9_13", "P8_40", "P8_42", "P8_44", "P8_46")
emmc_pins = list(range(32,32+8))
emmc_pins.extend([32+30, 32+31])
hdmi_pins = list(range(2*32+6, 2*32+18))
hdmi_pins.extend([8,9,10,11,999])
hdmi_pins.extend(list(range(2*32+22, 2*32+26)))

col_name = 'pinheader_name'
col_addr = 'addr'
col_modes = 'modes'
col_gpioid = 'gpioid'

def load_func_table():
    with open('pinmux-table.csv') as csvfile:
        pinmextable_reader = csv.reader(csvfile, delimiter=';')
        header = next(pinmextable_reader)
        table = { col_name: [], col_addr: [], col_modes: [], col_gpioid: []}
        for row in pinmextable_reader:
            if len(row) < 11:
                break
            try:
                table[col_addr].append(int(row[1], 16) + 0x44E10000)
                table[col_name].append(row[0])
                table[col_gpioid].append(int(row[10]))
                table[col_modes].append(row[2:10])
            except ValueError:
                continue
        table[col_addr].append(0x44e109b0)
        table[col_name].append('XDMA_EVENT_INTR0')
        table[col_gpioid].append(999)
        table[col_modes].append(list(range(7)))
        return table

def load_cur_pinmux(file):
    pinre = re.compile(r'pin \d+ \(PIN\d+\).* (\w+) (\w+) pinctrl-single.*')
    pinctrl = dict()
    with open(file) as f:
        for line in f:
            matched = pinre.match(line)
            if not matched:
                continue
            addr = int(matched.group(1), 16)
            pinctrl_regval = int(matched.group(2), 16)
            pinctrl[addr] = pinctrl_regval
    return pinctrl


def decode_pinctrl(val):
    muxflags = []
    if not val&(1<<3):
        if val&(1<<4):
            muxflags.append('puUP')
        else:
            muxflags.append('puDOWN')
    if val&(1<<5):
        muxflags.append('RXEN')
    if val&(1<<6):
        muxflags.append('SLOW')
    muxmode = val & 7
    return muxmode, muxflags


pinmodes_table = load_func_table()
cur_pinmux_file = '/sys/kernel/debug/pinctrl/44e10800.pinmux-pinctrl-single/pins'
if len(sys.argv) > 1:
    cur_pinmux_file = sys.argv[1]
pinmux_map = load_cur_pinmux(cur_pinmux_file)

for name in bytd_pins:
    idx = pinmodes_table[col_name].index(name)
    mode, flags = decode_pinctrl(pinmux_map[pinmodes_table[col_addr][idx]])
    print(f"BYTD {name} {pinmodes_table[col_modes][idx][mode]} {flags}")


for id in emmc_pins:
    idx = pinmodes_table[col_gpioid].index(id)
    mode, flags = decode_pinctrl(pinmux_map[pinmodes_table[col_addr][idx]])
    print(f"EMMC {pinmodes_table[col_name][idx]} {pinmodes_table[col_modes][idx][mode]} {flags}")

for id in hdmi_pins:
    idx = pinmodes_table[col_gpioid].index(id)
    mode, flags = decode_pinctrl(pinmux_map[pinmodes_table[col_addr][idx]])
    print(f"HDMI {pinmodes_table[col_name][idx]} {pinmodes_table[col_modes][idx][mode]} {flags}")
