#!/usr/bin/python3

import argparse
import yaml
import paho.mqtt.client as mqtt
import json

parser = argparse.ArgumentParser(description='configure homeasistent mqtt integration')
parser.add_argument('--yamlfile', default='config.yaml')
parser.add_argument('--broker', default='localhost')
parser.add_argument('--dry_run', action='store_true')

args = parser.parse_args()

def mqtt_on_connect(client, sensors, flags, rc):
    jsontemplate = json.loads('{"device_class":"temperature", "value_template":"{{ value }}"}')


    for sens in sensors:
        if sens[:4] == 'tret':
            continue
        js = jsontemplate
        js['state_topic'] = f"rb/stat/sens/{sens}"
        js['unit_of_measurement'] = "\u00b0"+" C"
        js['unique_id'] = sens
        js['friendly_name'] = sens
        js['name'] = sens
        js['device'] = {"identifiers": f"sens_{sens}_idtag", "name": "T"}
        if client is None:
            print(json.dumps(js, indent=4, ensure_ascii=False))
        else:
            payload = bytearray(json.dumps(js,ensure_ascii=True), encoding='utf8')
            client.publish(f"homeassistant/sensor/{sens}/config", payload, 0, True)
        # client.publish(f"homeassistant/sensor/{sens}/config", "", 0, True)

    if client is not None:
        client.disconnect()


def mqtt_discovery():
    sensors = []
    with open(args.yamlfile, "r") as stream:
        config = yaml.safe_load(stream)
        for node in config['NodeCAN']:
            if 'OwT' in config['NodeCAN'][node]:
                for sens in config['NodeCAN'][node]['OwT']:
                    sensors.append(sens)
        for node in config['BBOw']:
            sensors.append(node)

    if args.dry_run:
        mqtt_on_connect(None, sensors, None, None)
    else:
        #client_id = f"config_ha_mqtt_discovery"
        mqttc = mqtt.Client(userdata=sensors)
        mqttc.on_connect = mqtt_on_connect
        mqttc.connect(args.broker)
        mqttc.loop_forever()


mqtt_discovery()
