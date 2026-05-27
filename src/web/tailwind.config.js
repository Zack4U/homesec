/** @type {import('tailwindcss').Config} */
export default {
  darkMode: 'class',
  content: ['./index.html', './src/**/*.{js,jsx}'],
  theme: {
    extend: {
      colors: {
        border: 'hsl(217 33% 17%)',
        input: 'hsl(217 33% 17%)',
        ring: 'hsl(188 95% 43%)',
        background: 'hsl(222 47% 6%)',
        foreground: 'hsl(210 40% 98%)',
        primary: { DEFAULT: 'hsl(188 95% 43%)', foreground: 'hsl(222 47% 6%)' },
        secondary: { DEFAULT: 'hsl(217 33% 14%)', foreground: 'hsl(210 40% 98%)' },
        destructive: { DEFAULT: 'hsl(0 72% 51%)', foreground: 'hsl(210 40% 98%)' },
        muted: { DEFAULT: 'hsl(217 33% 12%)', foreground: 'hsl(215 20% 55%)' },
        accent: { DEFAULT: 'hsl(217 33% 17%)', foreground: 'hsl(210 40% 98%)' },
        card: { DEFAULT: 'hsl(222 44% 8%)', foreground: 'hsl(210 40% 98%)' },
        popover: { DEFAULT: 'hsl(222 44% 8%)', foreground: 'hsl(210 40% 98%)' },
        warning: { DEFAULT: 'hsl(45 93% 47%)', foreground: 'hsl(222 47% 6%)' },
        success: { DEFAULT: 'hsl(142 71% 45%)', foreground: 'hsl(222 47% 6%)' },
        danger: { DEFAULT: 'hsl(0 84% 60%)', foreground: 'hsl(210 40% 98%)' },
        cyan: {
          50: '#ecfeff', 100: '#cffafe', 200: '#a5f3fc', 300: '#67e8f9',
          400: '#22d3ee', 500: '#06b6d4', 600: '#0891b2', 700: '#0e7490',
          800: '#155e75', 900: '#164e63', 950: '#083344',
        },
      },
      borderRadius: { lg: '0.75rem', md: '0.5rem', sm: '0.375rem' },
      keyframes: {
        'accordion-down': { from: { height: '0' }, to: { height: 'var(--radix-accordion-content-height)' } },
        'accordion-up': { from: { height: 'var(--radix-accordion-content-height)' }, to: { height: '0' } },
        'pulse-glow': { '0%, 100%': { opacity: '1' }, '50%': { opacity: '0.5' } },
        'sweep': { '0%': { transform: 'rotate(0deg)' }, '100%': { transform: 'rotate(180deg)' } },
      },
      animation: {
        'accordion-down': 'accordion-down 0.2s ease-out',
        'accordion-up': 'accordion-up 0.2s ease-out',
        'pulse-glow': 'pulse-glow 2s ease-in-out infinite',
        'sweep': 'sweep 4s linear infinite',
      },
    },
  },
  plugins: [require('tailwindcss-animate')],
};
