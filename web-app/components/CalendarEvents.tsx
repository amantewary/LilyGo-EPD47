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
      <div className="h-full text-epd-black">
        <div className="mb-[22px] flex h-[24px] items-center gap-2">
          <span className="text-[18px] leading-none">▣</span>
          <h2 className="text-[18px] font-semibold uppercase leading-none">UPCOMING</h2>
        </div>
        <div className="text-[18px] leading-snug">No upcoming events</div>
      </div>
    );
  }

  return (
    <div className="h-full text-epd-black">
      <div className="mb-[22px] flex h-[24px] items-center gap-2">
        <span className="text-[18px] leading-none">▣</span>
        <h2 className="text-[18px] font-semibold uppercase leading-none">UPCOMING</h2>
        {minutesUntilNext !== null && minutesUntilNext >= 0 && (
          <span className="text-[13px] leading-none text-epd-black">{formatCountdown(minutesUntilNext)}</span>
        )}
      </div>
      <div className="flex flex-col gap-[8px]">
        {events.slice(0, 5).map((event, index) => {
          const compactDate = event.date.replace(/^0+/, '').replace(/-/g, '/');
          const compactTime = event.startTime.replace(/^0+/, '');
          
          return (
            <div key={index} className="flex flex-col gap-[2px] text-epd-black">
              <div className="text-[15px] leading-tight">
                {compactDate} {compactTime}
              </div>
              <div className="pl-[14px] text-[18px] leading-snug">
                {event.title.length > 28 ? `${event.title.substring(0, 25)}...` : event.title}
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
}
