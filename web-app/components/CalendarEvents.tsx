'use client';

import { CalendarEvent } from '@/lib/types';
import { useState, useEffect } from 'react';

interface CalendarEventsProps {
  events: CalendarEvent[];
}

export default function CalendarEvents({ events }: CalendarEventsProps) {
  const [minutesUntilNext, setMinutesUntilNext] = useState<number | null>(null);

  useEffect(() => {
    if (events.length === 0) return;

    const updateCountdown = () => {
      const now = new Date();
      const nextEvent = events.find(evt => {
        const eventDate = new Date(evt.isoDateTime);
        return eventDate > now;
      });

      if (nextEvent) {
        const eventDate = new Date(nextEvent.isoDateTime);
        const diffMs = eventDate.getTime() - now.getTime();
        const diffMins = Math.floor(diffMs / 60000);
        setMinutesUntilNext(diffMins);
      } else {
        setMinutesUntilNext(null);
      }
    };

    updateCountdown();
    const interval = setInterval(updateCountdown, 60000); // Update every minute

    return () => clearInterval(interval);
  }, [events]);

  const formatCountdown = (minutes: number) => {
    const hours = Math.floor(minutes / 60);
    const mins = minutes % 60;
    if (hours > 0) {
      return `(Next: ${hours}h ${mins}m)`;
    }
    return `(Next: ${mins}m)`;
  };

  if (events.length === 0) {
    return (
      <div className="flex flex-col h-full">
        <div className="flex items-center justify-between mb-3">
          <h2 className="text-xs font-semibold tracking-[0.2em] text-epd-gray uppercase">Upcoming</h2>
          <div className="h-px flex-1 ml-3 bg-epd-gray/30" />
        </div>
        <div className="text-sm text-epd-gray">No upcoming events</div>
      </div>
    );
  }

  return (
    <div className="flex flex-col h-full">
      <div className="flex items-center gap-2 mb-3">
        <h2 className="text-xs font-semibold tracking-[0.2em] text-epd-gray uppercase">Upcoming</h2>
        {minutesUntilNext !== null && minutesUntilNext >= 0 && (
          <span className="text-[11px] text-epd-gray">{formatCountdown(minutesUntilNext)}</span>
        )}
        <div className="h-px flex-1 ml-2 bg-epd-gray/30" />
      </div>
      <div className="flex flex-col gap-3 overflow-y-auto pr-1">
        {events.map((event, index) => {
          const compactDate = event.date.replace(/^0+/, '').replace(/-/g, '/');
          const compactTime = event.startTime.replace(/^0+/, '');
          
          return (
            <div key={index} className="flex flex-col gap-1 pb-2 border-b border-epd-gray/30 last:border-b-0 last:pb-0">
              <div className="flex items-center gap-2">
                <div className="h-8 w-1 bg-epd-gray/50 rounded-full" />
                <div className="flex flex-col leading-tight">
                  <div className="text-xs text-epd-gray">
                    {compactDate} {compactTime}
                  </div>
                  <div className="text-sm text-epd-black">
                    {event.title.length > 32 ? `${event.title.substring(0, 29)}...` : event.title}
                  </div>
                </div>
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
}
