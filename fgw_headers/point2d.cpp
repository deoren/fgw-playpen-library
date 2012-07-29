#include "fgw_text.h"
#include "point2d.h"

#include <cmath>

using namespace std;

namespace fgw {

	point2d::point2d(double xval, double yval): x_(xval), y_(yval) {}
	double point2d::x()const { return x_;}
	double point2d::y()const { return y_;}
	point2d & point2d::x(double xval){
		x_ = xval;
		return *this;
	}
	point2d & point2d::y(double yval){
		y_ = yval;
		return *this;
	}
	double point2d::modulus() const { return std::sqrt(x_*x_ + y_*y_); }
	
	point2d & point2d::modulus(double newmod) {
		double const oldmod(modulus());
		double const scale(newmod/oldmod);
		x_ *= scale;
		y_ *= scale;
		return *this;
	}
	
	double point2d::argument() const {
		return degrees(atan2(y_, x_));   
	}
		
	
	point2d & point2d::argument(double newarg){
		double const mod(modulus());
		x_ = mod * std::cos(fgw::radians(newarg));
		y_ = mod * std::sin(fgw::radians(newarg));
		return *this;
	}



	point2d getpoint2d(istream & inp) {
		if(not match(inp, '(')) throw bad_input("Failed to find opening paren in getpoint2d.\n");
		double const x(read<double>(inp));
		if(not match(inp, ',')) throw bad_input("Failed to find comma separator in getpoint2d.\n");
		double const y(read<double>(inp));
		if(not match(inp, ')')) throw bad_input("Failed to find closing paren in getpoint2d.\n");
		return point2d(x, y);
	}
	
	point2d getpoint2d() {
		double const x(read<double>("x: "));
		double const y(read<double>("y: "));
		return point2d(x, y);
	}
	ostream & send_to(point2d pt, ostream & out){
		out << '(' << pt.x() << ", " << pt.y() << ')';
		return out;
	}
	
}
