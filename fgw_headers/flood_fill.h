#ifndef FLOOD_FILLS_H
#define FLOOD_FILLS_H

#include "playpen.h"

namespace fgw{
	void seed_fill(fgw::playpen & canvas, int i, int j, fgw::hue new_hue, fgw::hue boundary);
	void replace_hue(fgw::playpen & canvas, int i, int j, fgw::hue new_hue);


}

#endif
