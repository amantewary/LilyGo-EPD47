'use client';

import { QuoteData } from '@/lib/types';
import { useState, useEffect } from 'react';

interface QuoteProps {
  quotes: QuoteData[];
  currentIndex: number;
}

export default function Quote({ quotes, currentIndex }: QuoteProps) {
  const [displayQuote, setDisplayQuote] = useState<QuoteData | null>(null);

  useEffect(() => {
    if (quotes.length > 0 && currentIndex < quotes.length) {
      setDisplayQuote(quotes[currentIndex]);
    }
  }, [quotes, currentIndex]);

  if (!displayQuote) {
    return null;
  }

  const quoteText = `"${displayQuote.text}"`;
  const maxLength = 120;
  const truncatedText =
    quoteText.length > maxLength
      ? `${quoteText.substring(0, maxLength - 3)}...`
      : quoteText;

  return (
    <div className="flex flex-col gap-1">
      <div className="text-base text-epd-black italic leading-relaxed">
        {truncatedText}
      </div>
      {displayQuote.author && (
        <div className="text-xs text-epd-gray uppercase tracking-[0.12em]">
          — {displayQuote.author}
        </div>
      )}
    </div>
  );
}
