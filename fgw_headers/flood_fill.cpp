#include "playpen.h"
#include <stack>

using namespace std;

namespace fgw {
	namespace {
		struct point_ij{
			int i;
			int j;
		};
	}
	void seed_fill(fgw::playpen & canvas, int i, int j, fgw::hue new_shade, fgw::hue boundary){
	// convert to internal representation	
		playpen::raw_pixel_data const raw_seed(canvas.get_raw_xy(i,j));
		if(raw_seed.x() > Xpixels or raw_seed.x() < 0 or raw_seed.y() > Ypixels or raw_seed.y() < 0){
			cerr << "Seed for fill outside Playpen. Nothing to do.\n"
				 << "Did you forget the scale is " << canvas.scale() << "?\n";
			return;
		}
		point_ij seed;
		seed.i = raw_seed.x();
		seed.j = raw_seed.y();
		std::stack<point_ij> pixel_stack;  
		pixel_stack.push(seed);
		while(not pixel_stack.empty()){
			point_ij pixel(pixel_stack.top());
			canvas.setrawpixel(pixel.i, pixel.j, new_shade);
	// go left
			--pixel.i;
			while(pixel.i >= 0  and canvas.getrawpixel(pixel.i, pixel.j) != boundary){
					canvas.setrawpixel(pixel.i, pixel.j, new_shade);
					--pixel.i;
			}
			int extreme_left(pixel.i + 1);
			pixel.i = pixel_stack.top().i + 1;
			pixel_stack.pop();
	// go right	
			while(pixel.i < Xpixels and canvas.getrawpixel(pixel.i, pixel.j) != boundary){
					canvas.setrawpixel(pixel.i, pixel.j, new_shade);
					++pixel.i;
			}
			int extreme_right(pixel.i - 1);
	// scan row above
			point_ij save(pixel);
			--pixel.j;
		    if (pixel.j > 0){
		      bool previous_pixel_is_border(true);
		      for(pixel.i = extreme_right; pixel.i >= extreme_left; --pixel.i){
		        if(previous_pixel_is_border and canvas.getrawpixel(pixel.i, pixel.j) != boundary 
					and canvas.getrawpixel(pixel.i, pixel.j) != new_shade){
		          pixel_stack.push(pixel);
		          previous_pixel_is_border = false;
		        }
		        else {
			        if(canvas.getrawpixel(pixel.i, pixel.j) == boundary or canvas.getrawpixel(pixel.i, pixel.j) == new_shade)
			          previous_pixel_is_border = true;
				}
		      }
		    }
	// scan below 
			pixel = save;        
		    ++pixel.j;
			if (pixel.j < Ypixels -1){
		      bool previous_pixel_is_border = true;
			  for(pixel.i = extreme_right; pixel.i >= extreme_left; --pixel.i){
		        if (previous_pixel_is_border and canvas.getrawpixel(pixel.i, pixel.j) != boundary 
											and canvas.getrawpixel(pixel.i, pixel.j) != new_shade){
		          pixel_stack.push(pixel);
		          previous_pixel_is_border = false;
		        }
		        else {
			        if (canvas.getrawpixel(pixel.i, pixel.j) == boundary or canvas.getrawpixel(pixel.i, pixel.j) == new_shade)
			          previous_pixel_is_border = true;
				}
			  }
			}
		}
	}            
	// this one replces colour of seed with a new colour and then
	// fill applies that to adjecent pixels as long as they are the same colour
	// as the seed pixel was.
	
	void replace_hue(fgw::playpen & canvas, int i, int j, fgw::hue new_shade){
		playpen::raw_pixel_data const raw_seed(canvas.get_raw_xy(i,j));
		point_ij seed;
		if(raw_seed.x() > Xpixels or raw_seed.x() < 0 or raw_seed.y() > Ypixels or raw_seed.y() < 0){
			cerr << "Seed for fill outside Playpen. Nothing to do.\n"
				 << "Did you forget the scale is " << canvas.scale() << "?\n";
			return;
		}
		seed.i = raw_seed.x();
		seed.j = raw_seed.y();
		hue old_shade(canvas.getrawpixel(seed.i, seed.j));
		std::stack<point_ij> pixel_stack;  
		pixel_stack.push(seed);
		while(not pixel_stack.empty()){
			point_ij pixel(pixel_stack.top());
			canvas.setrawpixel(pixel.i, pixel.j, new_shade);
	// go left
			--pixel.i;
			while(pixel.i >= 0  and canvas.getrawpixel(pixel.i, pixel.j) == old_shade){
					canvas.setrawpixel(pixel.i, pixel.j, new_shade);
					--pixel.i;
			}
			int extreme_left(pixel.i + 1);
			pixel.i = pixel_stack.top().i + 1;
			pixel_stack.pop();
	// go right	
			while(pixel.i < Xpixels and canvas.getrawpixel(pixel.i, pixel.j) == old_shade){
					canvas.setrawpixel(pixel.i, pixel.j, new_shade);
					++pixel.i;
			}
			int extreme_right(pixel.i - 1);
	// scan row above
			point_ij save(pixel);
			--pixel.j;
		    if (pixel.j > 0){
		      bool previous_pixel_is_border(true);
		      for(pixel.i = extreme_right; pixel.i >= extreme_left; --pixel.i){
		        if(previous_pixel_is_border and canvas.getrawpixel(pixel.i, pixel.j)== old_shade){
		          pixel_stack.push(pixel);
		          previous_pixel_is_border = false;
		        }
		        else {
			        if(canvas.getrawpixel(pixel.i, pixel.j) != old_shade)
			          previous_pixel_is_border = true;
				}
		      }
		    }
	// scan below 
			pixel = save;        
		    ++pixel.j;
			if (pixel.j < Ypixels -1){
		      bool previous_pixel_is_border = true;
			  for(pixel.i = extreme_right; pixel.i >= extreme_left; --pixel.i){
		        if (previous_pixel_is_border and canvas.getrawpixel(pixel.i, pixel.j) == old_shade){
		          pixel_stack.push(pixel);
		          previous_pixel_is_border = false;
		        }
		        else {
			        if (canvas.getrawpixel(pixel.i, pixel.j) != old_shade)
			          previous_pixel_is_border = true;
				}
			  }
			}
		}
	}            
}

