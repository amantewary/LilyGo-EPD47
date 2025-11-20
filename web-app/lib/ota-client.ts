import axios from 'axios';
import { DeviceConfig } from './types';

export class OTAClient {
  private config: DeviceConfig;

  constructor(config: DeviceConfig) {
    this.config = config;
  }

  async uploadFirmware(firmwareBinary: Buffer, onProgress?: (progress: number) => void): Promise<void> {
    // ArduinoOTA uses /update endpoint with password in header
    // Format: multipart/form-data with password header
    const FormData = require('form-data');
    const formData = new FormData();
    
    formData.append('firmware', firmwareBinary, {
      filename: 'firmware.bin',
      contentType: 'application/octet-stream',
    });

    try {
      const response = await axios.post(
        `http://${this.config.ip}/update`,
        formData,
        {
          headers: {
            ...formData.getHeaders(),
            'X-OTA-Password': this.config.otaPassword,
          },
          timeout: 300000, // 5 minutes timeout
          maxContentLength: Infinity,
          maxBodyLength: Infinity,
          onUploadProgress: (progressEvent) => {
            if (progressEvent.total && onProgress) {
              const progress = Math.round((progressEvent.loaded * 100) / progressEvent.total);
              onProgress(progress);
            }
          },
        }
      );

      if (response.status !== 200) {
        throw new Error(`OTA update failed with status ${response.status}`);
      }
    } catch (error: any) {
      if (error.response) {
        throw new Error(`OTA update failed: ${error.response.statusText}`);
      }
      throw new Error(`OTA update failed: ${error.message}`);
    }
  }

  async checkDeviceStatus(): Promise<boolean> {
    try {
      // Try to ping the device or check if it's reachable
      const response = await axios.get(`http://${this.config.ip}/`, {
        timeout: 5000,
      });
      return response.status === 200 || response.status === 404; // 404 is OK, device is reachable
    } catch (error) {
      return false;
    }
  }
}

