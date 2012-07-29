//////////////////////////////////////////////////////////////////////
//
// File:			playpen.h
// Description:		Declaration of the playpen as described
//                  in CVu 9-2. A few things have been assumed.
//                  Like should the display vanish on ~playpen,
//                  and what does colour(n) actually mean.
// History:
//
// 1.0	19-01-1997	SDW	Original
// rewritten by Francis Glassborow 29/09/2002
// uses namespaces and exceptions
// plus style changes
// minor mods by Garry Lancaster 10/2002 
/////////////////////////////////////////////////////////////////////
#if !defined (PLAYPEN_H)
#define PLAYPEN_H
#include "fgw_text.h"
#include <bitset>
#include <iostream>
#include <string>


namespace studentgraphics {
	class playpen;

	// Frequently used elements of the standard library.
	using std::cout;
	using std::cin;
	using std::endl;
	using std::string;
	using std::istream;
	using std::ostream;
	using std::bitset;

	// Wrapper for OS-specific sleep function.
    void Wait(unsigned ms);

	namespace detail {	
		// Forward declare the class that provides the OS specific code
		class SingletonWindow;
	}
	
	// A class to support palette codes and limit the operators that can be used
	
	class hue{
	public:
		hue(unsigned char code = 0):code_(code){}
		hue(int code):code_(code%256){}
		operator unsigned char()const{return code_;}
		bool operator[](int bit)const {if(bit <0 or bit >7) return false; return bitset<8>(code_)[bit];}
		class ref;
		friend class hue::ref;
		class ref{
		public:
			ref(hue & h, int n):h_(h),bit(n){};
			operator bool(){if (bit <0 or bit >7) return false; return bitset<8>(h_)[bit];}
			void operator=(bool val){if(bit <0 or bit >7) return; bitset<8> v(h_); v[bit] = val; h_ = (unsigned char)v.to_ulong();} 
		private:
			hue & h_;
			int bit;
		};
		ref operator[](int bit){return ref(*this, bit);}
		unsigned char value()const{return code_;}	 	 
		hue operator +=(hue h){code_ |= h.code_; return code_;}
		hue operator -=(hue h){code_ &= ~h.code_; return code_;}
	private:
		unsigned char code_;
	};
	inline hue operator+(hue code1, hue code2){return (code1.value() | code2.value());}	 
	inline hue operator+(unsigned char code1, hue code2){return (code1 | code2.value());}
	inline hue operator+(hue code1, unsigned char code2){return (code1.value() | code2);}	 
	inline hue operator-(hue code1, hue code2){return  (code1.value() & ~(code2.value()));}
 	inline hue operator-(unsigned char  code1, hue code2){return (code1 & ~(code2.value()));} 	 
	inline hue operator-(hue code1, unsigned char code2){return (code1.value() & ~(code2));}
	// prevent multiplication/division of palette codes
	hue operator*(hue, hue);
	hue operator/(hue, hue);
	// provide special values for rgbpalette
	hue const white(255);
	hue const black(0);
	hue const red4(128);
	hue const red2(64);
	hue const red1(32);
	hue const green4(16);
	hue const green2(8);
	hue const green1(1);
	hue const blue4(4);
	hue const blue2(2);
	hue const blue1(1);
	hue const torquoise(1);
	inline istream & operator >> (istream & in , hue & shade){
		shade = (std::cin == in ? fgw::read<int>() : fgw::read<int>(in));
		return in;
	}
	
	
	// A typedef for the palette elements	    
	typedef unsigned char palettecode;
	
	
	struct HueRGB {
		unsigned char r;
		unsigned char g;
		unsigned char b;

		HueRGB() : r(0), g(0), b(0) {}
		HueRGB(unsigned char red, unsigned char green, unsigned char blue) :
			r(red), g(green), b(blue) {}
	};

	unsigned int const colours = 0x100;	// number of colours in palette
	int const Xpixels = 512;	// pixels across
	int const Ypixels = 512;	// pixels down
	// plotmode determines the way source and destination hues are combined 
	// during a plot.  
	enum plotmode { direct, additive, filter, disjoint};

	class pixelsize {
	public:
		pixelsize(int size =1);
		int size()const {return dim;}
		bool size(int i){if(i<1 || i >64) return false; dim = i; return true;}
	private:
		int dim;
	};
	inline pixelsize::pixelsize(int size):dim(size){if(dim <1) dim = 1;}

	// Front end.
	class playpen {
	public:
		class exception{
		public:
			enum level{unknown, fatal, error, warning, info};
			exception():lev_(unknown), message_("unknown problem"){};
			exception(level l,char const * message):lev_(l), message_(message){};
			void report()const {cout << message_ << endl;}
			// Compiler generated copying and destruction are OK.
				  	  	   
		private:
			level lev_;
			string message_;	
		};

		class raw_pixel_data {
			int const x_, y_;
		public:
			explicit raw_pixel_data(int xval=0, int yval=0):x_(xval), y_(yval) {};
			int x() const {return x_;}
			int y() const {return y_;}
		};
// 07/06/03 changed to more general name, derived to provide
// backward compatibility
		class origin_data: public raw_pixel_data {
		public:
			explicit origin_data(int xval=0, int yval = 0):raw_pixel_data(xval, yval){}
		};
		
		
		
		playpen(hue background = white);
		~playpen();

		// Playpens are copyable because all instances share the same 
		// representation. But we need a copy ctor to maintain the reference count
		playpen(playpen const & pp);
		// Update the physical display.
		playpen const&	display() const;

		// Plot a pixel. Changes are not visible until next display() call.
		playpen&		plot(int x, int y, hue h);
		// handle plotting points that are provided as doubles,
		// the rounding is to avoid problems of conversion from int to double and back again
		// introducing a rounding error
		playpen&		plot(double x, double y, hue h){return plot(int(x+0.5), int(y+0.5), h);}
   	    hue	    	    get_hue(int x, int y)const;
	      
		// Set the plotting mode for subsequent calls to plot().
		plotmode		setplotmode(plotmode pm);
		playpen&		origin(int xval, int yval){xorg = xval; yorg = yval; return *this;}
		origin_data	   	origin()const {return origin_data(xorg, yorg);}
		bool			scale(int i){return pixsize.size(i);}
		int				scale()const {return pixsize.size();}
		raw_pixel_data get_raw_xy(int i, int j){return raw_pixel_data(xorg+ i * pixsize.size(),
																yorg - j * pixsize.size());}			
		
		
		
		
		// Clear all pixels to the specified hue. Changes is not visible
		// until the next display() call.
		playpen&		clear(hue h = white);
		playpen&		rgbpalette();
		

		// Palette handling: how hues map to a RGB (red, green, blue)
		// value. Depending on display mode there may not be an exact match
		// between the RGB value specified and that physically displayed.
		// Initial palette (when the first playpen object is created) has
		// entry 255 white and entries 0-215 (inclusive) a standard web
		// safety palette, including black at entry 0.

		// Change a palette entry. Change is not visible until the next call
		// to UpdatePalette().
		playpen&		setpalettentry(hue, HueRGB const & target);

		// Get the RGB value mapped to a specified hue.
		HueRGB getpalettentry(hue) const;

		// Update the physical palette, possibly changing the display.		
		playpen const &	updatepalette() const;

		// Persistence (a.k.a serialization). Ensure stream is opened in
		// binary (not text) mode (at least for MSVC6 - a bug, I think).

		// Save all state to binary file.
		ostream & save(ostream &)const;	 	 
		
		// Restore all state from binary file. Automatically updates physical
		// display to reflect changed state.
		istream & restore(istream &);	

		// Not currently implemented.
		//ostream & savepalette(ostream&);
		//istream & restorepalette(ostream&);

		// GSL: Added. Required by MiniPNG. Ignores plotmode, origin and 
		// scaling.
		hue getrawpixel(int x, int y) const;
		void setrawpixel(int x, int y, hue h);

	private:
		plotmode pmode;
		int xorg, yorg;
		pixelsize pixsize;
		
		// There is only one SingletonWindow, used by all playpen objects.
		static detail::SingletonWindow * graphicswindow; 
	};// class playpen

// two utility functions for Playpen + PNG

	// Purpose:
	//	Load a playpen image from PNG format.
	// Parameters:
	//	[out] p -	The playpen to which the image will be written.
	//	[in, out] stm -	The stream from which the PNG data will be read. This
	//					stream must be opened in binary mode.
	// Exception Safety:
	//	Basic.
	void LoadPlaypen(playpen& p, std::string filename);

	// Purpose:
	//	Save a playpen image in PNG format.
	// Parameters:
	//	[in] p -	The playpen from which the image will be read.
	//	[in, out] stm - The stream to which the image will be saved. The
	//					stream must be opened in binary mode.
	// Exception Safety:
	//	Basic.
	void SavePlaypen(playpen const & p, std::string filename);
	
}// namespace studentgraphics

namespace fgw {
	using namespace studentgraphics;
}
#endif

