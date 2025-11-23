'use client';

import { WeatherData } from '@/lib/types';

interface WeatherProps {
  weather: WeatherData;
}

export default function Weather({ weather }: WeatherProps) {
  const getWeatherIcon = () => {
    const condition = weather.condition.toLowerCase();
    if (condition.includes('sun') || condition.includes('clear')) {
      return '☀️';
    } else if (condition.includes('cloud')) {
      return '☁️';
    } else if (condition.includes('rain')) {
      return '🌧️';
    } else if (condition.includes('snow')) {
      return '❄️';
    }
    return '🌤️';
  };

  return (
    <div className="flex items-center gap-3">
      <span className="text-2xl leading-none">{getWeatherIcon()}</span>
      <div className="flex flex-col leading-tight">
        <span className="text-lg font-semibold text-epd-black">{weather.temperature}</span>
        <span className="text-[11px] text-epd-gray uppercase tracking-[0.12em]">{weather.condition}</span>
      </div>
    </div>
  );
}
