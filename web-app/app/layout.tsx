import type { Metadata } from 'next'
import './globals.css'

export const metadata: Metadata = {
  title: 'EPD47 Dashboard',
  description: 'Home Assistant Dashboard for EPD47 E-Paper Display',
}

export default function RootLayout({
  children,
}: {
  children: React.ReactNode
}) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  )
}

