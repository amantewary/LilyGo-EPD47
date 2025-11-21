'use client';

import { format, startOfWeek, addDays, getDate, getMonth, getYear, isSameDay } from 'date-fns';

export default function MiniCalendar() {
  const today = new Date();
  const weekStart = startOfWeek(today, { weekStartsOn: 0 }); // Sunday = 0
  const days = Array.from({ length: 7 }, (_, i) => addDays(weekStart, i));
  const dayLabels = ['S', 'M', 'T', 'W', 'T', 'F', 'S'];

  return (
    <div className="flex flex-col">
      <div className="flex justify-around mb-2">
        {dayLabels.map((label, index) => (
          <div key={index} className="text-xs text-epd-gray w-12 text-center">
            {label}
          </div>
        ))}
      </div>
      <div className="flex justify-around">
        {days.map((day, index) => {
          const dayNum = getDate(day);
          const isToday = isSameDay(day, today);
          
          return (
            <div
              key={index}
              className={`w-12 h-8 flex items-center justify-center text-xs ${
                isToday
                  ? 'border border-epd-black text-epd-black font-semibold'
                  : 'text-epd-black'
              }`}
            >
              {dayNum}
            </div>
          );
        })}
      </div>
    </div>
  );
}

