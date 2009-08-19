class ScreenCursor {
public:
	ScreenCursor(int screen_width,int screen_height)
	: _screen_width(screen_width),
	_screen_height(screen_height),
	_row(0),
	_col(0),
	_scroll_top(0),
	_scroll_bottom(screen_height)
	{
	}

	int row() const { return _row; }
	int col() const { return _col; }

	void setScreenWidth(int value)
	{
		_screen_width = value;
		fix();
	}

	void setScreenHeight(int value)
	{
		_screen_height = value;
		_scroll_top = 0;
		_scroll_bottom = value;
		fix();
	}

	int screenWidth() const { return _screen_width; }
	int screenHeight() const { return _screen_height; }

	void fix()
	{
		if (_row<0) {
			_row = 0;
		}
		if (_row>=_screen_height) {
			_row = _screen_height-1;
		}
		if (_col<0) {
			_col = 0;
		}
		if (_col>_screen_width) {
			// the cursor can be one character off the screen to the right
			_col = _screen_width;
		}
	}

	void setScrollRegion(int top, int bottom)
	{
		_scroll_top = top;
		_scroll_bottom = bottom;
	}

	int scrollTop()	{ return _scroll_top;	}
	int scrollBottom()	{ return _scroll_bottom; }

	void set(int r,int c)
	{
		_row = r;
		_col = c;
		fix();
	}

	void moveHome()
	{
		_row = 0;
		_col = 0;
	}

	void moveUp(int count=1)
	{
		_row-=count;
		if (_row<0) {
			_row = 0;
		}
	}

	void moveRight()
	{
		++_col;
		fix();
	}

	void backSpace()
	{
		--_col;
		if (_col<0) {
			_col = _screen_width+_col;
			moveUp();
		}
	}

	void moveLeft()
	{
		--_col;
		if (_col<0) {
			_col = 0;
		}
	}

	void carriageReturn()
	{
		_col = 0;
	}

	void moveDown()
	{
		++_row;
		if (_row==_screen_height) {
			_row = _screen_height-1;
		}
	}

	void lineFeed()
	{
		moveDown();
	}

	bool atTop() const
	{
		return _row==0;
	}

	bool atBottom() const
	{
		return _row == _scroll_bottom - 1;//_screen_height-1;
	}

	bool atRight() const
	{
		return _col >= _screen_width-1;
	}

	bool atLeft() const
	{
		return _col == 0;
	}

private:
	int _screen_width;
	int _screen_height;
	int _row;
	int _col;
	int _scroll_top;
	int _scroll_bottom;
};
