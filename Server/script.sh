#!/bin/bash
# A simple script to make the Python file executable and run it.

# Make the Python file executable
PYTHON=$(which python3)
SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

# Run the Python script
SERVICE="$SCRIPTPATH/MQTT_Reciever.py

echo "$SERVICE"

process=$(pgrep -f "$SERVICE")
process=${process[0]}

if [[ ! -z $process ]]
then
    echo "$SERVICE is running at $now"
else
    cd $SCRIPTPATH
    $PYTHON $SERVICE
    echo "$SERVICE is restarted at $now"
    # echo "$SERVICE started at $now" >> $SCRIPTPATH/$LOG

fi