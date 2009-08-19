#ifndef SCREEN_HPP_
#define SCREEN_HPP_

#include "charbuffer.hpp"
#include <algorithm>

class Screen {
public:
	typedef CharBuffer CharBuf;
	typedef ScreenCursor Cursor;
	static const int default_font_width = 8;
	static const int default_font_height = 8;

	Screen()
	: _font_ptr(new FontInfo(default_font_width,default_font_height)),
	_width_pixels(0),
	_height_pixels(0),
	_visible_height_pixels(0)
	// Note that the font_width and font_height passed in is a preference, and the actual
	// font may not be that size, so we can't use the passed-in values to calculate the
	// character buffer size.
	{
	}

	~Screen()
	{
		delete _font_ptr;
	}

	void setVisibleHeightPixels(int height_pixels)
	{
		_visible_height_pixels = height_pixels;
		if (_visible_height_pixels>_height_pixels) {
			_height_pixels = _visible_height_pixels;
			_updateCharBufSize();
		}
		_updateCharBufVisibleSize();
	}

	// returns false if the size was the same.
	bool setSizePixels(int width,int height)
	{
		if (width==_width_pixels && height==_height_pixels) {
			return false;
		}
		_width_pixels = width;
		_height_pixels = height;
		_updateCharBufSize();
		_updateCharBufVisibleSize();
		return true;
	}

	CharBuf &charBuf() { return _char_buf; }

	FontInfo &font()
	{
		assert(_font_ptr);
		return *_font_ptr;
	}

	const Cursor &cursor() { return _char_buf.cursor(); }

	void setFont(int width, int height)
	{
		FontInfo *font = new FontInfo(width, height);
		delete _font_ptr;
		_font_ptr = font;
		_updateCharBufSize();
		_updateCharBufVisibleSize();
	}

private:
	FontInfo *_font_ptr;
	int _width_pixels;
	int _height_pixels;
	int _visible_height_pixels;
	CharBuffer _char_buf;

	void _updateCharBufSize()
	{
		FontInfo &font = this->font();
		int width_chars = std::max(_char_buf.width(),_width_pixels/font.char_width());
		int height_chars = _height_pixels/font.char_height();
		_char_buf.setSize(width_chars,height_chars);
	}

	void _updateCharBufVisibleSize()
	{
		FontInfo &font = this->font();
		int vis_width_chars = _width_pixels/font.char_width();
		int vis_height_chars = _visible_height_pixels/font.char_height();

		_char_buf.setVisibleWidth(vis_width_chars);
		_char_buf.setVisibleHeight(vis_height_chars);
	}
};

#endif /* SCREEN_HPP_ */
