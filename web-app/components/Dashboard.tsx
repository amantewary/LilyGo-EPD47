'use client';

import { useState, useEffect } from 'react';
import { format } from 'date-fns';
import { DashboardData, AppConfig } from '@/lib/types';
import Clock from './Clock';
import Weather from './Weather';
import TodoList from './TodoList';
import CalendarEvents from './CalendarEvents';
import MiniCalendar from './MiniCalendar';
import Quote from './Quote';

interface DashboardProps {
  config: AppConfig;
}

export default function Dashboard({ config }: DashboardProps) {
  const [data, setData] = useState<DashboardData | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [currentDate, setCurrentDate] = useState(new Date());

  const fetchData = async () => {
    try {
      setLoading(true);
      setError(null);

      // Validate config
      if (!config.host || !config.token) {
        throw new Error('Home Assistant configuration is missing. Please check your settings.');
      }

      if (!config.weatherEntity || !config.quoteEntity) {
        throw new Error('Required entities are not configured. Please select weather and quote entities in settings.');
      }

      const baseUrl = '/api/ha';
      const params = new URLSearchParams({
        host: config.host,
        port: config.port.toString(),
        token: config.token,
      });

      // Build fetch promises - handle empty arrays
      const fetchPromises: Promise<Response>[] = [
        fetch(`${baseUrl}/weather?${params}&entity=${config.weatherEntity}`),
      ];

      if (config.todoEntities && config.todoEntities.length > 0) {
        fetchPromises.push(
          fetch(`${baseUrl}/todos?${params}&entities=${config.todoEntities.join(',')}`)
        );
      } else {
        // Return empty array if no todos configured
        fetchPromises.push(Promise.resolve(new Response(JSON.stringify([]), { status: 200 })));
      }

      if (config.calendarEntities && config.calendarEntities.length > 0) {
        fetchPromises.push(
          fetch(`${baseUrl}/calendars?${params}&entities=${config.calendarEntities.join(',')}`)
        );
      } else {
        // Return empty array if no calendars configured
        fetchPromises.push(Promise.resolve(new Response(JSON.stringify([]), { status: 200 })));
      }

      fetchPromises.push(
        fetch(`${baseUrl}/quotes?${params}&entity=${config.quoteEntity}`)
      );

      const [weatherRes, todosRes, calendarsRes, quotesRes] = await Promise.all(fetchPromises);

      // Check for errors
      if (!weatherRes.ok) {
        const errorData = await weatherRes.json().catch(() => ({}));
        throw new Error(errorData.error || `Weather API error: ${weatherRes.status}`);
      }
      if (!todosRes.ok) {
        const errorData = await todosRes.json().catch(() => ({}));
        throw new Error(errorData.error || `Todos API error: ${todosRes.status}`);
      }
      if (!calendarsRes.ok) {
        const errorData = await calendarsRes.json().catch(() => ({}));
        throw new Error(errorData.error || `Calendars API error: ${calendarsRes.status}`);
      }
      if (!quotesRes.ok) {
        const errorData = await quotesRes.json().catch(() => ({}));
        throw new Error(errorData.error || `Quotes API error: ${quotesRes.status}`);
      }

      const [weather, todos, calendarEvents, quotes] = await Promise.all([
        weatherRes.json(),
        todosRes.json(),
        calendarsRes.json(),
        quotesRes.json(),
      ]);

      setData({
        weather,
        todos: Array.isArray(todos) ? todos : [],
        calendarEvents: Array.isArray(calendarEvents) ? calendarEvents : [],
        quotes: Array.isArray(quotes) ? quotes : [],
        currentQuoteIndex: 0,
      });
    } catch (err: any) {
      console.error('Dashboard fetch error:', err);
      setError(err.message || 'Failed to fetch data. Check browser console for details.');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchData();
    
    // Refresh weather, todos, calendars every hour
    const hourlyInterval = setInterval(() => {
      fetchData();
    }, 60 * 60 * 1000);

    // Update date every minute
    const dateInterval = setInterval(() => {
      setCurrentDate(new Date());
    }, 60 * 1000);

    return () => {
      clearInterval(hourlyInterval);
      clearInterval(dateInterval);
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [config]);

  // Rotate quotes every 3 hours
  useEffect(() => {
    if (!data || data.quotes.length === 0) return;

    const rotateInterval = setInterval(() => {
      setData(prev => {
        if (!prev) return prev;
        const nextIndex = (prev.currentQuoteIndex + 1) % prev.quotes.length;
        return { ...prev, currentQuoteIndex: nextIndex };
      });
    }, 3 * 60 * 60 * 1000);

    return () => clearInterval(rotateInterval);
  }, [data]);

  if (loading) {
    return (
      <div className="w-[960px] h-[540px] bg-epd-white flex items-center justify-center">
        <div className="text-epd-black">Loading...</div>
      </div>
    );
  }

  if (error) {
    return (
      <div className="w-[960px] h-[540px] bg-epd-white flex flex-col items-center justify-center p-8">
        <div className="text-epd-black text-lg font-semibold mb-2">Error Loading Dashboard</div>
        <div className="text-epd-black text-sm mb-4 text-center">{error}</div>
        <button
          onClick={() => fetchData()}
          className="px-4 py-2 bg-epd-black text-epd-white rounded hover:bg-epd-gray text-sm"
        >
          Retry
        </button>
      </div>
    );
  }

  if (!data) {
    return null;
  }

  return (
    <div className="w-[960px] h-[540px] bg-epd-white p-5 flex flex-col relative overflow-hidden">
      {/* Top Header */}
      <div className="flex justify-between items-center mb-4">
        <div className="flex items-center gap-4">
          <Clock />
          <div className="w-4 h-4 rounded-full bg-epd-black"></div>
        </div>
        <Weather weather={data.weather} />
        <div className="text-lg font-medium text-epd-black">
          {format(currentDate, 'MMM d')}
        </div>
      </div>

      {/* Quote Section */}
      <div className="mb-4 pb-2 border-b border-epd-gray">
        <Quote quotes={data.quotes} currentIndex={data.currentQuoteIndex} />
      </div>

      {/* Middle Section - Todo and Calendar */}
      <div className="flex gap-4 flex-1 mb-4">
        <div className="w-[450px]">
          <TodoList todos={data.todos} />
        </div>
        <div className="w-[450px]">
          <CalendarEvents events={data.calendarEvents} />
        </div>
      </div>

      {/* Divider */}
      <div className="border-t border-epd-gray mb-2"></div>

      {/* Mini Calendar */}
      <div className="h-[45px]">
        <MiniCalendar />
      </div>
    </div>
  );
}

