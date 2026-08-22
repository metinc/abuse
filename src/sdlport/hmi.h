//
//  Abuse - dark 2D side-scrolling platform game
//
//  Copyright (c) 2011 Jochen Schleu <jjs@jjs.at>
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the Do What The Fuck You Want To
//   Public License, Version 2, as published by Sam Hocevar. See
//   http://sam.zoy.org/projects/COPYING.WTFPL for more details.
//

#ifndef __HMI_HPP_
#define __HMI_HPP_

#include <cstdint>
#include <vector>

std::vector<uint8_t> load_hmi_as_midi(char const *filename);
bool convert_hmi_to_midi_file(char const *input_filename, char const *output_filename);

#endif
