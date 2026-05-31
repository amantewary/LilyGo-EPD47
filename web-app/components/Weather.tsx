'use client';

import { WeatherData } from '@/lib/types';

interface WeatherProps {
  weather: WeatherData;
}

export default function Weather({ weather }: WeatherProps) {
  const getWeatherIcon = () => {
    const condition = weather.condition.toLowerCase();
    if (condition.includes('sun') || condition.includes('clear')) {
      return '☀';
    } else if (condition.includes('cloud')) {
      return '☁';
    } else if (condition.includes('rain')) {
      return '☂';
    } else if (condition.includes('snow')) {
      return '*';
    }
    return '☁';
  };

  return (
    <div className="flex h-full items-center gap-2 text-epd-black">
      <span className="w-[34px] text-[28px] leading-none">{getWeatherIcon()}</span>
      <span className="text-[20px] font-semibold leading-none">
        {weather.temperature.replace(' C', '°C').replace(' F', '°F')}
      </span>
    </div>
  );
}
