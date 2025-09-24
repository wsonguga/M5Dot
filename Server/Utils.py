import subprocess
import time
import json # NEW: Import the standard JSON library

# MODIFIED: The function is renamed and completely rewritten to parse JSON.
def parse_json_payload(message_string):
    """
    Parses the JSON payload from the M5Stick.
    Transforms the JSON data into the dictionary format required by write_influx.
    """
    try:
        # Decode the JSON string into a Python dictionary
        parsed_json = json.loads(message_string)

        # Build the dictionary that write_influx expects
        returnDict = {
            # Hardcode db_name and table_name as they are no longer in the payload
            "db_name": "shake",
            "table_name": "imu_data",

            # Map keys from the JSON payload to the expected keys
            "mac_address": parsed_json.get("mac"),
            "start_timestamp": float(parsed_json.get("epoch", 0)),
            "interval": float(parsed_json.get("interval_ms", 0)) / 1000.0, # Convert ms to seconds

            # Copy the data arrays directly
            "gyroX": parsed_json.get("gyroX", []),
            "gyroY": parsed_json.get("gyroY", []),
            "gyroZ": parsed_json.get("gyroZ", []),
            "accX": parsed_json.get("accX", []),
            "accY": parsed_json.get("accY", []),
            "accZ": parsed_json.get("accZ", [])
        }
        return returnDict

    except json.JSONDecodeError as e:
        print(f"Error decoding JSON: {e}. Message: '{message_string[:100]}...'")
        return None
    except (TypeError, KeyError) as e:
        print(f"Error accessing key in parsed JSON: {e}. Check payload structure.")
        return None


def write_influx(influx_data):
    """
    Writes the parsed sensor data to InfluxDB.
    (This function remains unchanged as the input dictionary format is preserved).
    """
    if influx_data is None:
        return

    influx_ip = 'https://sensorserver.engr.uga.edu'
    influx_user = 'helena'
    influx_pass = 'helena'
    db_name = influx_data['db_name']
    
    table_map = {
        'accX': 'X',
        'accY': 'Y',
        'accZ': 'Z',
        'gyroX': 'gyroX',
        'gyroY': 'gyroY',
        'gyroZ': 'gyroZ'
    }

    http_payload = ""
    start_timestamp_ns = int(influx_data['start_timestamp'] * 1e9)
    interval_ns = int(influx_data['interval'] * 1e9)
    mac_address = influx_data['mac_address']
    
    if 'accX' not in influx_data or not influx_data['accX']:
        print("Warning: No data points found in the parsed message. Skipping InfluxDB write.")
        return
        
    num_points = len(influx_data['accX'])

    for i in range(num_points):
        current_timestamp_ns = start_timestamp_ns + (i * interval_ns)
        for key, table_name in table_map.items():
            value = influx_data[key][i]
            http_payload += f"{table_name},location={mac_address} value={value} {current_timestamp_ns}\n"
            
    http_post = (
        f"curl -i -k -XPOST '{influx_ip}:8086/write?db={db_name}' "
        f"-u '{influx_user}:{influx_pass}' --data-binary '{http_payload}'"
    )

    try:
        subprocess.call(http_post, shell=True, timeout=10)
        print(f"Successfully wrote batch of {num_points} points to InfluxDB.")
    except subprocess.TimeoutExpired:
        print("Error: curl command timed out.")
    except Exception as e:
        print(f"An error occurred during InfluxDB write: {e}")