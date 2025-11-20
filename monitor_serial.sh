#!/bin/bash
# Serial monitor script to capture output

PORT="/dev/cu.usbmodem8401"
BAUD=115200
OUTPUT_FILE="serial_output.log"

echo "Starting serial monitor..."
echo "Port: $PORT"
echo "Baud: $BAUD"
echo "Output will be saved to: $OUTPUT_FILE"
echo "Press Ctrl+C to stop"
echo ""

# Use screen to capture serial output
screen -L -Logfile "$OUTPUT_FILE" "$PORT" "$BAUD"

