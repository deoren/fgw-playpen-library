// mouse.h - Basic support for reading the mouse.
//
// Created: Jan 2003 
// Author: Garry Lancaster
//
// Notes:
// 1. The mouse class is implemented in playpen.cpp.
// bug in mous set functions fixed 29/05/03

#if !defined(MOUSE_H)
#define MOUSE_H

namespace studentgraphics {

	namespace detail {	
		// Forward declare the class that provides the implementation.
		class SingletonWindow;
	}
	class playpen;

	class mouse {
	public:
		class location {
			int x_, y_;
		public:
			int x()const {return x_;}
			int y()const {return y_;}
			void x(int i) {x_ = i;}
			void y(int i) {y_ = i;}
		};

		mouse();
		~mouse();

		// Returns:
		//	The location of the mouse cursor or {-1, -1} if the mouse is not
		//	currently owned by the playpen window.
		location cursor_at() const;

		// Returns:
		//	true if a mouse button is currently pressed, false if it is not or
		//	the playpen window does not currently own the mouse.
		bool button_pressed() const;

	private:
		detail::SingletonWindow*	window_;

		// Not copyable. If we change it to be copyable, don't neglect the
		// reference counting of the window_.
		mouse(mouse const &);
		mouse& operator=(mouse const &);
	};// class mouse

}// namespace studentgraphics

namespace fgw {
	using namespace studentgraphics;
}

#endif // Header guards.

