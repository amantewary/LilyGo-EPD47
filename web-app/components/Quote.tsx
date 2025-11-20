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
  const truncatedText = quoteText.length > 90 
    ? `${quoteText.substring(0, 87)}...`
    : quoteText;

  return (
    <div className="text-base text-epd-black italic">
      {truncatedText}
    </div>
  );
}

