#ifndef __COMMON_H__
#define __COMMON_H__

#ifndef NDEBUG
#define DEBUG_OUTPUT			//	allow debug output
#endif

#define USE_MOJO_FOR_KEYSTATE

#ifdef DEBUG_OUTPUT
#define my_fprintf				fprintf
#else
#define my_fprintf(io, fmt, ...)
#endif

#define RGB(r,g,b)		(((r) << 16) | ((g) << 8) | (b) | 0xFF000000)
//#define RGB(r, g, b)	(colorAlpha | ((r)<<16) | ((g)<<8) | (b))
#define max(a, b)		(	((a) >= (b))	? (a) : (b)	)
#define min(a, b)		(	((a) <= (b))	? (a) : (b)	)
//void debugLog(int line, const char *format, ...);
double CurrentTime();

typedef enum
{
	colorAlpha		= 0xFF000000,
	colorBlack		= colorAlpha | 0,
	colorWhite		= colorAlpha | 0xFFFFFF,
	colorLightGrey	= colorAlpha | 0x808080,
	colorMediumGrey	= colorAlpha | 0x777777,
	colorRed		= colorAlpha | 0xFF0000,
	colorGreen		= colorAlpha | 0x00FF00,
	colorBlue		= colorAlpha | 0x0000FF,
	colorFuchsia	= colorAlpha | 0xFF00FF,
	colorYellow		= colorAlpha | 0xFFFF00,
	colorAqua		= colorAlpha | 0x00FFFF,
}eRGB_Color;


typedef enum
{
	ansiBlack		= 0,
	ansiRed			= 1,
	ansiGreen		= 2,
	ansiYellow		= 3,
	ansiBlue		= 4,
	ansiMagenta		= 5,
	ansiCyan		= 6,
	ansiWhite		= 7,
	ansiColorMask	= 7,
	ansiDefault		= 9,
} eANSI_Color;
#endif //__COMMON_H__
