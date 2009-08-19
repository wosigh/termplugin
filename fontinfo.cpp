#include "fontinfo.hpp"
#include "common.h"
#include <stdio.h>
#include "font.xpm"
#include "font-4x6.xpm"
#include "font-5x7.xpm"
#include "font-6x10.xpm"


typedef struct
	{
	int width;
	int height;
	const char **data;
	}FontFiles;
static const FontFiles fontFiles[] =
	{
		{	width:4,	height:6,	data:font_4x6_xpm	},
		{	width:5,	height:7,	data:font_5x7_xpm	},
		{	width:8,	height:8,	data:font_8x8_xpm	},
		{	width:6,	height:10,	data:font_6x10_xpm	},
		{	0, 0, 0 },
	};

FontInfo::FontInfo(int width,int height) :
	_glyph_start(32),
	_glyph_end(127)
	{
	const FontFiles *ff = fontFiles;
	for(; ff->height; ff++)
		{
		if(ff->height >= height)
			break;
		}
	if(!ff->height)
		ff--;	//	use largest font
	_char_width = ff->width;
	_char_height = ff->height;
	_glyph_data = ff->data;
#if 0
	if(height >= 8)
	{
		_char_width = 8;
		_char_height = 8;
		_glyph_data = font_8x8_xpm;
	}
	else
	{
		_char_width = 4;
		_char_height = 6;
		_glyph_data = font_4x6_xpm;
	}
#endif
	my_fprintf(stderr,"  FontInfo(width:%d, height:%d): width:%d, height:%d\n", width, height, _char_width, _char_height);
	}

int FontInfo::getLine(int y)
{
	return (y + _char_height - 1) / _char_height;
}
