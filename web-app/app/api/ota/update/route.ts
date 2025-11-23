import { NextRequest, NextResponse } from 'next/server';
import axios from 'axios';
import FormData from 'form-data';
import fs from 'fs';
import path from 'path';

export async function POST(request: NextRequest) {
  try {
    const formData = await request.formData();
    const firmware = formData.get('firmware') as File | null;
    const useBundled = formData.get('useBundled') === 'true';
    const deviceIp = (formData.get('deviceIp') as string) || process.env.OTA_DEVICE_IP || '';
    const otaPassword = (formData.get('otaPassword') as string) || process.env.OTA_PASSWORD || '';

    if ((!firmware && !useBundled) || !deviceIp || !otaPassword) {
      return NextResponse.json(
        { error: 'Missing required parameters' },
        { status: 400 }
      );
    }

    // Resolve firmware buffer either from uploaded file or bundled binary on disk
    let firmwareBuffer: Buffer;
    if (firmware) {
      firmwareBuffer = Buffer.from(await firmware.arrayBuffer());
    } else {
      // Default to bundled demo firmware in the repo (one level up from web-app)
      const defaultFirmwarePath =
        process.env.DEFAULT_FIRMWARE_PATH ||
        path.join(process.cwd(), '..', 'firmware', 'T5-ePaper-S3_demo_250901.bin');

      if (!fs.existsSync(defaultFirmwarePath)) {
        return NextResponse.json(
          { error: 'Bundled firmware not found on server. Provide a .bin file instead.' },
          { status: 500 }
        );
      }

      firmwareBuffer = fs.readFileSync(defaultFirmwarePath);
    }

    // Create form data for ArduinoOTA
    const uploadFormData = new FormData();
    uploadFormData.append('firmware', firmwareBuffer, {
      filename: 'firmware.bin',
      contentType: 'application/octet-stream',
    });

    // Upload to device using ArduinoOTA endpoint
    // ArduinoOTA uses HTTP Basic Auth with password
    const response = await axios.post(
      `http://${deviceIp}/update`,
      uploadFormData,
      {
        headers: {
          ...uploadFormData.getHeaders(),
        },
        auth: {
          username: 'admin',
          password: otaPassword,
        },
        timeout: 300000, // 5 minutes
        maxContentLength: Infinity,
        maxBodyLength: Infinity,
      }
    );

    if (response.status !== 200) {
      throw new Error(`OTA update failed with status ${response.status}`);
    }

    return NextResponse.json({ success: true, message: 'OTA update completed' });
  } catch (error: any) {
    console.error('OTA update error:', error);
    return NextResponse.json(
      { error: error.response?.data?.message || error.message || 'OTA update failed' },
      { status: 500 }
    );
  }
}
