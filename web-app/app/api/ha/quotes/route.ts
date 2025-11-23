import { NextRequest, NextResponse } from 'next/server';
import { HAClient } from '@/lib/ha-client';
import { HAConfig } from '@/lib/types';

export const dynamic = 'force-dynamic';

export async function GET(request: NextRequest) {
  try {
    const searchParams = request.nextUrl.searchParams;
    const host = searchParams.get('host') || process.env.HA_HOST || '';
    const port = parseInt(searchParams.get('port') || process.env.HA_PORT || '8123');
    const token = searchParams.get('token') || process.env.HA_TOKEN || '';
    const entityId = searchParams.get('entity') || '';

    if (!host || !token || !entityId) {
      return NextResponse.json(
        { error: 'Missing required parameters: host, token, and entity are required' },
        { status: 400 }
      );
    }

    const client = new HAClient({ host, port, token });
    const quotes = await client.fetchQuotes(entityId);

    return NextResponse.json(quotes);
  } catch (error: any) {
    console.error('Quotes API error:', error);
    return NextResponse.json(
      { error: error.message || 'Failed to fetch quotes' },
      { status: 500 }
    );
  }
}
