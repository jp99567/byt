#!/usr/bin/python3

import argparse
import yaml
import paho.mqtt.client as mqtt
import json

parser = argparse.ArgumentParser(description='configure homeasistent mqtt integration')
parser.add_argument('--yamlfile', default='config.yaml')
parser.add_argument('--broker', default='localhost')

args = parser.parse_args()


def mqtt_on_connect(client, sensors, flags, rc):
    jsontemplate = json.loads('{"device_class":"temperature",'
                              '"state_topic":"homeassistant/sensor/sensorBedroom/state",'
                              '"unit_of_measurement":"°C","value_template":"{{ value }}"}')

    print(f"JSON: {jsontemplate}")
    for sens in sensors:
        js = jsontemplate
        js['state_topic'] = f"rb/stat/sens/{sens}"
        js['unique_id'] = sens
        js['friendly_name'] = sens
        js['name'] = sens
        js['device'] = {"identifiers": f"sens_{sens}_idtag", "name": "T"}
        client.publish(f"homeassistant/sensor/{sens}/config", json.dumps(js), 0, True)
        # client.publish(f"homeassistant/sensor/{sens}/config", "", 0, True)

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
    print(f"{sensors}")

    client_id = f"config_ha_mqtt_discovery"
    mqttc = mqtt.Client(client_id, userdata=sensors)
    mqttc.on_connect = mqtt_on_connect
    mqttc.connect(args.broker)
    mqttc.loop_forever()


mqtt_discovery()
