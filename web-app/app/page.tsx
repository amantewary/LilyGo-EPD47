'use client';

import { useState, useEffect } from 'react';
import Dashboard from '@/components/Dashboard';
import { AppConfig } from '@/lib/types';
import Link from 'next/link';

const DEFAULT_CONFIG: AppConfig = {
  host: process.env.NEXT_PUBLIC_HA_HOST || '',
  port: parseInt(process.env.NEXT_PUBLIC_HA_PORT || '8123'),
  token: process.env.NEXT_PUBLIC_HA_TOKEN || '',
  weatherEntity: 'weather.toronto_forecast',
  quoteEntity: 'sensor.quote_of_the_day',
  todoEntities: ['todo.errands', 'todo.work', 'todo.personal'],
  calendarEntities: ['calendar.aman_outlook_calendar', 'calendar.home_2'],
  ip: process.env.NEXT_PUBLIC_OTA_DEVICE_IP || '',
  otaPassword: process.env.NEXT_PUBLIC_OTA_PASSWORD || 'epd47ota',
};

export default function Home() {
  const [config, setConfig] = useState<AppConfig>(DEFAULT_CONFIG);
  const [isConfigured, setIsConfigured] = useState(false);

  useEffect(() => {
    // Load config from localStorage
    const savedConfig = localStorage.getItem('epd47-config');
    if (savedConfig) {
      try {
        const parsed = JSON.parse(savedConfig);
        const mergedConfig = { ...DEFAULT_CONFIG, ...parsed };
        // Ensure arrays are arrays
        if (!Array.isArray(mergedConfig.todoEntities)) {
          mergedConfig.todoEntities = [];
        }
        if (!Array.isArray(mergedConfig.calendarEntities)) {
          mergedConfig.calendarEntities = [];
        }
        setConfig(mergedConfig);
        setIsConfigured(true);
        console.log('Loaded config from localStorage:', mergedConfig);
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
                const mergedConfig = { ...DEFAULT_CONFIG, ...parsed };
                if (!Array.isArray(mergedConfig.todoEntities)) {
                  mergedConfig.todoEntities = [];
                }
                if (!Array.isArray(mergedConfig.calendarEntities)) {
                  mergedConfig.calendarEntities = [];
                }
                setConfig(mergedConfig);
                console.log('Reloaded config:', mergedConfig);
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
      <div className="bg-epd-white shadow-lg rounded-lg overflow-hidden">
        <Dashboard config={config} />
      </div>
    </div>
  );
}

