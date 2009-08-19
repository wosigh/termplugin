#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include "fontinfo.hpp"
#include "charbuffer.hpp"

class PixelBuffer {
public:
	typedef uint32_t Pixel;

	PixelBuffer(FontInfo &font, Pixel *buf,int w,int h,int stride_bytes,int top)
	: _font(font), _buffer(buf), _width(w), _height(h), _stride(stride_bytes/4), _top(top)
	{
	}

	// The first visible line
	int topY() const { return _top; }

	// One past the last visible line
	int bottomY() const { return _top+_height; }

	// The first visible row
	int topRow() const
	{
		return topY()/_font.char_height();
	}

	// One past the last visible row
	int bottomRow() const
	{
		return (bottomY()-1)/_font.char_height()+1;
	}

	//void writeChar(int row,int col,char c);
	//void writeChar(int row,int col,CellBuffer &c);
	//void writeCharXY(int x,int y,char c);
	//void writeCharXY(int x,int y,CellBuffer &c);
	//void writeCharAbsoluteXY(int x1,int y1,char c);
	void writeCharAbsoluteXY(int x1,int y1,char c, Pixel foreground, Pixel background);
	int writeStrAbsoluteXY(int x1,int y1,const char *str, Pixel foreground, Pixel background);
	void writeCharXY(int x1,int y1,CellBuffer &c, AnsiColor default_fg_color, AnsiColor default_bg_color);
	void writeRow(int row, CharBuffer &char_buf);
	void invertChar(int row,int col);
	void invertCharXY(int x,int y);
	bool validXY(int x,int y)
	{
		return x>=0 && x<_width && y>=_top && y<_height+_top;
	}
	bool validAbsoluteXY(int x,int y)
	{
		return x>=0 && x<_width && y>=0 && y<_height;
	}
	int index(int x,int y)
	{
		assert(validXY(x,y));
		return (y-_top)*_stride+x;
	}
	int indexAbsolute(int x,int y)
	{
		assert(validAbsoluteXY(x,y));
		return y*_stride+x;
	}
	void writePixelAbsolute(int x,int y,Pixel p)
	{
		if (!validAbsoluteXY(x,y)) {
			return;
		}
		_buffer[indexAbsolute(x,y)] = p;
	}
	void writePixel(int x,int y,Pixel p)
	{
		if (!validXY(x,y)) {
			return;
		}
		_buffer[index(x,y)] = p;
	}
	Pixel readPixel(int x,int y)
	{
		if (!validXY(x,y)) {
			return pixel(0,0,0);
		}
		return _buffer[index(x,y)];
	}

	void writePixel(int x,int y,int r,int g,int b)
	{
		writePixel(x,y,pixel(r,g,b));
	}

	static Pixel pixel(int r,int g,int b)
	{
		int a = 255;
		return (a<<24) | (r<<16) | (g<<8) | b;
	}

	static void getComponents(Pixel p,int *rp,int *gp,int *bp)
	{
		*rp = (p>>16)&0xff;
		*gp = (p>>8)&0xff;
		*bp = (p)&0xff;
	}

	Pixel inverseColor(Pixel p)
	{
		int r,g,b;
		getComponents(p,&r,&g,&b);
		return pixel(255-r,255-g,255-b);
	}

	void clear(Pixel p);

	//static const int char_width = 8;
	//static const int char_height = 8;
private:
	FontInfo &_font;
	Pixel *_buffer;
	int _width;
	int _height;
	int _stride;
	int _top;
};
