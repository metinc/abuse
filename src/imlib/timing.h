/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef __TIMING_HPP_
#define __TIMING_HPP_

#include <cstdint>

class time_marker
{
  public:
    std::uint64_t ticks;
    void get_time();
    time_marker();
    double diff_time(time_marker *other); // return time diff in seconds
};

#endif
