// keyboard.h - Basic support for reading the keyboard.
//
// Created: Jan 2003 
// Author: Garry Lancaster

#if !defined(KEYBOARD_H)
#define KEYBOARD_H

namespace studentgraphics {

	namespace detail {
		// Forward declare the class that provides the implementation.
		class SingletonWindow;
	}// namespace detail

	// Key codes for "character" keys. At most one of these can be 
	// reported by a single key_pressed call. Not all keyboards have all
	// of these keys. Not all keys have a key code. Unused key codes up
	// to 0xFE may occur: they indicate an unknown key was pressed.
	// Alphanumeric key codes are a subset of ASCII.
	int const key_backspace			= 0x08;
	int const key_tab				= 0x09;
	int const key_enter				= 0x0D;
	int const key_pause				= 0x13;
	int const key_escape			= 0x1B;
	int const key_space				= 0x20;
	int const key_page_up			= 0x21;
	int const key_page_down			= 0x22;
	int const key_end				= 0x23;
	int const key_home				= 0x24;
	int const key_left_arrow		= 0x25;
	int const key_up_arrow			= 0x26;
	int const key_right_arrow		= 0x27;
	int const key_down_arrow		= 0x28;
	int const key_print_screen		= 0x2C;
	int const key_insert			= 0x2D;
	int const key_delete			= 0x2E;
	int const key_help				= 0x2F;
	int const key_0					= 0x30;
	int const key_1					= 0x31;
	int const key_2					= 0x32;
	int const key_3					= 0x33;
	int const key_4					= 0x34;
	int const key_5					= 0x35;
	int const key_6					= 0x36;
	int const key_7					= 0x37;
	int const key_8					= 0x38;
	int const key_9					= 0x39;
	int const key_a					= 0x41;
	int const key_b					= 0x42;
	int const key_c					= 0x43;
	int const key_d					= 0x44;
	int const key_e					= 0x45;
	int const key_f					= 0x46;
	int const key_g					= 0x47;
	int const key_h					= 0x48;
	int const key_i					= 0x49;
	int const key_j					= 0x4A;
	int const key_k					= 0x4B;
	int const key_l					= 0x4C;
	int const key_m					= 0x4D;
	int const key_n					= 0x4E;
	int const key_o					= 0x4F;
	int const key_p					= 0x50;
	int const key_q					= 0x51;
	int const key_r					= 0x52;
	int const key_s					= 0x53;
	int const key_t					= 0x54;
	int const key_u					= 0x55;
	int const key_v					= 0x56;
	int const key_w					= 0x57;
	int const key_x					= 0x58;
	int const key_y					= 0x59;
	int const key_z					= 0x5A;
	int const key_numpad_0			= 0x60;
	int const key_numpad_1			= 0x61;
	int const key_numpad_2			= 0x62;
	int const key_numpad_3			= 0x63;
	int const key_numpad_4			= 0x64;
	int const key_numpad_5			= 0x65;
	int const key_numpad_6			= 0x66;
	int const key_numpad_7			= 0x67;
	int const key_numpad_8			= 0x68;
	int const key_numpad_9			= 0x69;
	int const key_multiply			= 0x6A;
	int const key_add				= 0x6B;
	int const key_subtract			= 0x6D;
	int const key_decimal_point		= 0x6E;
	int const key_divide			= 0x6F;
	int const key_f1				= 0x70;
	int const key_f2				= 0x71;
	int const key_f3				= 0x72;
	int const key_f4				= 0x73;
	int const key_f5				= 0x74;
	int const key_f6				= 0x75;
	int const key_f7				= 0x76;
	int const key_f8				= 0x77;
	int const key_f9				= 0x78;
	int const key_f10				= 0x79;
	int const key_f11				= 0x7A;
	int const key_f12				= 0x7B;

	// Special code to indicate multiple "character" keys were pressed.
	int const key_multiple			= 0xFF;

	// Key codes for "modifier" keys. Zero or more of these can be
	// reported by a single key_pressed call and these may be combined
	// (ORed) with a single "character" key.
	int const modifier_shift		= 0x100;
	int const modifier_control		= 0x200;
	int const modifier_alt			= 0x400;
	int const modifier_caps_lock	= 0x800;
	int const modifier_num_lock		= 0x1000;

	// Some useful bit masks.
	int const character_bits		= 0xFF;
	int const modifier_bits			= 0x1F00;

	class keyboard {
	public:
		keyboard();
		~keyboard();

		// Purpose:
		//	Find out which keys are currently pressed.
		// Returns:
		//	A key code of zero or one "character" keys ORed with zero or
		//	more "modifier" keys. Or 0 if no key is currently pressed.
		//	The special "character" key key_multiple is returned if multiple
		//	"character" key presses are detected (this includes multiple
		//	presses or auto-repeats of the same key).
		// Notes:
		// 1. This function detects key presses only when either the console 
		//	window or playpen window is active.
		int key_pressed() const;

	private:
		detail::SingletonWindow* window_;
		// Not copyable. If we change it to be copyable, don't neglect the
		// reference counting of the window_.
		keyboard(keyboard const &);
		keyboard& operator=(keyboard const &);	     	 
	};// class keyboard

}// namespace studentgraphics

namespace fgw {

	using namespace studentgraphics;

}

#endif // Header guards.


