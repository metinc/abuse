#ifndef ABUSE_SDLPORT_VIDEO_MODE_H
#define ABUSE_SDLPORT_VIDEO_MODE_H

// 0 = window, 1 = borderless desktop, 2 = exclusive fullscreen.
bool video_set_fullscreen_mode(int mode);
void video_change_settings(int scale_add, bool toggle_fullscreen);

#endif
