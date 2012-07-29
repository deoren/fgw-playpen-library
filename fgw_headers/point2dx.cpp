#include "point2dx.h"
#include <cmath>

namespace fgw {
	void plot(playpen & pp, point2d pt,hue shade){
		pp.plot(pt.x(), pt.y(),shade);
	}	 
		

	double length(point2d pt1, point2d pt2){
		double x(pt1.x()-pt2.x());
		double y(pt1.y()-pt2.y());
		return std::sqrt(x*x + y*y);
	}	 

	double direction(point2d location, point2d remote){
		// shift both points equally so the location becomes the origin
		point2d temp(remote.x()-location.x(), remote.y()-location.y());
		return temp.argument();
	}
	
}
