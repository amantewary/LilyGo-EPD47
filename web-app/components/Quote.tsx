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

  const quoteText = `"${displayQuote.text}"${displayQuote.author ? ` - ${displayQuote.author}` : ''}`;
  const maxLength = 150;
  const truncatedText =
    quoteText.length > maxLength
      ? `${quoteText.substring(0, maxLength - 3)}...`
      : quoteText;

  return (
    <div className="h-full text-epd-black">
      <div className="text-[19px] leading-[1.35]">
        {truncatedText}
      </div>
    </div>
  );
}
