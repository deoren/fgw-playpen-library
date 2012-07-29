#ifndef POINT2DX_H
#define POINT2DX_H

#include <iostream>
#include "playpen.h"
#include "point2d.h"


namespace fgw{
	void plot(playpen & pp, point2d pt, hue palettecode);
	double length(point2d pt1, point2d pt2);
	double direction(point2d location, point2d remote);
}


#endif



