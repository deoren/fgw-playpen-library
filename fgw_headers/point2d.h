#ifndef POINT2D_H
#define POINT2D_H
#include "fgw_text.h"

// ISOLATED FROM GPAHICS.H 29/05/03

namespace fgw {

	class point2d {
	public:
	// constructor:
		explicit point2d(double xval=0, double yval=0);
	// read access functions
		double x()const;
		double y()const;
		double modulus()const;
		double argument()const;
	// write access functions
		point2d & x(double xval);
		point2d & y(double yval);
		point2d & modulus(double mod);
		point2d & argument(double degrees);
	private:
		double x_;
		double y_;

	};
// specific point2d io functions
	point2d getpoint2d(std::istream & inp);
	point2d getpoint2d();  // get from cin
	std::ostream& send_to(fgw::point2d, std::ostream& output);
	inline std::ostream& operator<<(std::ostream& out, fgw::point2d const & pt){ return send_to(pt, out);}
	inline std::istream& operator>>(std::istream& in, fgw::point2d & pt){
		if(&in == &std::cin) pt = getpoint2d();
		else pt = getpoint2d(in); 
		return in;
	}	              	  
      
}

#endif
