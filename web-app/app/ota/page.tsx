'use client';

import { useState } from 'react';
import { useRouter } from 'next/navigation';
import axios from 'axios';
import { DEFAULT_CONFIG, mergeWithDefaultConfig } from '@/lib/default-config';

export default function OTAPage() {
  const router = useRouter();
  const [file, setFile] = useState<File | null>(null);
  const [uploading, setUploading] = useState(false);
  const [progress, setProgress] = useState(0);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState(false);
  const [statusMessage, setStatusMessage] = useState<string | null>(null);
  const [buildLog, setBuildLog] = useState('');
  const [buildRunning, setBuildRunning] = useState(false);
  const [buildError, setBuildError] = useState<string | null>(null);
  const [buildSuccess, setBuildSuccess] = useState(false);

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    if (e.target.files && e.target.files[0]) {
      setFile(e.target.files[0]);
      setError(null);
    }
  };

  const appendLog = (text: string) => {
    setBuildLog((prev) => prev + text);
  };

  const handleBuildAndUpload = async () => {
    const savedConfig = localStorage.getItem('epd47-config');
    if (!savedConfig && (!DEFAULT_CONFIG.ip || !DEFAULT_CONFIG.otaPassword)) {
      setBuildError('Please configure device settings first');
      return;
    }

    try {
      const config = savedConfig
        ? mergeWithDefaultConfig(JSON.parse(savedConfig))
        : DEFAULT_CONFIG;
      if (!config.ip || !config.otaPassword) {
        setBuildError('Device IP and OTA password must be configured');
        return;
      }

      setBuildRunning(true);
      setBuildError(null);
      setBuildSuccess(false);
      setBuildLog('');

      const response = await fetch('/api/ota/build-upload', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          deviceIp: config.ip,
          otaPassword: config.otaPassword,
        }),
      });

      if (!response.body) {
        throw new Error('No response stream from server');
      }
      if (!response.ok) {
        const text = await response.text();
        throw new Error(text || 'Build/Upload failed');
      }

      const reader = response.body.getReader();
      const decoder = new TextDecoder();
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        if (value) {
          appendLog(decoder.decode(value));
        }
      }

      setBuildSuccess(true);
    } catch (err: any) {
      setBuildError(err.message || 'Build/Upload failed');
    } finally {
      setBuildRunning(false);
    }
  };

  const handleUpload = async () => {
    const savedConfig = localStorage.getItem('epd47-config');
    if (!savedConfig && (!DEFAULT_CONFIG.ip || !DEFAULT_CONFIG.otaPassword)) {
      setError('Please configure device settings first');
      return;
    }

    try {
      const config = savedConfig
        ? mergeWithDefaultConfig(JSON.parse(savedConfig))
        : DEFAULT_CONFIG;
      if (!config.ip || !config.otaPassword) {
        setError('Device IP and OTA password must be configured');
        return;
      }

      setUploading(true);
      setError(null);
      setSuccess(false);
      setProgress(0);
      setStatusMessage(null);

      const formData = new FormData();
      if (file) {
        formData.append('firmware', file);
      }
      formData.append('deviceIp', config.ip);
      formData.append('otaPassword', config.otaPassword);
      if (!file) {
        formData.append('useBundled', 'true');
      }

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

      setStatusMessage('Firmware sent. Device will restart after flashing.');
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
            {statusMessage && <div className="text-sm mt-1">{statusMessage}</div>}
          </div>
        )}

        <div className="space-y-6">
          <div className="flex flex-col gap-3">
            <div>
              <label className="block text-sm font-medium mb-2 text-epd-black">
                Select Firmware File (.bin) — optional if using bundled firmware
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
            <div className="text-sm text-epd-gray">
              If you skip selecting a file, the app will push the bundled demo firmware from the server.
            </div>
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
              disabled={uploading}
              className="px-6 py-2 bg-epd-black text-white rounded hover:bg-epd-gray disabled:opacity-50"
            >
              {uploading ? 'Uploading...' : 'Update Now'}
            </button>
            <button
              onClick={() => router.push('/')}
              className="px-6 py-2 bg-gray-300 text-epd-black rounded hover:bg-gray-400"
            >
              Cancel
            </button>
          </div>

          <div className="border-t border-gray-200 pt-6 mt-4">
            <div className="flex items-center justify-between mb-3">
              <h2 className="text-lg font-semibold text-epd-black">Build & Upload (PlatformIO)</h2>
              <span className="text-xs text-epd-gray">Requires PlatformIO on the server</span>
            </div>

            {buildError && (
              <div className="mb-3 p-3 bg-red-100 border border-red-400 text-red-700 rounded">
                {buildError}
              </div>
            )}
            {buildSuccess && (
              <div className="mb-3 p-3 bg-green-100 border border-green-400 text-green-700 rounded">
                Build & upload completed.
              </div>
            )}

            <div className="flex gap-3 mb-3">
              <button
                onClick={handleBuildAndUpload}
                disabled={buildRunning}
                className="px-5 py-2 bg-epd-black text-white rounded hover:bg-epd-gray disabled:opacity-50"
              >
                {buildRunning ? 'Running…' : 'Build & Upload'}
              </button>
              <button
                onClick={() => {
                  setBuildLog('');
                  setBuildError(null);
                  setBuildSuccess(false);
                }}
                className="px-5 py-2 bg-gray-200 text-epd-black rounded hover:bg-gray-300"
              >
                Clear Log
              </button>
            </div>

            <div className="bg-black text-green-200 rounded-md p-3 h-60 overflow-auto text-xs font-mono border border-gray-800">
              {buildLog.trim().length === 0 ? (
                <div className="text-gray-400">Logs will appear here when you run Build & Upload.</div>
              ) : (
                <pre className="whitespace-pre-wrap break-words">{buildLog}</pre>
              )}
            </div>
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
