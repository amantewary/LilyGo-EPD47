import { AppConfig } from './types';

function splitEnvList(value: string | undefined): string[] {
  return value
    ? value
        .split(',')
        .map((item) => item.trim())
        .filter(Boolean)
    : [];
}

export const DEFAULT_CONFIG: AppConfig = {
  host: process.env.NEXT_PUBLIC_HA_HOST || '',
  port: parseInt(process.env.NEXT_PUBLIC_HA_PORT || '8123', 10),
  token: process.env.NEXT_PUBLIC_HA_TOKEN || '',
  weatherEntity: process.env.NEXT_PUBLIC_HA_WEATHER_ENTITY || '',
  quoteEntity: process.env.NEXT_PUBLIC_HA_QUOTE_ENTITY || '',
  todoEntities: splitEnvList(process.env.NEXT_PUBLIC_HA_TODO_ENTITIES),
  calendarEntities: splitEnvList(process.env.NEXT_PUBLIC_HA_CALENDAR_ENTITIES),
  ip: process.env.NEXT_PUBLIC_OTA_DEVICE_IP || '',
  otaPassword: process.env.NEXT_PUBLIC_OTA_PASSWORD || '',
};

export function mergeWithDefaultConfig(config: Partial<AppConfig>): AppConfig {
  return {
    ...DEFAULT_CONFIG,
    ...config,
    todoEntities: Array.isArray(config.todoEntities)
      ? config.todoEntities
      : DEFAULT_CONFIG.todoEntities,
    calendarEntities: Array.isArray(config.calendarEntities)
      ? config.calendarEntities
      : DEFAULT_CONFIG.calendarEntities,
  };
}
