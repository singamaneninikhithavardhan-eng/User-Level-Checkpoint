#!/bin/bash

PID=$1

if [ -z "$PID" ]; then
  echo "Usage: ./script.sh <pid>"
  exit 1
fi

while true
do
  echo "Taking checkpoint for PID $PID..."

  sudo ./attach $PID

  echo "Checkpoint done."

  sleep 10   # interval (change if needed)
done
