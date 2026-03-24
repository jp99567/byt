#!/usr/bin/env python3
"""Central Heating Web Control

Flask web application for monitoring and controlling central heating zones.
Communicates with the bytd backend via MQTT.

Usage:
    python heating_web.py [--mqtt-host HOST] [--mqtt-port PORT] [--web-port PORT]
"""

from __future__ import annotations

import argparse
import json
import logging
import queue
import threading
import time
from dataclasses import dataclass, field

import paho.mqtt.client as mqtt
from flask import Flask, Response, render_template, request, redirect, url_for

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
logger = logging.getLogger("heating_web")

# ---------------------------------------------------------------------------
# MQTT topic constants (matching bytd backend)
# ---------------------------------------------------------------------------
TOPIC_SENS_PREFIX = "rb/stat/sens/"
TOPIC_TEV_PREFIX = "rb/stat/tev/"
TOPIC_SETPOINT_CH = "rb/stat/setpointCH"
TOPIC_SP_PREFIX = "rb/ctrl/kurenie/sp/"

# ---------------------------------------------------------------------------
# Heating zones configuration
# ---------------------------------------------------------------------------
@dataclass
class Zone:
    """Represents a single heating zone."""
    room_id: str          # MQTT room name (matches C++ roomTxt())
    display_name: str     # Human-readable name
    sensor_name: str      # Sensor name used in rb/stat/sens/<sensor_name>
    current_temp: float | None = None
    setpoint: float | None = None
    tev_pwm: float | None = None  # Valve opening [0-100%]

ZONES: list[Zone] = [
    Zone("Obyvka",     "Obývka (Living Room)",      "tObyvka"),
    Zone("Spalna",     "Spálňa (Bedroom)",           "tSpalna2"),
    Zone("Kuchyna",    "Kuchyňa (Kitchen)",          "tZadverie"),
    Zone("Izba",       "Izba (Children's Room)",     "tIzba"),
    Zone("Kupelna",    "Kúpeľňa (Bathroom)",         "KupelnaT"),
    Zone("Podlahovka", "Podlahové kúrenie (Underfloor)", "tPodlahovka"),
]

# ---------------------------------------------------------------------------
# Shared state
# ---------------------------------------------------------------------------
_state_lock = threading.Lock()
_sse_lock = threading.Lock()
_boiler_setpoint: float | None = None
_sse_clients: list[queue.Queue] = []

# Minimum allowed setpoint (°C) – prevents accidental freezing conditions
SETPOINT_MIN = 5.0
SETPOINT_MAX = 30.0


def _zone_by_room_id(room_id: str) -> Zone | None:
    for z in ZONES:
        if z.room_id == room_id:
            return z
    return None


def _zone_by_sensor(sensor_name: str) -> Zone | None:
    for z in ZONES:
        if z.sensor_name == sensor_name:
            return z
    return None


# ---------------------------------------------------------------------------
# MQTT handling
# ---------------------------------------------------------------------------
def _on_connect(client: mqtt.Client, userdata, flags, rc):
    if rc == 0:
        logger.info("MQTT connected")
        client.subscribe(TOPIC_SENS_PREFIX + "#")
        client.subscribe(TOPIC_TEV_PREFIX + "#")
        client.subscribe(TOPIC_SETPOINT_CH)
        client.subscribe(TOPIC_SP_PREFIX + "#")
    else:
        logger.warning("MQTT connect failed rc=%d", rc)


def _on_message(client: mqtt.Client, userdata, msg: mqtt.MQTTMessage):
    global _boiler_setpoint
    topic = msg.topic
    try:
        value = float(msg.payload.decode())
    except (ValueError, UnicodeDecodeError):
        return

    changed = False
    with _state_lock:
        if topic.startswith(TOPIC_SENS_PREFIX):
            sensor_name = topic[len(TOPIC_SENS_PREFIX):]
            zone = _zone_by_sensor(sensor_name)
            if zone is not None:
                zone.current_temp = value
                changed = True

        elif topic.startswith(TOPIC_TEV_PREFIX):
            room_id = topic[len(TOPIC_TEV_PREFIX):]
            zone = _zone_by_room_id(room_id)
            if zone is not None:
                zone.tev_pwm = value
                changed = True

        elif topic == TOPIC_SETPOINT_CH:
            _boiler_setpoint = value
            changed = True

        elif topic.startswith(TOPIC_SP_PREFIX):
            room_id = topic[len(TOPIC_SP_PREFIX):]
            zone = _zone_by_room_id(room_id)
            if zone is not None:
                zone.setpoint = value
                changed = True

    if changed:
        _push_sse_update()


def _push_sse_update():
    """Notify all SSE clients that state has changed."""
    with _state_lock:
        data = _build_state_dict()
    payload = "data: " + json.dumps(data) + "\n\n"
    dead = []
    with _sse_lock:
        clients = list(_sse_clients)
    for q in clients:
        try:
            q.put_nowait(payload)
        except queue.Full:
            dead.append(q)
    if dead:
        with _sse_lock:
            for q in dead:
                try:
                    _sse_clients.remove(q)
                except ValueError:
                    pass


def _build_state_dict() -> dict:
    """Build a JSON-serialisable snapshot of the current state (call with lock held or after copy)."""
    return {
        "boiler_setpoint": _boiler_setpoint,
        "zones": [
            {
                "room_id": z.room_id,
                "display_name": z.display_name,
                "current_temp": z.current_temp,
                "setpoint": z.setpoint,
                "tev_pwm": z.tev_pwm,
            }
            for z in ZONES
        ],
    }


# ---------------------------------------------------------------------------
# Flask application
# ---------------------------------------------------------------------------
app = Flask(__name__)
_mqtt_client: mqtt.Client | None = None


def _valve_class(tev_pwm: float | None) -> str:
    if tev_pwm is None:
        return "valve-unknown"
    if tev_pwm >= 80:
        return "valve-open"
    if tev_pwm > 10:
        return "valve-partial"
    return "valve-closed"


def _valve_label(tev_pwm: float | None) -> str:
    if tev_pwm is None:
        return "—"
    return f"{tev_pwm:.0f}%"


app.jinja_env.globals["valve_class"] = _valve_class
app.jinja_env.globals["valve_label"] = _valve_label


@app.route("/")
def index():
    with _state_lock:
        state = _build_state_dict()
    return render_template("index.html", state=state)


@app.route("/setpoint", methods=["POST"])
def set_setpoint():
    room_id = request.form.get("room_id", "").strip()
    try:
        value = float(request.form.get("value", ""))
    except ValueError:
        return "Invalid value", 400

    if not (SETPOINT_MIN <= value <= SETPOINT_MAX):
        return f"Value out of range [{SETPOINT_MIN}, {SETPOINT_MAX}]", 400

    zone = _zone_by_room_id(room_id)
    if zone is None:
        return "Unknown room", 404

    topic = TOPIC_SP_PREFIX + room_id
    if _mqtt_client is not None:
        result = _mqtt_client.publish(topic, str(value), retain=True)
        if result.rc != mqtt.MQTT_ERR_SUCCESS:
            logger.warning("Publish to %s failed: rc=%d", topic, result.rc)
        else:
            logger.info("Published setpoint %s -> %.1f", topic, value)

    # Optimistic local update so the UI reflects the change immediately,
    # even before the retained MQTT echo arrives.
    with _state_lock:
        zone.setpoint = value

    _push_sse_update()
    return redirect(url_for("index"))


@app.route("/events")
def sse_stream():
    """Server-Sent Events endpoint for real-time state updates."""
    q: queue.Queue = queue.Queue(maxsize=20)
    with _sse_lock:
        _sse_clients.append(q)

    def generate():
        # Send current state immediately on connect
        with _state_lock:
            data = _build_state_dict()
        yield "data: " + json.dumps(data) + "\n\n"
        try:
            while True:
                try:
                    msg = q.get(timeout=30)
                    yield msg
                except queue.Empty:
                    yield ": keepalive\n\n"
        except GeneratorExit:
            pass
        finally:
            with _sse_lock:
                try:
                    _sse_clients.remove(q)
                except ValueError:
                    pass

    return Response(generate(), mimetype="text/event-stream",
                    headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"})


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main():
    global _mqtt_client

    parser = argparse.ArgumentParser(description="Central Heating Web Control")
    parser.add_argument("--mqtt-host", default="localhost", help="MQTT broker host")
    parser.add_argument("--mqtt-port", type=int, default=1883, help="MQTT broker port")
    parser.add_argument("--web-port", type=int, default=5000, help="Flask web server port")
    parser.add_argument("--web-host", default="0.0.0.0", help="Flask web server host")
    parser.add_argument("--debug", action="store_true", help="Enable Flask debug mode")
    args = parser.parse_args()

    _mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, client_id="heating-web")
    _mqtt_client.on_connect = _on_connect
    _mqtt_client.on_message = _on_message
    _mqtt_client.connect_async(args.mqtt_host, args.mqtt_port)
    _mqtt_client.loop_start()

    app.run(host=args.web_host, port=args.web_port, debug=args.debug, threaded=True)

    _mqtt_client.loop_stop()
    _mqtt_client.disconnect()


if __name__ == "__main__":
    main()
