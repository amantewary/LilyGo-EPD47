#!/bin/bash
# Battery monitoring script for LilyGo EPD47
# Monitors battery voltage and charging status from serial output
# Usage: ./monitor_battery.sh [serial_port]
# Example: ./monitor_battery.sh /dev/cu.usbserial-0001

SERIAL_PORT=${1:-/dev/cu.usbserial-0001}
BAUD_RATE=115200

echo "=========================================="
echo "Battery Monitoring - LilyGo EPD47"
echo "=========================================="
echo "Serial Port: $SERIAL_PORT"
echo "Baud Rate: $BAUD_RATE"
echo ""
echo "Looking for battery readings..."
echo "Press Ctrl+C to stop"
echo "=========================================="
echo ""

# Monitor serial output and filter for battery-related messages
screen -L -Logfile /tmp/battery_monitor.log $SERIAL_PORT $BAUD_RATE 2>/dev/null &
SCREEN_PID=$!

# Wait a moment for screen to start
sleep 2

# Monitor the log file for battery updates
tail -f /tmp/battery_monitor.log 2>/dev/null | grep --line-buffered -E "(Battery:|Charging:|Updating Battery)" | while read line; do
    timestamp=$(date '+%H:%M:%S')
    echo "[$timestamp] $line"
    
    # Extract voltage and percentage if present
    if echo "$line" | grep -q "Battery:"; then
        voltage=$(echo "$line" | grep -oP '\d+\.\d+V' | head -1)
        percentage=$(echo "$line" | grep -oP '\d+%' | head -1)
        charging=$(echo "$line" | grep -oP 'Charging: \K[^)]+' | head -1)
        
        if [ ! -z "$voltage" ] && [ ! -z "$percentage" ]; then
            echo "  └─ Voltage: $voltage | Percentage: $percentage | Status: $charging"
            
            # Check if charging
            if echo "$charging" | grep -qi "yes\|USB"; then
                echo "  └─ ✓ CHARGING DETECTED (USB-C connected)"
            else
                echo "  └─ ⚠ Running on battery only"
            fi
            echo ""
        fi
    fi
done

# Cleanup
kill $SCREEN_PID 2>/dev/null
rm -f /tmp/battery_monitor.log

