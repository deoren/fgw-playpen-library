#include "flood_fill.h"
#include "line_drawing.h"
#include "point2dx.h"
#include "shape.h"
#include <algorithm>

   namespace fgw{
   
   // utility functions placed in anon namespace
   // public functions
   
       void drawshape(playpen & pp, shape const & s, hue shade){
         for(unsigned int i=0; i != (s.size()-1); ++i){
            drawline(pp, s[i], s[i+1], shade);
         }
      }
   
       void moveshape(shape & s, point2d offset){
         for(unsigned int i=0; i != s.size(); ++i){
            s[i].x(s[i].x() + offset.x());
            s[i].y(s[i].y() + offset.y());
         }
      }
   
       void growshape(shape & s, double xfactor, double yfactor){
         for(unsigned int i=0; i != s.size(); ++i){
            s[i].x(s[i].x() * xfactor);
            s[i].y(s[i].y() * yfactor);
         }
      }
   
       void scaleshape(shape & s, double scalefactor){
         growshape(s, scalefactor, scalefactor);
      }
   
       void rotateshape(shape & s, double rotation){
         for(unsigned int i=0; i != s.size(); ++i){
            s[i].argument(s[i].argument() + rotation);
         }
      }
   
       void rotateshape(shape & s, double rotation, point2d centre){
         moveshape(s, centre);
         rotateshape(s, rotation);
         moveshape(s, point2d(-centre.x(), -centre.y()));
      }
   
   
       void sheershape(shape & s, double sheer){
         for(unsigned int i=0; i != s.size(); ++i){
            s[i].x(s[i].x() + s[i].y()*sheer);
         }
      }
   
       double area_of_triangle(shape s){
      // if too few points for a triangle make the area 0
         if(s.size() < 3) 
            return 0.0;
      // However for shapes with too many vertices, 
      // just use the first three	   	   
         point2d side1;
         point2d side2;
         side1.x(s[1].x()-s[0].x());
         side1.y(s[1].y()-s[0].y());
         side2.x(s[2].x()-s[0].x());
         side2.y(s[2].y()-s[0].y());
         double area;
         area = (side1.x() * side2.y() - side1.y() * side2.x())/2;
         return area;
      }
   
       shape make_regular_polygon(double radius, int n){
         shape polygon;
         if(n>0) {
            double angle(360.0/n);
            point2d vertex(radius, 0);
            for(int i=0; i != n; ++i){
               polygon.push_back(vertex.argument(i*angle));
            }
         // close the polygon
            polygon.push_back(polygon[0]);
         }
         return polygon;
      }
   
       shape makecircle(double radius, point2d centre){
         shape circle(make_regular_polygon(radius, int(radius * 2.1)));
      // now we have a circle centred at the origin
         moveshape(circle, centre);
      // move it where we want it
         return circle;
      }
   
   
     
       void filled_polygon(playpen & pp, shape const & s, point2d local, hue shade){
         if(s.size() < 3) 
            return;
         drawshape(pp, s, shade);
         seed_fill(pp, int(local.x()), int(local.y()), shade, shade);
      }
   
   // assumes that the mean of the verteces gives an interior point
       void filled_polygon(playpen & pp, shape const & s, hue shade){
         if(s.size() < 4) 
            return; // nothing to do
         double x_mean(0);
         double y_mean(0);
      // In calculating the mean I have allowed for my representation of
      // closed shapes having the last point being a repeat of the first
         for(unsigned int i = 0; i != s.size()-1; ++i){
            x_mean += s[i].x();
            y_mean += s[i].y();
         }
         x_mean /= s.size();
         y_mean /= s.size();
         filled_polygon(pp, s, point2d(x_mean, y_mean), shade);
      }	   
   
       shape read_shape(istream& in){
         shape local;
         int const count(read<int>(in));
         for(int i(0); i != count; ++i){
            point2d const point(read<point2d>(in));
            local.push_back(point);
         }
         return local;
      }
   
       void write_shape(shape const & s, std::ostream&  out){
         out << s.size() << '\n';
         for(unsigned int i(0); i != s.size(); ++i){
            out << s[i] << '\n';
         }
      }
       	   
   
   
   }	 
