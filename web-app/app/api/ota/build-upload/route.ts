import { NextRequest } from 'next/server';
import { spawn } from 'child_process';
import path from 'path';

export const runtime = 'nodejs';
export const dynamic = 'force-dynamic';

export async function POST(req: NextRequest) {
  const body = await req.json().catch(() => ({}));
  const deviceIp = body.deviceIp || process.env.OTA_DEVICE_IP || '';
  const otaPassword = body.otaPassword || process.env.OTA_PASSWORD || '';
  const envName = body.env || process.env.PIO_ENV || 'T5-ePaper-S3-OTA';
  const pioCmd = process.env.PIO_CMD || 'pio';

  if (!deviceIp) {
    return new Response('Missing deviceIp (set in settings or body)', { status: 400 });
  }

  const projectRoot = path.join(process.cwd(), '..'); // repo root, one level above web-app
  const args = ['run', '-e', envName, '-t', 'upload'];

  const stream = new ReadableStream({
    start(controller) {
      const encoder = new TextEncoder();
      const send = (text: string) => controller.enqueue(encoder.encode(text));

      send(`$ ${pioCmd} ${args.join(' ')}\n`);
      send(`cwd: ${projectRoot}\n\n`);

      const child = spawn(pioCmd, args, {
        cwd: projectRoot,
        env: {
          ...process.env,
          OTA_IP: deviceIp,
          OTA_PASSWORD: otaPassword,
        },
      });

      child.stdout.on('data', (data) => {
        send(data.toString());
      });

      child.stderr.on('data', (data) => {
        send(data.toString());
      });

      child.on('error', (err) => {
        send(`Process error: ${err.message}\n`);
        controller.close();
      });

      child.on('close', (code) => {
        send(`\nProcess exited with code ${code}\n`);
        controller.close();
      });
    },
  });

  return new Response(stream, {
    headers: {
      'Content-Type': 'text/plain; charset=utf-8',
      'Cache-Control': 'no-cache',
    },
  });
}
