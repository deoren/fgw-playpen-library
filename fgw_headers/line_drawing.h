#ifndef LINE_DRAWING_H
#define LINE_DRAWING_H
#include "playpen.h"
#include "point2d.h"
#include <fstream>
#include <cmath>

// removed point2d to own header 29/05/03
// renamed line_drawing 29/05/03
// and added plot policy based lines 29/05/03


namespace fgw {
using namespace studentgraphics;
// declaration of plot policy function pointer type
	typedef void (plot_policy)(fgw::playpen &, int x, int y, fgw::hue);
	
//	  wrapper to convert member function to free function
//    allows simple line drawing functions to delegat 
inline void plot(fgw::playpen & canvas, int x, int y, fgw::hue shade = fgw::black){
	canvas.plot(x, y, shade);
}


// two versions of drawline that will overload with fgw::drawline if both namespaces are visible

	void drawline(fgw::playpen & p, int beginx, int beginy, int endx, int endy, fgw::hue, plot_policy plotter = plot);
// next is a forwarding function to allow simple use of fgw::point2d values in drawline
	inline void drawline(fgw::playpen & p, fgw::point2d begin, fgw::point2d end, fgw::hue shade, plot_policy plotter = plot){
		return drawline(p, 
		int(std::floor(begin.x()+.5)), 
		int(std::floor(begin.y()+.5)), 
		int(std::floor(end.x()+0.5)), 
		int(std::floor(end.y()+0.5)), shade, plotter);
	}
// and the special cases where the plot colours is defaulted to black

inline void drawline(fgw::playpen & p, int beginx, int beginy, int endx, int endy, plot_policy plotter = plot){
	return drawline(p, beginx, beginy, endx, endy, fgw::black, plotter);
}

inline void drawline(fgw::playpen & p, fgw::point2d begin, fgw::point2d end, plot_policy plotter = plot){
	return drawline(p, begin, end, black, plotter);
}

// special case lines	 
	void vertical_line(fgw::playpen & p, int xval, int y1, int y2, fgw::hue,  plot_policy plotter = plot);
	void horizontal_line(fgw::playpen & p, int yval, int x1, int x2, fgw::hue, plot_policy plotter = plot);
// with defaulted hue to black
inline void vertical_line(fgw::playpen & p, int xval, int y1, int y2, plot_policy plotter = plot){
	return vertical_line(p, xval, y1, y2, fgw::black, plotter);
}
inline void horizontal_line(fgw::playpen & p, int yval, int x1, int x2, plot_policy plotter = plot){
	return horizontal_line(p, yval, x1, x2, fgw::black, plotter);
}


// inline overloads to handle case where point2d is used to pass start point
	inline void vertical_line(fgw::playpen & p, fgw::point2d pt, int length, fgw::hue shade,  plot_policy plotter=plot){ 
		return vertical_line(p,int(pt.x()+.5),int(pt.y()+.5), int(pt.y()+length+.5), shade, plotter);
	}
	inline void horizontal_line  (fgw::playpen & p, fgw::point2d pt, int length, fgw::hue shade,  plot_policy plotter=plot){
		return horizontal_line(p,int(pt.y()+.5),int(pt.x()+.5), int(pt.x()+length+.5),shade, plotter);
	}
// and with colour defaulted to black


	inline void vertical_line(fgw::playpen & p, fgw::point2d pt, int length,  plot_policy plotter=plot){ 
		return vertical_line(p,int(pt.x()+.5),int(pt.y()+.5), int(pt.y()+length+.5), black, plotter);
	}
	inline void horizontal_line  (fgw::playpen & p, fgw::point2d pt, int length,  plot_policy plotter=plot){
		return horizontal_line(p,int(pt.y()+.5),int(pt.x()+.5), int(pt.x()+length+.5), black, plotter);
	}

// old versions implementation provided by defaults to above
//	  inline void drawline(playpen & p, int beginx, int beginy, int endx, int endy, hue c=black){
//	  	  drawline(p, beginx, beginy, endx, endy, c, plot);
//	  }
	
// forward point2d version to int version	 
//	  inline void drawline(playpen & p, point2d begin, point2d end, hue c=black){
//	  	  drawline(p, 
//	  	  int(std::floor(begin.x()+.5)), 
//	  	  int(std::floor(begin.y()+.5)), 
//	  	  int(std::floor(end.x()+0.5)), 
//	  	  int(std::floor(end.y()+0.5)), c, plot);
//	  }

// retained for backward compatibility
		

}

#endif
