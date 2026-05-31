import { NextRequest, NextResponse } from 'next/server';
import { HAClient } from '@/lib/ha-client';
import { logSafeError } from '@/lib/safe-error';

export const dynamic = 'force-dynamic';

async function fetchQuotes(params: {
  host: string;
  port: number;
  token: string;
  entityId: string;
}) {
  const { host, port, token, entityId } = params;

  if (!host || !token || !entityId) {
    return NextResponse.json(
      { error: 'Missing required parameters: host, token, and entity are required' },
      { status: 400 }
    );
  }

  const client = new HAClient({ host, port, token });
  const quotes = await client.fetchQuotes(entityId);

  return NextResponse.json(quotes);
}

export async function POST(request: NextRequest) {
  try {
    const body = await request.json();
    return fetchQuotes({
      host: body.host || process.env.HA_HOST || '',
      port: parseInt(body.port || process.env.HA_PORT || '8123', 10),
      token: body.token || process.env.HA_TOKEN || '',
      entityId: body.entity || '',
    });
  } catch (error: any) {
    logSafeError('Quotes API error:', error);
    return NextResponse.json(
      { error: error.message || 'Failed to fetch quotes' },
      { status: 500 }
    );
  }
}

export async function GET(request: NextRequest) {
  try {
    const searchParams = request.nextUrl.searchParams;
    return fetchQuotes({
      host: searchParams.get('host') || process.env.HA_HOST || '',
      port: parseInt(searchParams.get('port') || process.env.HA_PORT || '8123', 10),
      token: searchParams.get('token') || process.env.HA_TOKEN || '',
      entityId: searchParams.get('entity') || '',
    });
  } catch (error: any) {
    logSafeError('Quotes API error:', error);
    return NextResponse.json(
      { error: error.message || 'Failed to fetch quotes' },
      { status: 500 }
    );
  }
}
