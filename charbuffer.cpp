#include "charbuffer.hpp"
#include "pixelbuffer.hpp"
#include <string.h>
#include "common.h"

CharBuffer::CharBuffer()
: _width(0),
_height(0),
_cells(0),
_active_fg_color(ansiDefault),
_active_bg_color(ansiDefault),
_default_fg_color(ansiGreen),
_default_bg_color(ansiBlack),
_active_flags(eNormal),
_cursor(0, 0)
{
}

void CharBuffer::reset()
	{
	resetAttr();
	homeCursor();
	int cellCount = _height * _width;
	CellBuffer *o = _cells;
	for (int j = 0; j < cellCount; j++, o++)
		{
		o->ch = ' ';
		o->fg_color = ansiDefault;
		o->bg_color = ansiDefault;
		o->flags = eNormal;
		}
	}

bool CharBuffer::setSize(int new_width, int new_height)
{
	assert(new_width>=0);
	assert(new_height>=0);
	if (new_width==width() && new_height==height()) {
		// it's ok, we have no need to resize
		return false;
	}
	fprintf(stderr,"Changing size from %dx%d to %dx%d\n",_width,_height,new_width,new_height);
	int new_size = new_width * new_height;
	CellBuffer *tmp = (new_size==0) ? 0 : new CellBuffer [new_size];
	int maxCol = min(new_width, _width);
	int maxRow = min(new_height, _height);

#if 1
	// This seems to work better than the code below.  Without
	// this you can get ghost data from previous runs.
	{
		for (int row = 0; row<new_height;++row) {
			for (int col = 0; col<new_width;++col) {
				CellBuffer &cell = tmp[row*new_width+col];
				cell.ch = ' ';
				cell.fg_color = _active_fg_color;
				cell.bg_color = _active_bg_color;
				cell.flags = _active_flags;
			}
		}
	}
#else

	if(new_width > _width)
	{
		//	we are expanding the width, so we must pad out all the cols
		CellBuffer *o = tmp;
		for(int row = 0; row < maxRow; row++, o += new_width)
		{
			for (int col = _width; col < new_width;++col) {
				o[col].ch = ' ';
				o[col].fg_color = _active_fg_color;
				o[col].bg_color = _active_bg_color;
				o[col].flags = _active_flags;
			}
		}
	}
	if(new_height > _height)
	{
		CellBuffer *o = tmp + _height;
		for(int row = maxRow; row < new_height; row++, o += new_width)
		{
			for (int col = 0; col < new_width;++col) {
				o[col].ch = ' ';
				o[col].fg_color = _active_fg_color;
				o[col].bg_color = _active_bg_color;
				o[col].flags = _active_flags;
			}
		}
	}
#endif

	my_fprintf(stderr,"Changing size to %dx%d cleared\n",new_width,new_height);

	for(int row = 0; row < maxRow; row++)
	{
		memcpy(
				tmp + ((new_height-1-row) * new_width),
				_cells + ((_height-1-row) * _width),
				maxCol * sizeof(CellBuffer)
		);		//	copy all the data
	}
	// clean up old data
	delete [] _cells;

	// store the new values here
	_cells = tmp;
	_width = new_width;	// store the new width
	_height = new_height;

	my_fprintf(stderr,"Changing size to %dx%d complete\n",new_width,new_height);
	return true;
}

#if 0
int CharBuffer::addLines(int lines)
{
	return resize(_width, _height + lines);
}
#endif

void CharBuffer::clearRow(int row)
{
	assert(row<height());
	int width = _width;
	int col = 0;
	for (;col!=width;++col) {
		CellBuffer &theCell = cell(row,col);
		theCell .ch = ' ';
		theCell .fg_color = _active_fg_color;
		theCell .bg_color = _active_bg_color;
		theCell .flags = _active_flags;
	}
}

void CharBuffer::_getScrollRange(int *top_ptr,int *bottom_ptr)
{
	assert(top_ptr);
	assert(bottom_ptr);
	int height = _height;
	int top = _cursor.scrollTop();
	int bottom = _cursor.scrollBottom();
	if (top==0) {
		*top_ptr = 0;
	}
	else {
		*top_ptr = (height - _cursor.screenHeight()) + top;
	}
	*bottom_ptr = (height - _cursor.screenHeight()) + bottom;
	assert(*top_ptr>=0);
	assert(*bottom_ptr<=height);
}

void CharBuffer::scrollUp(int count)
{
	int top,end;
	_getScrollRange(&top,&end);

	int width = _width;

	int row = top;
	for (;row<end-count;++row) {
		int col = 0;
		for (;col!=width;++col) {
			cell(row,col) = cell(row+count,col);
		}
	}
	for (;row<end;++row) {
		clearRow(row);
	}
}

void CharBuffer::scrollDown(int count)
{
	int top,end;
	_getScrollRange(&top,&end);

	int width = _width;

	int row = end-1;
	for (;row-count>=top;--row) {
		int col = 0;
		for (;col!=width;++col) {
			cell(row,col) = cell(row-count,col);
		}
	}
	for (;row>=top;--row) {
		clearRow(row);
	}
}
