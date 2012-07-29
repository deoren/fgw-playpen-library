#include "line_drawing.h"

#include <iostream>
#include <string>



   using namespace std;


   namespace fgw {
   
   // drawline with a policy based plot method. For detailed comments see non-policy version
   // in graphics.cpp
   
       void drawline(playpen & p, int begin_x, int begin_y, int end_x, int end_y, hue shade, plot_policy plotter){
      // caculate the deltas for x and y
         long delta_x = end_x-begin_x;
         long delta_y = end_y-begin_y;
      // deal with special cases by delegation
         if(delta_x==0) 
            return vertical_line(p, begin_x, begin_y, end_y, shade, plotter);
         if(delta_y==0) 
            return horizontal_line(p, begin_y, begin_x, end_x, shade, plotter);
      // allow for plotting in all directions 
         int x_sign = 1;
         int y_sign = 1;
         if(delta_x<0) {x_sign = -1; delta_x = -delta_x;}
         if(delta_y<0) {y_sign = -1; delta_y = -delta_y;}
      // scale deltas to low 16-bits -- high 16 (or more) represent pixel co-ordinates
         while (delta_x > 65535 || delta_y > 65535){	  
            delta_x >>= 1;	 	 //effectively divide by 2
            delta_y >>= 1;
         }
      // now prepare to step through from start to finish
         int next_x(begin_x);
         int next_y(begin_y);
         long xaccum = 32767;
         long yaccum = 32767;
      // set accumulators to half a pixel each, makes round behaviour correct
         while(next_x != end_x || next_y != end_y){
         // as long as the end point is not straight across or up from current point
            xaccum &= 0XFFFF;
            yaccum &= 0XFFFF;
         // mask the high bits of the two accumulators and plot the current point			
            plotter(p, next_x, next_y, shade);
            bool is_new_pixel = false;
         // set flag and repeatedly increment both accumulators	  	  	  
            while(not is_new_pixel){
               xaccum += delta_x;
               yaccum += delta_y;
            // till one or both 'overflow', then adjust for next pixel	  	  	  	  
               if(xaccum>65535){next_x += x_sign; is_new_pixel = true;}
               if(yaccum>65535){next_y += y_sign; is_new_pixel = true;}
            } 
         }
      // finally finish the line with one or other of the special case functions	  	  
         if(next_x == end_x) vertical_line(p, next_x, next_y, end_y, shade, plotter);
         else horizontal_line(p, next_y, next_x, end_x, shade, plotter);
      }
   	
   	
       void vertical_line(playpen & p, int xval, int y1, int y2, hue shade, plot_policy plotter){
         if (y1 < y2) 
            for(int i(y1); i != y2 ; ++i) plotter(p, xval, i, shade);
         else 
            for(int i(y1); i != y2; --i) plotter(p, xval, i, shade);
      }
   
       void horizontal_line(playpen & p, int yval,int x1, int x2, hue shade, plot_policy plotter){
         if (x2 < x1)
            for(int i(x1); i != x2; --i)plotter(p, i, yval, shade);
         else 
            for(int i(x1); i != x2; ++i)plotter(p, i, yval, shade);
      }
   
   
   }
