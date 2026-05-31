'use client';

import { useState, useEffect } from 'react';
import Dashboard from '@/components/Dashboard';
import { AppConfig } from '@/lib/types';
import { DEFAULT_CONFIG, mergeWithDefaultConfig } from '@/lib/default-config';
import Link from 'next/link';

export default function Home() {
  const [config, setConfig] = useState<AppConfig>(DEFAULT_CONFIG);
  const [isConfigured, setIsConfigured] = useState(false);

  useEffect(() => {
    // Load config from localStorage
    const savedConfig = localStorage.getItem('epd47-config');
    if (savedConfig) {
      try {
        const parsed = JSON.parse(savedConfig);
        const mergedConfig = mergeWithDefaultConfig(parsed);
        setConfig(mergedConfig);
        setIsConfigured(true);
      } catch (e) {
        console.error('Failed to parse saved config:', e);
      }
    } else {
      // Check if env vars are set
      if (DEFAULT_CONFIG.host && DEFAULT_CONFIG.token) {
        setIsConfigured(true);
      }
    }
  }, []);

  if (!isConfigured) {
    return (
      <div className="min-h-screen bg-epd-white flex items-center justify-center">
        <div className="text-center">
          <h1 className="text-2xl font-bold mb-4 text-epd-black">EPD47 Dashboard</h1>
          <p className="text-epd-gray mb-4">Please configure your settings first.</p>
          <Link
            href="/settings"
            className="inline-block px-4 py-2 bg-epd-black text-epd-white rounded hover:bg-epd-gray"
          >
            Go to Settings
          </Link>
        </div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gray-100 flex flex-col items-center p-4">
      <div className="mb-4 flex gap-4 items-center">
        <Link
          href="/settings"
          className="px-4 py-2 bg-epd-black text-epd-white rounded hover:bg-epd-gray text-sm"
        >
          Settings
        </Link>
        <Link
          href="/ota"
          className="px-4 py-2 bg-epd-black text-epd-white rounded hover:bg-epd-gray text-sm"
        >
          OTA Update
        </Link>
        <button
          onClick={() => {
            const savedConfig = localStorage.getItem('epd47-config');
            if (savedConfig) {
              try {
                const parsed = JSON.parse(savedConfig);
                const mergedConfig = mergeWithDefaultConfig(parsed);
                setConfig(mergedConfig);
              } catch (e) {
                console.error('Failed to reload config:', e);
              }
            }
          }}
          className="px-4 py-2 bg-gray-500 text-white rounded hover:bg-gray-600 text-sm"
        >
          Reload Config
        </button>
      </div>
      <div className="bg-epd-white overflow-hidden">
        <Dashboard config={config} />
      </div>
    </div>
  );
}
