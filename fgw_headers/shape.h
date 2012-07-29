#ifndef SHAPE_H
#define SHAPE_H
#include "playpen.h"
#include "point2d.h"
#include <istream>
#include <ostream>
#include <vector>



namespace fgw {
	typedef std::vector<point2d> shape;
	void filled_polygon(playpen & pp, shape const & s, point2d centre, hue shade);
	void filled_polygon(playpen & pp, shape const & s, hue shade);
	void drawshape(playpen & pp, shape const & s, hue shade);
	void moveshape(shape & s, point2d offset);
	void growshape(shape & s, double xfactor, double yfactor);
	void scaleshape(shape & s, double scalefactor);
	void rotateshape(shape & s, double rotation, point2d centre);
	void rotateshape(shape & s, double rotation);
    void sheershape(shape & s, double sheer);
	double area_of_triangle(shape s);
	shape make_regular_polygon(double radius, int n);
	shape makecircle(double radius, point2d centre);
	shape read_shape(std::istream &);
	void write_shape(shape const & s, std::ostream &);



}

#endif
