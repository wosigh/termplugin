#ifndef __FONTINFO_HPP__
#define __FONTINFO_HPP__

//#include "font.xpm"
class FontInfo {
public:
	FontInfo(int width,int height);
	~FontInfo()	{	}
	int char_width()			{	return _char_width;		}
	int char_height()			{	return _char_height;	}
	int line_height()			{	return _char_height;	}
	const char **glyph_data()	{	return _glyph_data;		}
	bool isValidGlyph(int ch)	{	return (ch >= _glyph_start && ch <= _glyph_end);	}
	int glyph_start()			{	return _glyph_start;	}
	int getLine(int y);
private:
	int _char_width;
	int _char_height;
	int _glyph_start;		//	first glyph in font
	int _glyph_end;			//	last glyph in font
	const char **_glyph_data;
};
#endif //__FONTINFO_HPP__
