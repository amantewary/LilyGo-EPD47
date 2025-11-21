import { NextRequest, NextResponse } from 'next/server';
import { HAClient } from '@/lib/ha-client';
import { HAConfig } from '@/lib/types';

export async function GET(request: NextRequest) {
  try {
    const searchParams = request.nextUrl.searchParams;
    const host = searchParams.get('host') || process.env.HA_HOST || '';
    const port = parseInt(searchParams.get('port') || process.env.HA_PORT || '8123');
    const token = searchParams.get('token') || process.env.HA_TOKEN || '';
    const entitiesParam = searchParams.get('entities');
    const entityIds = entitiesParam ? entitiesParam.split(',').filter(e => e.trim()) : [];
    const daysAhead = parseInt(searchParams.get('days') || '7');

    if (!host || !token) {
      return NextResponse.json(
        { error: 'Missing required parameters: host and token are required' },
        { status: 400 }
      );
    }

    if (entityIds.length === 0) {
      // Return empty array if no entities provided
      return NextResponse.json([]);
    }

    const client = new HAClient({ host, port, token });
    const events = await client.fetchCalendarEvents(entityIds, daysAhead);

    return NextResponse.json(events);
  } catch (error: any) {
    console.error('Calendars API error:', error);
    return NextResponse.json(
      { error: error.message || 'Failed to fetch calendar events' },
      { status: 500 }
    );
  }
}

