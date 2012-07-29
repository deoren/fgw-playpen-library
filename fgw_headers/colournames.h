#ifndef COLOURNAMES_H
#define COLOURNAMES_H
#include "playpen.h"


// some useful colour names
// they only make sense for the rgbpalette setting
namespace fgw {
	hue const red3(red1 + red2);
	hue const red5(red1 + red4);
	hue const red6(red2 + red4);
	hue const red7(red1 + red2 + red4);
	
	hue const green3(green1 + green2);
	hue const green5(green1 + green4);
	hue const green6(green2 + green4);
	hue const green7(green1 + green2 + green4);
	
	hue const blue3(blue1 + blue2);
	hue const blue5(blue1 + blue4);
	hue const blue6(blue2 + blue4);
	hue const blue7(blue1 + blue2 + blue4);
	
	hue const yellow1(red1 + green1);
	hue const yellow2(red2 + green2);
	hue const yellow3(red3 + green3);
	hue const yellow4(red4 + green4);
	hue const yellow5(red5 + green5);
	hue const yellow6(red6 + green6);
	hue const yellow7(red7 + green7);
	
	hue const magenta1(red1 + blue1);
	hue const magenta2(red2 + blue2);
	hue const magenta3(red3 + blue3);
	hue const magenta4(red4 + blue4);
	hue const magenta5(red5 + blue5);
	hue const magenta6(red6 + blue6);
	hue const magenta7(red7 + blue7);
	
	hue const cyan1(blue1 + green1);
	hue const cyan2(blue2 + green2);
	hue const cyan3(blue3 + green3);
	hue const cyan4(blue4 + green4);
	hue const cyan5(blue5 + green5);
	hue const cyan6(blue6 + green6);
	hue const cyan7(blue7 + green7);
	
	hue const grey1(red1 + green1 + blue1);
	hue const grey2(red2 + green2 + blue2);
	hue const grey3(red3 + green3 + blue3);
	hue const grey4(red4 + green4 + blue4);
	hue const grey5(red5 + green5 + blue5);
	hue const grey6(red6 + green6 + blue6);
}

#endif
