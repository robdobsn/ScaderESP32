import paho.mqtt.client as mqtt
import argparse

class MQTTClient:
    # The callback for when the client receives a CONNACK response from the server.
    def on_connect(self, client, userdata, flags, rc):
        print("Connected with result code "+str(rc))

        # Subscribing in on_connect() means that if we lose the connection and
        # reconnect then subscriptions will be renewed.
        client.subscribe("scader/out/#")

    # The callback for when a PUBLISH message is received from the server.
    def on_message(self, client, userdata, msg):
        print("=====================================")
        print(msg.topic + " " + str(msg.payload))

    def start(self, specificScader=None):
        client = mqtt.Client()
        client.on_connect = self.on_connect
        client.on_message = self.on_message

        client.connect("benbecula", 1883, 60)

        # Blocking call that processes network traffic, dispatches callbacks and
        # handles reconnecting.
        # Other loop*() functions are available that give a threaded interface and a
        # manual interface.
        client.loop_forever()

if __name__ == "__main__":

    # Add optional argument for specific scader
    parser = argparse.ArgumentParser()
    parser.add_argument("-s", "--scader", help="Scader to get MQTT for")
    args = parser.parse_args()
   
    client = MQTTClient()
    client.start(args.scader)
