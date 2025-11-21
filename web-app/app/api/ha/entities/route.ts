import { NextRequest, NextResponse } from 'next/server';
import axios from 'axios';

export async function POST(request: NextRequest) {
  try {
    const body = await request.json();
    const host = body.host || process.env.HA_HOST || '';
    const port = parseInt(body.port || process.env.HA_PORT || '8123');
    const token = body.token || process.env.HA_TOKEN || '';

    if (!host || !token) {
      return NextResponse.json(
        { error: 'Missing required parameters: host and token are required' },
        { status: 400 }
      );
    }

    // Fetch all states from Home Assistant
    const response = await axios.get(
      `http://${host}:${port}/api/states`,
      {
        headers: {
          Authorization: `Bearer ${token}`,
        },
        timeout: 10000,
      }
    );

    const entities = response.data || [];
    const todos: string[] = [];
    const calendars: string[] = [];
    const weather: string[] = [];
    const sensors: string[] = [];

    entities.forEach((entity: any) => {
      const entityId = entity.entity_id;
      if (entityId.startsWith('todo.')) {
        todos.push(entityId);
      } else if (entityId.startsWith('calendar.')) {
        calendars.push(entityId);
      } else if (entityId.startsWith('weather.')) {
        weather.push(entityId);
      } else if (entityId.startsWith('sensor.')) {
        sensors.push(entityId);
      }
    });

    return NextResponse.json({
      todos: todos.sort(),
      calendars: calendars.sort(),
      weather: weather.sort(),
      sensors: sensors.sort(),
    });
  } catch (error: any) {
    console.error('Entities API error:', error);
    if (error.response) {
      return NextResponse.json(
        { error: `Home Assistant API error: ${error.response.status} ${error.response.statusText}` },
        { status: error.response.status || 500 }
      );
    }
    return NextResponse.json(
      { error: error.message || 'Failed to fetch entities' },
      { status: 500 }
    );
  }
}

