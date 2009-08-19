#ifndef KEYMAN_HPP_
#define KEYMAN_HPP_

#include <assert.h>

class KeyManager {
public:
	static const int buf_size = 5;

	struct Client {
		virtual void sendChars(const char *buf,int len) = 0;
	};

	KeyManager(Client *cp)
	: client_ptr(cp),
	gesture_hold_state(0),
	red_state(0),
	sym_state(0),
	shift_state(0),
	app_cursor_keys(false),
	buf_pos(0)
	{
		assert(client_ptr);
	}

	Client &client() const
	{
		assert(client_ptr);
		return *client_ptr;
	}

	void sendChar(char c)
	{
		assert(buf_pos<buf_size);
		buf[buf_pos++] = c;
	}

	void flush()
	{
		if (buf_pos==0) {
			return;
		}
		client().sendChars(buf,buf_pos);
		buf_pos = 0;
	}

	bool
	keyPressed(
			int key_code,
			bool shift_pressed,
			bool alt_pressed,
			bool ctrl_pressed,
			bool meta_pressed
	);

	void sendEscape() { sendChar(27); }

	void setAppCursorKeys(bool value) { app_cursor_keys = value; }

	void sendCursor(char suffix)
	{
		sendEscape();
		if (app_cursor_keys) {
			sendChar('O');
		}
		else {
			sendChar('[');
		}
		sendChar(suffix);
	}

	void sendSpecial(char c)
	{
		sendCSI();
		sendChar(c);
		sendChar('~');
	}

	void sendCSI() { sendChar(27); sendChar('['); }
	void sendUp() { sendCursor('A'); }
	void sendDown() { sendCursor('B'); }
	void sendRight() { sendCursor('C'); }
	void sendLeft() { sendCursor('D'); }
	void sendHome() { sendSpecial('1'); }
	void sendInsert() { sendSpecial('2'); }
	void sendDelete() { sendSpecial('3'); }
	void sendEnd() { sendSpecial('4'); }
	void sendPrior() { sendSpecial('5'); }
	void sendNext() { sendSpecial('6'); }

	void sendF(int num);  // function keys
	int getGestureHoldSstate()	{	return gesture_hold_state;	}
	int getRedState()			{	return red_state;	}
	int getSymState()			{	return sym_state;	}
	int getShiftState()			{	return shift_state;	}
private:
	Client *client_ptr;
	int gesture_hold_state;
	int red_state;
	int sym_state;
	int shift_state;
	bool app_cursor_keys;
	char buf[buf_size];
	int buf_pos;

};

#endif /* KEYMAN_HPP_ */
