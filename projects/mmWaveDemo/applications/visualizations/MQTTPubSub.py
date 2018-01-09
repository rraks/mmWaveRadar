import sys
import os
import inspect
import paho.mqtt.client as mqtt
import threading


class MQTTPubSub:
    def __init__(self, params):

        self.url = params["url"]
        self.port = params["port"]
        self.timeout = params["timeout"]
        self.topic = params["topic"]
        self._mqttc = mqtt.Client(None)
        
        if( "username" in params):
            self.username = params["username"]
            if( "password" in params):
                self.password = params["password"]
                self._mqttc.username_pw_set(self.username,self.password)

        if ("onMessage" in params):
            self._mqttc.on_message = params["onMessage"]
        if ("onConnect" in params):
            self._mqttc.on_connect = params["onConnect"]
        if ("onPublish" in params):
            self._mqttc.on_publish = params["onPublish"]
        if ("onSubscribe" in params):
            self._mqttc.on_subscribe = params["onSubscribe"]
        if ("onDisconnect" in params):
            self._mqttc.on_disconnect = params["onDisconnect"]


    def publish(self, topic, payload):
        self._mqttc.publish(topic, payload)


    def run(self):
        self._mqttc.connect(self.url, self.port, self.timeout)
        self._mqttc.subscribe(self.topic, 0)
        threading.Thread(target=self._mqttc.loop_start()).start()
        






