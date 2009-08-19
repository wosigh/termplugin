#ifndef __CHARBUFFER_HPP__
#define __CHARBUFFER_HPP__
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

#include "screencursor.hpp"
#include "fontinfo.hpp"
#include "common.h"

typedef uint8_t AnsiColor;
typedef enum
{
	eNormal		= 0x00,
	eBold		= 0x01,
	eDim		= 0x02,
	eItalics	= 0x04,
	eUnderline	= 0x08,		//	inverse on; reverses foreground & background colors
	eInverse	= 0x10,
	eBlink		= 0x20,
	eStrikeThrough	= 0x40,
	eFlagsAll	= 0xFF
}CellFlags;
typedef struct
{
	char ch;
	AnsiColor fg_color;
	AnsiColor bg_color;
	uint8_t flags;		//	bold/italic/underline/strike-through
}CellBuffer;

class CharBuffer {
public:
	CharBuffer();
	~CharBuffer()
	{
		delete [] _cells;
	}

	CellBuffer &cell(int row,int col)
	{
		assert(row>=0);
		assert(row<_height);
		assert(col>=0);
		assert(col<_width);
		return _cells[row*_width+col];
	}

	void setVisibleWidth(int value)
	{
		assert(value<=_width);
		_cursor.setScreenWidth(value);
	}

	void setVisibleHeight(int value)
	{
		assert(value<=_height);
		if (value<_cursor.screenHeight()) {
			// If we make it smaller, and the cursor position is still on the
			// screen, then we want to scroll down, so that the text on the top
			// row is preserved.
			int offset = _cursor.screenHeight()-value;
			if (_cursor.row()>=value) {
				// If the cursor position is off the screen, we have to adjust it
				// instead.
				int cursor_change = _cursor.row()-(value-1);
				offset -= cursor_change;
				_cursor.moveUp(cursor_change);
			}
			scrollDown(offset);
		}
		else {
			// If we make it larger, we effectively scroll up because we're adding
			// more rows at the bottom.
			int offset = value-_cursor.screenHeight();
			scrollUp(offset);
		}
		_cursor.setScreenHeight(value);
	}

	int visibleWidth() const { return _cursor.screenWidth(); }
	int visibleHeight() const { return _cursor.screenHeight(); }

	int width() const { return _width; }
	int height() const { return _height; }
	void scrollUp(int count=1);
	void scrollDown(int count=1);
	void clearRow(int row);
	bool setSize(int width,int height);
#if 0
int addLines(int lines);
#endif

int topRow() { return _height-visibleHeight(); }
int currentRow() { return topRow()+_cursor.row(); }
int currentCol() { return _cursor.col(); }
void setFgColor(AnsiColor color)	{	_active_fg_color = color & ansiColorMask;	}
void setBgColor(AnsiColor color)	{	_active_bg_color = color & ansiColorMask;	}
void setFlag(CellFlags flags, bool set)
{
	if(set)
		_active_flags |= flags;
	else
		_active_flags &= ~flags;
}
void setDefaultColors(AnsiColor fg, AnsiColor bg)
{
	/*_active_fg_color = */_default_fg_color = (fg & ansiColorMask);
	/*_active_bg_color = */_default_bg_color = (bg & ansiColorMask);
}
AnsiColor default_fg_color()		{	return _default_fg_color;	}
AnsiColor default_bg_color()		{	return _default_bg_color;	}
void reset();
void resetAttr()
{
	_active_fg_color = ansiDefault;//_default_fg_color;
	_active_bg_color = ansiDefault;//_default_bg_color;
	_active_flags = eNormal;
}

void put(char c)
{
	if (_cursor.col()>=visibleWidth()) {
		newLine();
	}
	assert(currentRow()<height());
	CellBuffer &theCell = cell(currentRow(),currentCol());
	theCell.ch = c;
	theCell.fg_color = _active_fg_color;
	theCell.bg_color = _active_bg_color;
	theCell.flags = _active_flags;
}
void put(char c, AnsiColor fg, AnsiColor bg)
{
	if (_cursor.col()>=visibleWidth()) {
		newLine();
	}
	assert(currentRow()<height());
	CellBuffer &theCell = cell(currentRow(),currentCol());
	theCell.ch = c;
	theCell.fg_color = fg;
	theCell.bg_color = bg;
	theCell.flags = _active_flags;
}

void newLine()
{
	carriageReturn();
	moveCursorDown();
}

void clearToRight()
{
	int row = currentRow();
	int col = currentCol();
	assert(row<height());
	for (;col!=_width;++col) {
		CellBuffer &theCell = cell(row,col);
		theCell.ch = ' ';
		theCell.bg_color = _active_bg_color;
		theCell.fg_color = _active_fg_color;
		theCell.flags = _active_flags;
	}
}

void clearBelow()
{
	int height = _height;
	int row = currentRow()+1;
	for (;row<height;++row) {
		clearRow(row);
	}
}

void advanceCursor() {
	_cursor.moveRight();
}
void setCursor(int row,int col) { _cursor.set(row-1,col-1); }
void setScrollRegion(int top, int bottom) { _cursor.setScrollRegion(top-1, bottom); }

void moveCursorRight(int count=1)
{
	while (count-->0) {
		if (_cursor.atRight()) {
			break;
		}
		_cursor.moveRight();
	}
}

void moveCursorLeft(int count=1)
{
	while (count-->0) {
		if (_cursor.atLeft()) {
			break;
		}
		_cursor.moveLeft();
	}
}

void moveCursorUp(int count=1)
{
	while (count-->0) {
		if (_cursor.atTop()) {
		}
		else {
			_cursor.moveUp();
		}
	}
}

void moveCursorDown(int count=1) {
	while (count-->0) {
		if (_cursor.atBottom()) {
			scrollUp();
		}
		else {
			_cursor.moveDown();
		}
	}
}
void homeCursor() { _cursor.moveHome(); }
void backSpaceCursor() { _cursor.backSpace(); }
void carriageReturn() { _cursor.carriageReturn(); }
const ScreenCursor &cursor() const { return _cursor; }

private:
	int _width;
	int _height;
	CellBuffer *_cells;
	AnsiColor _active_fg_color;
	AnsiColor _active_bg_color;
	AnsiColor _default_fg_color;
	AnsiColor _default_bg_color;
	uint8_t _active_flags;
	ScreenCursor _cursor;

	void _getScrollRange(int *top_ptr,int *bottom_ptr);
};
#endif	//__CHARBUFFER_HPP__
