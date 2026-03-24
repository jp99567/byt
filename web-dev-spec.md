# byt — Home Automation Web Dashboard Specification

## Project Overview

**byt** is a home automation system for a residential apartment, running on a
BeagleBone Black (BBB) embedded Linux board. The backend daemon (`bytd`) is
written in C++17 and exposes all real-time state via an **MQTT broker** (port
1883). A web dashboard should connect to the same broker and provide a
user-friendly interface for monitoring and controlling the apartment.

Language note: variable and device names are Slovak/Czech. Translations are
provided throughout this document.

---

## Architecture Overview

```
+-------------------+       MQTT (port 1883)      +-------------------+
|   bytd daemon     |  <----------------------->  |   Web Dashboard   |
|  (BeagleBone BB)  |                             |   (browser app)   |
+-------------------+                             +-------------------+
        |
        | (hardware)
        +-- CAN bus (distributed I/O nodes)
        +-- 1-Wire bus (temperature sensors)
        +-- OpenTherm (gas boiler)
        +-- UART (recuperation unit)
        +-- GPIO (pump, electricity/water meter)
        +-- I2C (PWM expander for TEV valves)
```

The web dashboard **subscribes** to status topics and **publishes** to control
topics.

---

## MQTT Connection

| Parameter | Value |
|-----------|-------|
| Host | hostname of the BeagleBone (configurable via `hwif`) |
| Port | 1883 (unencrypted) / 8883 (TLS, recommended for production) |
| Client ID | choose a unique string (e.g. `web-dashboard`) |
| Subscribe wildcard | `rb/#` (all topics) |

---

## MQTT Topic Reference

### Topic Prefix Convention

| Prefix | Direction | Meaning |
|--------|-----------|---------|
| `rb/ctrl/` | → broker (write) | Commands from dashboard to backend |
| `rb/stat/` | ← broker (read) | Status/sensor data from backend |
| `rb/ctrl/dev/` | → write + subscribe for feedback | Device on/off; the daemon publishes the new state back to the same topic with `retain=true` whenever a physical button changes state, so subscribing to it also provides current device state |

---

### 1. Temperature & Environmental Sensors

All sensors publish float values (e.g. `"21.500000"`) to:

```
rb/stat/sens/<sensor_name>
```

#### BeagleBone 1-Wire Sensors (BBOw)

| Topic | Description | Unit |
|-------|-------------|------|
| `rb/stat/sens/tBoiler` | Boiler (DHW) tank temperature | °C |
| `rb/stat/sens/tPavlac` | Balcony (outdoor) temperature | °C |
| `rb/stat/sens/tretKupelna` | Bathroom heating circuit return | °C |
| `rb/stat/sens/tretKupelnaP` | Bathroom floor heating supply | °C |
| `rb/stat/sens/tretPodlaha` | Floor heating return | °C |
| `rb/stat/sens/tretSpalna1` | Bedroom heating circuit return #1 | °C |
| `rb/stat/sens/tretSpalna2` | Bedroom heating circuit return #2 | °C |
| `rb/stat/sens/tretObyvka1` | Living-room heating circuit return #1 | °C |
| `rb/stat/sens/tretObyvka2` | Living-room heating circuit return #2 | °C |
| `rb/stat/sens/tretIzba` | Room/study heating circuit return | °C |
| `rb/stat/sens/tretKuchyna` | Kitchen heating circuit return | °C |
| `rb/stat/sens/tObyvka` | Living-room air temperature | °C |

#### CAN-Bus Sensors (NodeCAN)

| Topic | Description | Unit |
|-------|-------------|------|
| `rb/stat/sens/tIzba` | Room/study air temperature | °C |
| `rb/stat/sens/tZadverie` | Hallway air temperature | °C |
| `rb/stat/sens/tVodaKupelka` | Bathroom water temp #1 | °C |
| `rb/stat/sens/tVodaKupelka2` | Bathroom water temp #2 | °C |
| `rb/stat/sens/tPodlahovka` | Floor sensor temperature (DS18S20) | °C |
| `rb/stat/sens/tSpalna2` | Bedroom temperature (1-wire via CAN) | °C |

#### Sensorion Sensors (CAN, Kupelka node)

| Topic | Description | Unit |
|-------|-------------|------|
| `rb/stat/sens/tSpalna` | Bedroom air temperature (SCD41) | °C |
| `rb/stat/sens/SpalnaRH` | Bedroom relative humidity (SCD41) | % RH |
| `rb/stat/sens/SpalnaCO2` | Bedroom CO₂ concentration (SCD41) | ppm |
| `rb/stat/sens/KupelnaT` | Bathroom air temperature (SHT11) | °C |
| `rb/stat/sens/KupelnaRH` | Bathroom relative humidity (SHT11) | % RH |

#### OpenTherm (Gas Boiler) Sensors

| Topic | Description | Unit |
|-------|-------------|------|
| `rb/stat/sens/tCH` | Central heating flow temperature | °C |
| `rb/stat/sens/tDHW` | Domestic hot water temperature | °C |

---

### 2. Energy & Utility Meters

| Topic | Description | Unit | Direction |
|-------|-------------|------|-----------|
| `rb/stat/power` | Current electricity consumption | W (float) | ← stat |
| `rb/stat/elektromer_total_kwh` | Cumulative electricity total | kWh (float) | ← stat |
| `rb/stat/prietok` | Current water flow rate | L/min (float) | ← stat |
| `rb/stat/vodomer_total_litre` | Cumulative water total | L (float) | ← stat |
| `rb/stat/plynomer_total_m3` | Cumulative gas total | m³ (float) | ← stat |

---

### 3. On/Off Devices (Lights & Switches)

Device state is reported **and** controlled on the same topic:

```
rb/ctrl/dev/<DeviceName>
```

Values: `"1"` = on, `"0"` = off (integer string).

| Topic | Device | Notes |
|-------|--------|-------|
| `rb/ctrl/dev/SvetloKupelna` | Bathroom light | |
| `rb/ctrl/dev/SvetloSpalna` | Bedroom light | |
| `rb/ctrl/dev/SvetloChodbicka` | Hallway light | |
| `rb/ctrl/dev/SvetloStol` | Table lamp (living room) | |
| `rb/ctrl/dev/SvetloStena` | Wall lamp (living room) | |
| `rb/ctrl/dev/SvetloObyvka` | Living-room main light | |
| `rb/ctrl/dev/SvetloKuchyna` | Kitchen light | inverted output |
| `rb/ctrl/dev/SvetloIzba` | Room/study light | inverted output |
| `rb/ctrl/dev/SvetloPavlac` | Balcony light | inverted output |
| `rb/ctrl/dev/SvetloWc` | WC light | inverted output |
| `rb/ctrl/dev/SvetloLedStrip` | Kitchen LED strip on/off | dimmable via separate topic |
| `rb/ctrl/dev/Vetranie` | Ventilation boost (100% recuperation flow) | |
| `rb/ctrl/dev/Brana` | Gate (monostable, 0.25 s pulse) | publish `"1"` to trigger |
| `rb/ctrl/dev/DverePavlac` | Balcony door release (monostable, 2 s pulse) | publish `"1"` to trigger |

**Note:** Monostable devices (Brana, DverePavlac) auto-reset; a single `"1"`
publish triggers a timed pulse. Their state briefly goes to `1` then back to `0`.

---

### 4. Dimmable Lights

```
rb/ctrl/lightdimm/<name>
```

| Topic | Device | Value range |
|-------|--------|-------------|
| `rb/ctrl/lightdimm/linkaLedStrip` | Kitchen LED strip brightness | `"0"` – `"100"` (percent, float) |

---

### 5. Heating Control (Kurenie)

#### Room Setpoints

Publish the desired temperature setpoint (float string, °C):

```
rb/ctrl/kurenie/sp/<RoomName>
```

| Topic | Room | Notes |
|-------|------|-------|
| `rb/ctrl/kurenie/sp/Obyvka` | Living room | |
| `rb/ctrl/kurenie/sp/Spalna` | Bedroom | |
| `rb/ctrl/kurenie/sp/Kuchyna` | Kitchen | sensor = tZadverie |
| `rb/ctrl/kurenie/sp/Izba` | Room/study | |
| `rb/ctrl/kurenie/sp/Kupelna` | Bathroom | |
| `rb/ctrl/kurenie/sp/Podlahovka` | Floor heating zone | max CH temp limited to 50 °C |

Set setpoint to `"0"` (or `"nan"`) to disable regulation for that room.

#### Thermoelectric Valve Override

Override a specific room's valve PWM (0–100 %):

```
rb/ctrl/override/tev/<RoomName>    (float string, %)
```

Set to `"nan"` to release the override and return to automatic regulation.

#### Central Heating Override

Override the CH flow temperature setpoint:

```
rb/ctrl/override/setpointCH    (float string, °C)
```

Set to `"nan"` to release the override.

---

### 6. OpenTherm / Boiler Control

| Topic | Description | Value |
|-------|-------------|-------|
| `rb/ctrl/ot/setpoint/dhw` | DHW (hot water) setpoint | float °C (default 38) |
| `rb/ctrl/pumpa` | Central heating pump | `"1"` start / `"0"` stop |

---

### 7. 4-Way Valve (Ventil4w)

Controls the hydraulic 4-way valve that switches between floor heating and
radiator circuit.

| Topic | Description | Value |
|-------|-------------|-------|
| `rb/ctrl/ventil4w/target` | Move valve to target position | string (position name) |
| `rb/stat/ventil4w/position` | Current valve position | string |

---

### 8. PIR Motion Sensor

| Topic | Description | Value |
|-------|-------------|-------|
| `rb/stat/pirWC` | Motion detected in WC | `"1"` motion / `"0"` clear |

---

### 9. Command Interface

Send a command string to:

```
rb/ctrl/req
```

Responses arrive on:

```
rb/stat/response
```

Supported commands:

| Command | Description | Example |
|---------|-------------|---------|
| `rev` | Get firmware git revision | `rev` → `"fdd6d07-dirty"` |
| `setLogLevel <level>` | Change log verbosity | `setLogLevel debug` |
| `otTransfer <hex_frame>` | Send raw OpenTherm frame | `otTransfer 0x04050000` |
| `setNewTotal elektromer <kWh>` | Reset electricity meter total | `setNewTotal elektromer 12345.6` |
| `setNewTotal vodomer <L>` | Reset water meter total | `setNewTotal vodomer 987654` |
| `setNewTotal plynomer <m3>` | Reset gas meter total | `setNewTotal plynomer 1234.5` |

---

## Rooms Summary

| Slovak Name | English | Key Sensors | Heating Zone |
|-------------|---------|-------------|--------------|
| Obyvka | Living room | tObyvka, tretObyvka1/2 | Obyvka |
| Spalna | Bedroom | tSpalna, tSpalna2, SpalnaRH, SpalnaCO2 | Spalna |
| Kuchyna | Kitchen | tZadverie | Kuchyna |
| Izba | Room/study | tIzba | Izba |
| Kupelka (also Kupelna) | Bathroom | KupelnaT, KupelnaRH, tVodaKupelka | Kupelna (setpoint topic uses `Kupelna`) |
| Podlahovka | Floor heating | tPodlahovka, tretKupelnaP | Podlahovka |
| Chodba / Chodbicka | Hallway | tZadverie | — |
| Zadverie | Entrance | — | — |
| Pavlac | Balcony | tPavlac | — |
| WC | WC | pirWC | — |

---

## Suggested Dashboard Layout

### Overview Page

- **Floor plan** (or card grid) showing all rooms
- Each room card shows:
  - Current temperature
  - Heating setpoint with +/− controls
  - Active lights (on/off toggle buttons)

### Heating Page

- Table of all rooms with current temp vs setpoint
- Sliders or numeric inputs for setpoints
- CH flow temperature (`tCH`) vs setpoint
- Boiler flame status (parse from OpenTherm frame if needed)
- Pump state toggle
- DHW setpoint + DHW temperature (`tDHW`)
- TEV override section (per room)

### Energy Monitoring Page

- Real-time electricity power gauge (W)
- Historical chart: electricity (kWh/day), water (L/day), gas (m³/day)
- Current water flow rate
- Running totals for electricity (kWh), water (L), gas (m³)

### Lights & Devices Page

- Toggle buttons for every light
- LED strip brightness slider (`rb/ctrl/lightdimm/linkaLedStrip`)
- Gate and balcony door trigger buttons (with confirmation dialog)

### Climate / Air Quality Page

- Bedroom CO₂ ppm with colour-coded alert thresholds:
  - ≤ 800 ppm: 🟢 Good
  - 801–1200 ppm: 🟡 Moderate
  - > 1200 ppm: 🔴 Poor
- Bedroom + Bathroom relative humidity
- Outdoor temperature (tPavlac)
- Ventilation boost toggle (Vetranie)

---

## Data Format Notes

- All MQTT payloads are **plain UTF-8 strings** — no JSON envelope.
- Numeric values use `std::to_string()` format, e.g. `"21.500000"` for float,
  `"1"` / `"0"` for boolean-ish integers.
- Sensor values that are unavailable/invalid will be `"nan"`.
- Retained messages: sensors and device states use `retain=true`; commands use
  `retain=false`.

---

## Tech Stack Recommendations

| Layer | Suggestion |
|-------|-----------|
| MQTT client | [MQTT.js](https://github.com/mqttjs/MQTT.js) (WebSocket) or [Paho JS](https://eclipse.dev/paho/index.php?page=clients/js/index.php) |
| Framework | React, Vue 3, or Svelte |
| Charts | Chart.js or Recharts |
| State management | Zustand, Pinia, or Svelte stores |
| Styling | Tailwind CSS or Material UI |

The broker must expose an **MQTT-over-WebSocket** endpoint (typically port 9001
or 8883) for browser clients to connect. Configure mosquitto:

```
listener 9001
protocol websockets
```

---

## Security Notes

- The system currently has no MQTT authentication configured — add username/password
  or TLS for internet-facing deployments.
- Gate (`Brana`) and door (`DverePavlac`) controls should require explicit user
  confirmation in the UI before publishing.
