import { NextRequest, NextResponse } from 'next/server';
import { HAClient } from '@/lib/ha-client';
import { logSafeError } from '@/lib/safe-error';

export const dynamic = 'force-dynamic';

async function fetchTodos(params: {
  host: string;
  port: number;
  token: string;
  entityIds: string[];
}) {
  const { host, port, token, entityIds } = params;

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
  const todos = await client.fetchTodos(entityIds);

  return NextResponse.json(todos);
}

export async function POST(request: NextRequest) {
  try {
    const body = await request.json();
    return fetchTodos({
      host: body.host || process.env.HA_HOST || '',
      port: parseInt(body.port || process.env.HA_PORT || '8123', 10),
      token: body.token || process.env.HA_TOKEN || '',
      entityIds: Array.isArray(body.entities) ? body.entities : [],
    });
  } catch (error: any) {
    logSafeError('Todos API error:', error);
    return NextResponse.json(
      { error: error.message || 'Failed to fetch todos' },
      { status: 500 }
    );
  }
}

export async function GET(request: NextRequest) {
  try {
    const searchParams = request.nextUrl.searchParams;
    const entitiesParam = searchParams.get('entities');
    const entityIds = entitiesParam ? entitiesParam.split(',').filter(e => e.trim()) : [];

    return fetchTodos({
      host: searchParams.get('host') || process.env.HA_HOST || '',
      port: parseInt(searchParams.get('port') || process.env.HA_PORT || '8123', 10),
      token: searchParams.get('token') || process.env.HA_TOKEN || '',
      entityIds,
    });
  } catch (error: any) {
    logSafeError('Todos API error:', error);
    return NextResponse.json(
      { error: error.message || 'Failed to fetch todos' },
      { status: 500 }
    );
  }
}
