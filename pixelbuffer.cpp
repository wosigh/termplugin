#include "pixelbuffer.hpp"

#define XPM_SKIP_LINES		3

static const PixelBuffer::Pixel colors[8]=
{
		RGB(0,0,0),			//	 0 - Black
		RGB(255,0,0),		//	 1 - Red
		RGB(0,255,0),		//	 2 - Green
		RGB(255,255,0),		//	 3 - Yellow
		RGB(0,0,255),		//	 4 - Blue
		RGB(255,0,255),		//	 5 - Magenta
		RGB(0,255,255),		//	 6 - Cyan
		RGB(255,255,255),	//	 7 - White
};
static const PixelBuffer::Pixel colorsDim[8]=
{
	RGB(128,128,128),	//	 8 - Gray
	RGB(128,0,0),		//	 9 - Red
	RGB(0,128,0),		//	10 - Green
	RGB(128,128,0),		//	11 - Brown
	RGB(0,0,128),		//	12 - Blue
	RGB(128,0,128),		//	13 - Magenta
	RGB(0,128,128),		//	14 - Cyan
	RGB(192,192,192),	//	15 - White
};

void PixelBuffer::clear(Pixel pixel)
{
	int height = _height;
	int width = _width;
	int row = 0;
	for (;row!=height;++row) {
		int col = 0;
		for (;col!=width;++col) {
			writePixel(col,row,pixel);
		}
	}
}


struct FontGlyph {
	FontGlyph(FontInfo &f,int i)
	: font(f), index(i), glyph_data(0), col(0)
	{
		if (!font.isValidGlyph(index)) {
			return;
		}
		const int offset = index - font.glyph_start();
		int line = offset / glyphs_across;
		int row = line * font.char_height();
		int pos = offset % glyphs_across;
		col = pos * font.char_width();
		glyph_data = font.glyph_data()+XPM_SKIP_LINES+row;
	}

	bool isValid() const { return glyph_data!=0; }

	bool pixel(int x,int y) const
	{
		char c = glyph_data[y][col + x];
		if (c==' ') {
			return false;
		}
		return true;
	}

	static const int glyphs_across = 8;

	FontInfo &font;
	int index;
	const char **glyph_data;
	int col;
};

#if 0
#define GLYPHS_ACCROSS 8
static bool FontVal(FontInfo &font,int index,int x,int y)
{
	if (!font.isValidGlyph(index)) {
		return false;
	}
	const int offset = index - font.glyph_start();
	int line = offset / GLYPHS_ACCROSS;
	int row = line * font.char_height();
	int pos = offset % GLYPHS_ACCROSS;
	int col = pos * font.char_width();
	const char **glyph_data = font.glyph_data();
	char c = glyph_data[XPM_SKIP_LINES + row + y][col + x];
	if (c==' ') {
		return false;
	}
	return true;
}
#endif

void PixelBuffer::invertCharXY(int x1,int y1)
{
	int char_height = _font.char_height();
	int char_width = _font.char_width();

	int y = 0;
	for (;y!=char_height;++y) {
		int x = 0;
		for (;x!=char_width;++x) {
			Pixel color = readPixel(x+x1,y+y1);
			writePixel(x+x1,y+y1,inverseColor(color));
		}
	}
}

void PixelBuffer::invertChar(int row,int col)
{
	int char_height = _font.char_height();
	int char_width = _font.char_width();
	int x1 = col*char_width;
	int y1 = row*char_height;

	invertCharXY(x1,y1);
}
#if 0
void PixelBuffer::writeCharXY(int x1,int y1,char c)
{
	int char_height = _font.char_height();
	int char_width = _font.char_width();
	Pixel foreground = pixel(255,255,255);
	Pixel background = pixel(0,0,0);

#if 1
	FontGlyph glyph(_font,c);

	if (!glyph.isValid()) {
		int y = 0;
		for (;y < char_height;++y) {
			int x = 0;
			for (;x < char_width;++x) {
				writePixel(x+x1,y+y1,background);
			}
		}
		return;
	}

	int y = 0;
	for (;y < char_height;++y) {
		int x = 0;
		for (;x < char_width;++x) {
			Pixel color = glyph.pixel(x,y) ? foreground : background;
			writePixel(x+x1,y+y1,color);
		}
	}
#else
	int y = 0;
	for (;y < char_height;++y) {
		int x = 0;
		for (;x < char_width;++x) {
			Pixel color = FontVal(_font,c,x,y) ? foreground : background;
			writePixel(x+x1,y+y1,color);
		}
	}
#endif
}
#endif

void PixelBuffer::writeCharAbsoluteXY(int x1,int y1,char c, Pixel foreground, Pixel background)
{
	int char_height = _font.char_height();
	int char_width = _font.char_width();

	FontGlyph glyph(_font,c);

	if (!glyph.isValid()) {
		int y = 0;
		for (;y < char_height;++y) {
			int x = 0;
			for (;x < char_width;++x) {
				writePixelAbsolute(x+x1,y+y1,background);
			}
		}
		return;
	}

	int y = 0;
	for (;y < char_height;++y) {
		int x = 0;
		for (;x < char_width;++x) {
			Pixel color = glyph.pixel(x,y) ? foreground : background;
			writePixelAbsolute(x+x1,y+y1,color);
		}
	}
}
int PixelBuffer::writeStrAbsoluteXY(int x1,int y1,const char *str, Pixel foreground, Pixel background)
{
	int char_width = _font.char_width();
	for(; *str; str++, x1 += char_width)
		writeCharAbsoluteXY(x1, y1, *str, foreground, background);
	return x1;
}
#if 0
void PixelBuffer::writeCharAbsoluteXY(int x1,int y1,char c)
{
	Pixel foreground = pixel(255,255,255);
	Pixel background = pixel(0,0,0);
	writeCharAbsoluteXY(x1, y1, c, foreground, background);
}

void PixelBuffer::writeChar(int row,int col,char c)
{
	int char_height = _font.char_height();
	int char_width = _font.char_width();
	int x1 = col*char_width;
	int y1 = row*char_height;
	writeCharXY(x1, y1, c);
}
#endif
#if 0
void PixelBuffer::writeCharXY(int x1,int y1,CellBuffer &c)
{
	int char_height = _font.char_height();
	int char_width = _font.char_width();
	Pixel foreground = colors[c.fg_color & ansiColorMask];
	Pixel background = colors[c.bg_color & ansiColorMask];

	FontGlyph glyph(_font,c.ch);

	if (!glyph.isValid()) {
		int y = 0;
		for (;y < char_height;++y) {
			int x = 0;
			for (;x < char_width;++x) {
				writePixel(x+x1,y+y1,background);
			}
		}
		return;
	}

	int y = 0;
	for (;y < char_height;++y) {
		int x = 0;
		for (;x < char_width;++x) {
			Pixel color = glyph.pixel(x,y) ? foreground : background;
			writePixel(x+x1,y+y1,color);
		}
	}
}

void PixelBuffer::writeChar(int row,int col,CellBuffer &c)
{
	int char_height = _font.char_height();
	int char_width = _font.char_width();
	int x1 = col*char_width;
	int y1 = row*char_height;
	writeCharXY(x1, y1, c);
}
#endif
void 
  PixelBuffer::writeCharXY(
    int x1,
    int y1,
    CellBuffer &c,
    AnsiColor default_fg_color,
    AnsiColor default_bg_color
  )
{
	int char_height = _font.char_height();
	int char_width = _font.char_width();
	AnsiColor ansi_fg = c.fg_color;
	AnsiColor ansi_bg = c.bg_color;
	if(c.flags & eInverse)
		{
		AnsiColor tmp = ansi_fg;
		ansi_fg = ansi_bg;
		ansi_bg = tmp;
		}
	const Pixel *colorsList = (c.flags & eDim) ? colorsDim : colors;
	Pixel foreground = colorsList[(ansi_fg == ansiDefault ? default_fg_color : ansi_fg) & ansiColorMask];
	Pixel background = colors[(ansi_bg == ansiDefault ? default_bg_color : ansi_bg) & ansiColorMask];

	FontGlyph glyph(_font,c.ch);

	int y = 0;
	if (!glyph.isValid()) {
		for (;y < char_height;++y) {
			int x = 0;
			for (;x < char_width;++x) {
				writePixel(x+x1,y+y1,background);
			}
		}
	} else if(c.flags & eBold) {
		for (y = 0;y < char_height;++y) {
			for (int x = 0;x < char_width;++x) {
				writePixel(x+x1,y+y1,background);
			}
		}
		for (y = 0;y < char_height;++y) {
			for (int x = 0;x < char_width;++x) {
				if(glyph.pixel(x,y)) {
					writePixel(x+x1,y+y1,foreground);
					writePixel(x+x1+1,y+y1,foreground);
				}
			}
		}
	} else {
		for (;y < char_height;++y) {
			int x = 0;
			for (;x < char_width;++x) {
				Pixel color = glyph.pixel(x,y) ? foreground : background;
				writePixel(x+x1,y+y1,color);
			}
		}
	}
	if(c.flags & eStrikeThrough)
		{
		y = y1 + ((char_height - 1) / 2);
		for (int x = 0;x < char_width;++x)
			writePixel(x+x1,y, foreground);
		}
	if(c.flags & eUnderline)
		{
		y = y1 + (char_height - 1);
		for (int x = 0;x < char_width;++x)
			writePixel(x+x1,y, foreground);
		}
}


void PixelBuffer::writeRow(int row, CharBuffer &char_buf)
	{
	int char_height = _font.char_height();
	int char_width = _font.char_width();
	int y1 = row*char_height;
	const int width = char_buf.width();
	AnsiColor default_fg_color = char_buf.default_fg_color();
	AnsiColor default_bg_color = char_buf.default_bg_color();
	for (int col = 0; col < width; ++col)
		{
		int x1 = col*char_width;
		writeCharXY(x1, y1, char_buf.cell(row,col), default_fg_color, default_bg_color);
		}
	}
