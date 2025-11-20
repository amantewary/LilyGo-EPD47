# EPD47 Dashboard Web App

A Next.js web application that emulates the EPD47 e-paper display layout, integrates with Home Assistant for todos/calendars, and supports OTA updates to the ESP32 device.

## Features

- **Dashboard Preview**: Emulates the 960x540 e-paper display layout
- **Home Assistant Integration**: Fetches weather, todos, calendar events, and quotes
- **Entity Configuration**: Select which Home Assistant entities to display
- **OTA Updates**: Upload firmware updates to the ESP32 device over-the-air
- **Self-Hostable**: Docker container ready for Proxmox deployment

## Prerequisites

- Node.js 20+ or Docker
- Home Assistant instance with REST API access
- Long-lived access token from Home Assistant
- ESP32 device with OTA enabled

## Installation

### Local Development

1. Clone the repository and navigate to the web-app directory:
```bash
cd web-app
```

2. Install dependencies:
```bash
npm install
```

3. Copy `.env.example` to `.env.local` and configure:
```bash
cp .env.example .env.local
```

4. Run the development server:
```bash
npm run dev
```

5. Open [http://localhost:3000](http://localhost:3000)

### Docker Deployment

1. Build and run with Docker Compose:
```bash
docker-compose up -d
```

2. Or build manually:
```bash
docker build -t epd47-dashboard .
docker run -p 3000:3000 \
  -e HA_HOST=192.168.2.46 \
  -e HA_PORT=8123 \
  -e HA_TOKEN=your_token \
  -e OTA_DEVICE_IP=192.168.2.89 \
  -e OTA_PASSWORD=epd47ota \
  epd47-dashboard
```

## Configuration

### Initial Setup

1. Navigate to the Settings page (`/settings`)
2. Enter your Home Assistant configuration:
   - Host/IP address
   - Port (default: 8123)
   - Long-lived access token
3. Select entities:
   - Weather entity
   - Quote sensor entity
   - Todo entities (multiple selection)
   - Calendar entities (multiple selection)
4. Configure OTA settings:
   - Device IP address
   - OTA password (must match device configuration)

Configuration is saved in browser localStorage.

## Usage

### Dashboard

The main dashboard (`/`) displays:
- Clock (updates every minute)
- Weather information
- Current date
- Daily quote (rotates every 3 hours)
- Todo list (items due today)
- Upcoming calendar events
- Mini calendar (current week)

### OTA Updates

1. Navigate to the OTA page (`/ota`)
2. Select a firmware `.bin` file
3. Click "Upload Firmware"
4. Wait for the upload to complete (progress bar shown)
5. Device will restart automatically

**Note**: Ensure the device is powered on and connected to WiFi before uploading.

## API Routes

- `/api/ha/weather` - Fetch weather data
- `/api/ha/todos` - Fetch todo items
- `/api/ha/calendars` - Fetch calendar events
- `/api/ha/quotes` - Fetch quotes
- `/api/ota/update` - Upload firmware via OTA
- `/api/health` - Health check endpoint

## Project Structure

```
web-app/
├── app/                    # Next.js app directory
│   ├── api/               # API routes
│   ├── settings/          # Settings page
│   ├── ota/               # OTA update page
│   └── page.tsx           # Main dashboard
├── components/            # React components
│   ├── Dashboard.tsx      # Main dashboard component
│   ├── Clock.tsx          # Clock display
│   ├── Weather.tsx        # Weather section
│   ├── TodoList.tsx       # Todo list
│   ├── CalendarEvents.tsx # Calendar events
│   ├── MiniCalendar.tsx   # Mini calendar
│   └── Quote.tsx          # Quote display
├── lib/                   # Utilities
│   ├── ha-client.ts       # Home Assistant client
│   ├── ota-client.ts      # OTA update client
│   └── types.ts           # TypeScript types
└── public/                # Static assets
```

## Development

```bash
# Install dependencies
npm install

# Run development server
npm run dev

# Build for production
npm run build

# Start production server
npm start

# Lint code
npm run lint
```

## Environment Variables

- `HA_HOST` - Home Assistant host/IP
- `HA_PORT` - Home Assistant port (default: 8123)
- `HA_TOKEN` - Home Assistant long-lived access token
- `OTA_DEVICE_IP` - ESP32 device IP address
- `OTA_PASSWORD` - OTA password (must match device)

## Troubleshooting

### Cannot fetch entities
- Verify Home Assistant host and port are correct
- Check that the access token is valid
- Ensure Home Assistant is accessible from the web app

### OTA update fails
- Verify device IP address is correct
- Check that OTA password matches device configuration
- Ensure device is powered on and connected to WiFi
- Check device serial output for error messages

### Dashboard not updating
- Check browser console for errors
- Verify API routes are accessible
- Ensure Home Assistant entities exist and are accessible

## License

ISC

