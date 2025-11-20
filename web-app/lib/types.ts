// Type definitions for the EPD47 Dashboard

export interface WeatherData {
  temperature: string;
  condition: string;
  icon?: string;
}

export interface TodoItem {
  text: string;
  completed: boolean;
  due?: string;
}

export interface CalendarEvent {
  title: string;
  startTime: string; // HH:MM format
  date: string; // MM-DD format
  isoDateTime: string; // ISO datetime for sorting
}

export interface QuoteData {
  author: string;
  text: string;
}

export interface DashboardData {
  weather: WeatherData;
  todos: TodoItem[];
  calendarEvents: CalendarEvent[];
  quotes: QuoteData[];
  currentQuoteIndex: number;
}

export interface HAConfig {
  host: string;
  port: number;
  token: string;
}

export interface EntityConfig {
  weatherEntity: string;
  quoteEntity: string;
  todoEntities: string[];
  calendarEntities: string[];
}

export interface DeviceConfig {
  ip: string;
  otaPassword: string;
}

export interface AppConfig extends HAConfig, EntityConfig, DeviceConfig {}

