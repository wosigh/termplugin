#include "keyman.hpp"


static int TranslateRed(int key_code)
{
	// @ABCDEFGHIJKLMN OPQRSTUVWXYZ
	char table[] = "0&#84156$%!:';?\"=/2-3)9+7(*";
	if (key_code>=64 && key_code<='Z') {
		return table[key_code-64];
	}
	return 0;
}

typedef enum
{
	GestureAreaTap = 231,
	OrangeKey      = 129,
	ShiftKey       = 128,
	SymKey	       = 130,
	dotKey	       = 190,
	commaKey       = 188,
} eKeyCodes;


//	TODO: handle "<>^{}[]~\`"
bool
KeyManager::keyPressed(
		int key_code,
		bool shift_pressed,
		bool alt_pressed,
		bool ctrl_pressed,
		bool meta_pressed
)
{
	bool key_state_changed = false;
	if (key_code==GestureAreaTap) {
		// gesture area tap
		gesture_hold_state = (gesture_hold_state+1)%3;
		key_state_changed = true;
	}
	else if (key_code==OrangeKey) {
		// orange/red key
		red_state = (red_state+1)%3;
		key_state_changed = true;
	}
	else if (key_code==16 || key_code == ShiftKey || (key_code==0 && shift_pressed)) {
		// shift key
		shift_state = (shift_state+1)%3;
		key_state_changed = true;
	}
	else if (key_code==17 || key_code == SymKey || key_code==131) {
		// sym key or ctrl key
		sym_state = (sym_state+1)%3;
		key_state_changed = true;
	}
	else {
		if (key_code>='a' && key_code<='z') {
			key_code -= 32;
		}
		if (key_code>='A' && key_code<='Z') {
			if (red_state || alt_pressed) {
				key_code = TranslateRed(key_code);
			}
			else if (gesture_hold_state || meta_pressed) {
				switch (key_code) {
				case 'E': sendHome(); break;
				case 'R': sendUp(); break;
				case 'T': sendPrior(); break;
				case 'D': sendLeft(); break;
				case 'F': break;
				case 'G': sendRight(); break;
				case 'X': sendEnd(); break;
				case 'C': sendDown(); break;
				case 'V': sendNext(); break;
				case '0': sendInsert(); break;
				case dotKey: sendDelete(); break;
				}
				key_code = 0;
			}
			else if (sym_state || ctrl_pressed) {
				// control characters
				key_code = key_code-64;
			}
			else if (!shift_state && !shift_pressed) {
				key_code += 32;
			}
		}
		else if (key_code=='0' && !red_state) {
			key_code = '@';
		}
		else if (key_code==32 && red_state) {
			// escape
			key_code = 27;
		}
		else if (key_code==commaKey || key_code==',') {
			if (red_state) {
				key_code = '_';
			} else if (shift_state || shift_pressed) {
				key_code = '<';
			} else {
				key_code = ',';
			}
		}
		else if (key_code==dotKey || key_code=='.') {
			if (red_state) {
				key_code = '|';
			} else if (shift_state || shift_pressed) {
				key_code = '>';
			} else {
				key_code = '.';
			}
		}
		if (key_code!=0 && key_code<128) {
			sendChar(key_code);
		}
		if (red_state==1) {
			red_state = 0;
			key_state_changed = true;
		}
		if (shift_state==1) {
			shift_state = 0;
			key_state_changed = true;
		}
		if (sym_state==1) {
			sym_state = 0;
			key_state_changed = true;
		}
		if (gesture_hold_state==1) {
			gesture_hold_state = 0;
			key_state_changed = true;
		}
	}
	flush();
	return key_state_changed;
}


void KeyManager::sendF(int num)
{
	assert(num>=1 && num<=12);
	sendCSI();
	if (num<=5) {
		sendChar('1');
		sendChar('0'+num);
	}
	else if (num<=8) {
		sendChar('1');
		sendChar('0'+num+1);
	}
	else {
		sendChar('2');
		sendChar('0'+num-9);
	}
	sendChar('~');
}
