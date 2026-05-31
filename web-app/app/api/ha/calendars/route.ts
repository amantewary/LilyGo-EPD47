import { NextRequest, NextResponse } from 'next/server';
import { HAClient } from '@/lib/ha-client';
import { logSafeError } from '@/lib/safe-error';

export const dynamic = 'force-dynamic';

async function fetchCalendars(params: {
  host: string;
  port: number;
  token: string;
  entityIds: string[];
  daysAhead: number;
}) {
  const { host, port, token, entityIds, daysAhead } = params;

  if (!host || !token) {
    return NextResponse.json(
      { error: 'Missing required parameters: host and token are required' },
      { status: 400 }
    );
  }

  if (entityIds.length === 0) {
    return NextResponse.json([]);
  }

  const client = new HAClient({ host, port, token });
  const events = await client.fetchCalendarEvents(entityIds, daysAhead);

  return NextResponse.json(events);
}

export async function POST(request: NextRequest) {
  try {
    const body = await request.json();
    return fetchCalendars({
      host: body.host || process.env.HA_HOST || '',
      port: parseInt(body.port || process.env.HA_PORT || '8123', 10),
      token: body.token || process.env.HA_TOKEN || '',
      entityIds: Array.isArray(body.entities) ? body.entities : [],
      daysAhead: parseInt(body.days || '7', 10),
    });
  } catch (error: any) {
    logSafeError('Calendars API error:', error);
    return NextResponse.json(
      { error: error.message || 'Failed to fetch calendar events' },
      { status: 500 }
    );
  }
}

export async function GET(request: NextRequest) {
  try {
    const searchParams = request.nextUrl.searchParams;
    const entitiesParam = searchParams.get('entities');
    const entityIds = entitiesParam ? entitiesParam.split(',').filter(e => e.trim()) : [];

    return fetchCalendars({
      host: searchParams.get('host') || process.env.HA_HOST || '',
      port: parseInt(searchParams.get('port') || process.env.HA_PORT || '8123', 10),
      token: searchParams.get('token') || process.env.HA_TOKEN || '',
      entityIds,
      daysAhead: parseInt(searchParams.get('days') || '7', 10),
    });
  } catch (error: any) {
    logSafeError('Calendars API error:', error);
    return NextResponse.json(
      { error: error.message || 'Failed to fetch calendar events' },
      { status: 500 }
    );
  }
}
