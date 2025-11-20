'use client';

import { useState } from 'react';
import { useRouter } from 'next/navigation';
import axios from 'axios';

export default function OTAPage() {
  const router = useRouter();
  const [file, setFile] = useState<File | null>(null);
  const [uploading, setUploading] = useState(false);
  const [progress, setProgress] = useState(0);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState(false);

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    if (e.target.files && e.target.files[0]) {
      setFile(e.target.files[0]);
      setError(null);
    }
  };

  const handleUpload = async () => {
    if (!file) {
      setError('Please select a firmware file');
      return;
    }

    const savedConfig = localStorage.getItem('epd47-config');
    if (!savedConfig) {
      setError('Please configure device settings first');
      return;
    }

    try {
      const config = JSON.parse(savedConfig);
      if (!config.ip || !config.otaPassword) {
        setError('Device IP and OTA password must be configured');
        return;
      }

      setUploading(true);
      setError(null);
      setSuccess(false);
      setProgress(0);

      const formData = new FormData();
      formData.append('firmware', file);
      formData.append('deviceIp', config.ip);
      formData.append('otaPassword', config.otaPassword);

      const response = await axios.post('/api/ota/update', formData, {
        headers: {
          'Content-Type': 'multipart/form-data',
        },
        onUploadProgress: (progressEvent) => {
          if (progressEvent.total) {
            const percentCompleted = Math.round(
              (progressEvent.loaded * 100) / progressEvent.total
            );
            setProgress(percentCompleted);
          }
        },
      });

      setSuccess(true);
      setTimeout(() => {
        router.push('/');
      }, 2000);
    } catch (err: any) {
      setError(err.response?.data?.error || err.message || 'OTA update failed');
    } finally {
      setUploading(false);
    }
  };

  return (
    <div className="min-h-screen bg-gray-100 p-8">
      <div className="max-w-2xl mx-auto bg-white rounded-lg shadow-lg p-8">
        <h1 className="text-3xl font-bold mb-6 text-epd-black">OTA Firmware Update</h1>

        {error && (
          <div className="mb-4 p-4 bg-red-100 border border-red-400 text-red-700 rounded">
            {error}
          </div>
        )}

        {success && (
          <div className="mb-4 p-4 bg-green-100 border border-green-400 text-green-700 rounded">
            Firmware uploaded successfully! The device will restart automatically.
          </div>
        )}

        <div className="space-y-6">
          <div>
            <label className="block text-sm font-medium mb-2 text-epd-black">
              Select Firmware File (.bin)
            </label>
            <input
              type="file"
              accept=".bin"
              onChange={handleFileChange}
              className="w-full px-3 py-2 border border-gray-300 rounded focus:outline-none focus:ring-2 focus:ring-epd-black"
              disabled={uploading}
            />
            {file && (
              <div className="mt-2 text-sm text-epd-gray">
                Selected: {file.name} ({(file.size / 1024 / 1024).toFixed(2)} MB)
              </div>
            )}
          </div>

          {uploading && (
            <div>
              <div className="w-full bg-gray-200 rounded-full h-2.5">
                <div
                  className="bg-epd-black h-2.5 rounded-full transition-all duration-300"
                  style={{ width: `${progress}%` }}
                ></div>
              </div>
              <div className="text-sm text-epd-gray mt-2">{progress}% uploaded</div>
            </div>
          )}

          <div className="flex gap-4">
            <button
              onClick={handleUpload}
              disabled={!file || uploading}
              className="px-6 py-2 bg-epd-black text-white rounded hover:bg-epd-gray disabled:opacity-50"
            >
              {uploading ? 'Uploading...' : 'Upload Firmware'}
            </button>
            <button
              onClick={() => router.push('/')}
              className="px-6 py-2 bg-gray-300 text-epd-black rounded hover:bg-gray-400"
            >
              Cancel
            </button>
          </div>

          <div className="text-sm text-epd-gray mt-4">
            <p className="font-semibold mb-2">Note:</p>
            <ul className="list-disc list-inside space-y-1">
              <li>Make sure the device is powered on and connected to WiFi</li>
              <li>The device will restart automatically after the update</li>
              <li>Do not power off the device during the update</li>
              <li>The update may take several minutes depending on file size</li>
            </ul>
          </div>
        </div>
      </div>
    </div>
  );
}

