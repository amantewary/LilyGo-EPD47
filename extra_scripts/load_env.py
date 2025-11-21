"""
PlatformIO Extra Script: Load OTA Configuration from .platformio_env

This script loads OTA upload configuration (IP address and password) from a
.platformio_env file in the project root. This keeps sensitive information
out of platformio.ini and git.

Setup:
1. Copy .platformio_env.example to .platformio_env
2. Update .platformio_env with your device IP and OTA password
3. The script automatically loads these values when building/uploading

File format (.platformio_env):
    OTA_IP=192.168.1.100
    OTA_PASSWORD=epd47ota
"""

Import("env")
import os

# Path to .platformio_env file in project root
env_file = os.path.join(env["PROJECT_DIR"], ".platformio_env")

# Load environment variables from .platformio_env file
if os.path.exists(env_file):
    print(f"Loading OTA configuration from {env_file}...")
    with open(env_file, 'r') as f:
        for line in f:
            line = line.strip()
            # Skip comments and empty lines
            if line and not line.startswith('#'):
                if '=' in line:
                    key, value = line.split('=', 1)
                    key = key.strip()
                    value = value.strip()
                    # Set environment variable
                    os.environ[key] = value
                    # Mask password in output for security
                    display_value = value if key != 'OTA_PASSWORD' else '*' * len(value)
                    print(f"  Loaded: {key} = {display_value}")
else:
    print(f"Warning: {env_file} not found.")
    print(f"  Using defaults: OTA_IP=192.168.1.100, OTA_PASSWORD=epd47ota")
    print(f"  To configure OTA, copy .platformio_env.example to .platformio_env")

# Get OTA configuration from environment variables (with defaults)
ota_ip = os.environ.get('OTA_IP', '192.168.1.100')
ota_password = os.environ.get('OTA_PASSWORD', 'epd47ota')

# Only modify upload settings if we're in the OTA environment
if "T5-ePaper-S3-OTA" in str(env.get("PIOENV", "")):
    # Set upload port and flags for OTA upload
    env.Replace(
        UPLOAD_PORT=ota_ip,
        UPLOAD_FLAGS=["--auth=" + ota_password]
    )
    print(f"OTA Configuration applied: IP={ota_ip}, Password={'*' * len(ota_password)}")

