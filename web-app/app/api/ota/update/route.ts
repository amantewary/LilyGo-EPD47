import { NextRequest, NextResponse } from 'next/server';
import axios from 'axios';
import FormData from 'form-data';

export async function POST(request: NextRequest) {
  try {
    const formData = await request.formData();
    const firmware = formData.get('firmware') as File;
    const deviceIp = formData.get('deviceIp') as string || process.env.OTA_DEVICE_IP || '';
    const otaPassword = formData.get('otaPassword') as string || process.env.OTA_PASSWORD || '';

    if (!firmware || !deviceIp || !otaPassword) {
      return NextResponse.json(
        { error: 'Missing required parameters' },
        { status: 400 }
      );
    }

    // Convert File to Buffer
    const firmwareBuffer = Buffer.from(await firmware.arrayBuffer());

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

