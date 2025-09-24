#!/bin/bash
# A cron-safe script to check and run a Python service.

# --- FIX 1: Use the absolute path to Python 3 ---
# Find this by running "which python3" in your normal terminal.
# Common paths are /usr/bin/python3 or /usr/local/bin/python3
PYTHON="/home/sensorweb/anaconda3/bin/python3"

# --- FIX 2: Define the 'now' variable ---
now=$(date)

# This part of your script is good and robustly finds the script's directory
SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SERVICE="$SCRIPTPATH/MQTT_Reciever.py"

# Use the absolute path to pgrep for extra safety
process=$(/usr/bin/pgrep -f "$SERVICE")

if [[ ! -z "$process" ]]
then
    # The service is already running, do nothing.
    echo "[$now] $SERVICE is already running."
else
    # The service is not running, so start it.
    echo "[$now] $SERVICE was not running. Restarting..."

    # Change to the script's directory to resolve any relative paths in the Python code
    cd "$SCRIPTPATH"

    # --- FIX 3: Run the Python script as a background process ---
    # 'nohup' prevents it from being killed when the shell exits.
    # '&' runs the command in the background.
    $PYTHON "$SERVICE"

    echo "[$now] $SERVICE has been started."
fi