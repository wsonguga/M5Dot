import paho.mqtt.client as mqtt
import threading
from config import GetInfo
# MODIFIED: Import the new parsing function
from Utils import parse_json_payload, write_influx
import time
import queue

message_queue = queue.Queue()

def config():
    values = GetInfo()
    return values[0], values[1], values[2]

def on_connect(client, userdata, flags, rc):
    print("on_connect callback triggered.")
    if rc == 0:
        print("Connected to MQTT broker successfully.")
        broker, port, topic = config()
        print(f"Subscribing to topic: '{topic}'")
        client.subscribe(topic, qos=1)
    else:
        print(f"Failed to connect, return code {rc}\n")

def on_message(client, userdata, msg):
    try:
        message_queue.put(msg.payload.decode())
    except Exception as e:
        print(f"Error putting message in queue: {e}")

def database_worker():
    while True:
        try:
            message = message_queue.get()
            print(f"Worker processing message of length: {len(message)}. Queue size: {message_queue.qsize()}")
            
            # MODIFIED: Call the new JSON parsing function
            parsed_data = parse_json_payload(message)
            if parsed_data:
                write_influx(parsed_data)
            
            message_queue.task_done()
        except Exception as e:
            print(f"Error in database_worker: {e}")

def main():
    broker, port, topic = config()
    client_id = f'python-mqtt-receiver-{int(time.time())}'
    client = mqtt.Client(client_id=client_id, callback_api_version=mqtt.CallbackAPIVersion.VERSION1)

    print("--- Configuration ---")
    print(f"Broker: {broker}, Port: {port}, Topic: {topic}")
    print("---------------------")
    
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(broker, port, 60)

    worker_thread = threading.Thread(target=database_worker)
    worker_thread.daemon = True
    worker_thread.start()
    print("Database worker thread started.")

    print("MQTT client started. Waiting for messages...")
    client.loop_forever()

if __name__ == '__main__':
    main()