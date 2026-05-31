import axios, { AxiosInstance } from 'axios';
import { WeatherData, TodoItem, CalendarEvent, QuoteData, HAConfig, EntityConfig } from './types';
import { logSafeError } from './safe-error';

function getLocalDateString(date: Date): string {
  const year = date.getFullYear();
  const month = String(date.getMonth() + 1).padStart(2, '0');
  const day = String(date.getDate()).padStart(2, '0');
  return `${year}-${month}-${day}`;
}

export class HAClient {
  private client: AxiosInstance;
  private config: HAConfig;

  constructor(config: HAConfig) {
    this.config = config;
    this.client = axios.create({
      baseURL: `http://${config.host}:${config.port}`,
      headers: {
        'Authorization': `Bearer ${config.token}`,
        'Content-Type': 'application/json',
      },
      timeout: 10000,
    });
  }

  async fetchWeather(entityId: string): Promise<WeatherData> {
    try {
      const response = await this.client.get(`/api/states/${entityId}`);
      const state = response.data;
      
      return {
        temperature: state.attributes.temperature 
          ? `${state.attributes.temperature.toFixed(1)} C`
          : '-- C',
        condition: state.state || '--',
        icon: state.attributes.icon || '',
      };
    } catch (error) {
      logSafeError('Error fetching weather:', error);
      return {
        temperature: '-- C',
        condition: '--',
      };
    }
  }

  async fetchTodos(entityIds: string[]): Promise<TodoItem[]> {
    const todos: TodoItem[] = [];
    const today = getLocalDateString(new Date());

    for (const entityId of entityIds) {
      try {
        const stateResponse = await this.client.get(`/api/states/${entityId}`);
        const state = stateResponse.data;

        if (
          state.state === 'unavailable' ||
          state.state === 'unknown' ||
          state.attributes?.restored
        ) {
          console.warn(`Skipping unavailable todo entity: ${entityId}`);
          continue;
        }

        const response = await this.client.post(
          '/api/services/todo/get_items?return_response=true',
          {
            entity_id: entityId,
            status: 'needs_action',
          }
        );

        const items = response.data.service_response?.[entityId]?.items || [];
        
        for (const item of items) {
          if (item.status === 'completed' || !item.due) {
            continue;
          }

          const dueDate = item.due.substring(0, 10);
          if (dueDate.length < 10 || dueDate > today) {
            continue;
          }

          todos.push({
            text: item.summary || '',
            completed: false,
            due: dueDate,
          });
        }
      } catch (error) {
        logSafeError(`Error fetching todos from ${entityId}:`, error);
      }
    }

    todos.sort((a, b) => (a.due || '').localeCompare(b.due || ''));
    return todos.slice(0, 8);
  }

  async fetchCalendarEvents(entityIds: string[], daysAhead: number = 7): Promise<CalendarEvent[]> {
    const events: CalendarEvent[] = [];
    const now = new Date();
    const endDate = new Date(now.getTime() + daysAhead * 24 * 60 * 60 * 1000);

    const startStr = now.toISOString().replace(/:/g, '%3A');
    const endStr = endDate.toISOString().replace(/:/g, '%3A');

    for (const entityId of entityIds) {
      try {
        const response = await this.client.get(
          `/api/calendars/${entityId}?start=${startStr}&end=${endStr}`
        );

        const calendarEvents = Array.isArray(response.data) ? response.data : [];
        
        for (const event of calendarEvents) {
          const startDateTime = event.start?.dateTime || event.start?.date;
          if (!startDateTime) continue;

          const eventDate = new Date(startDateTime);
          const dateStr = `${String(eventDate.getMonth() + 1).padStart(2, '0')}-${String(eventDate.getDate()).padStart(2, '0')}`;
          const timeStr = event.start?.dateTime 
            ? `${String(eventDate.getHours()).padStart(2, '0')}:${String(eventDate.getMinutes()).padStart(2, '0')}`
            : 'All Day';

          events.push({
            title: event.summary || '',
            startTime: timeStr,
            date: dateStr,
            isoDateTime: startDateTime,
          });
        }
      } catch (error) {
        logSafeError(`Error fetching calendar from ${entityId}:`, error);
      }
    }

    // Sort by ISO datetime and limit to 10 events
    events.sort((a, b) => a.isoDateTime.localeCompare(b.isoDateTime));
    return events.slice(0, 10);
  }

  async fetchQuotes(entityId: string): Promise<QuoteData[]> {
    try {
      const response = await this.client.get(`/api/states/${entityId}`);
      const entries = response.data.attributes?.entries || [];

      const quotes: QuoteData[] = [];
      for (const entry of entries) {
        let text = entry.summary || '';
        // Remove surrounding quotes if present
        text = text.trim();
        if (text.startsWith('"') && text.endsWith('"')) {
          text = text.substring(1, text.length - 1);
        }
        text = text.replace(/\\"/g, '"');

        if (text.length > 0) {
          quotes.push({
            author: entry.title || 'Unknown',
            text: text,
          });
        }
      }

      return quotes;
    } catch (error) {
      logSafeError('Error fetching quotes:', error);
      return [];
    }
  }

  async fetchAllData(entityConfig: EntityConfig): Promise<{
    weather: WeatherData;
    todos: TodoItem[];
    calendarEvents: CalendarEvent[];
    quotes: QuoteData[];
  }> {
    const [weather, todos, calendarEvents, quotes] = await Promise.all([
      this.fetchWeather(entityConfig.weatherEntity),
      this.fetchTodos(entityConfig.todoEntities),
      this.fetchCalendarEvents(entityConfig.calendarEntities),
      this.fetchQuotes(entityConfig.quoteEntity),
    ]);

    return {
      weather,
      todos,
      calendarEvents,
      quotes,
    };
  }
}
