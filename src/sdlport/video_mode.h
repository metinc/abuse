#ifndef ABUSE_SDLPORT_VIDEO_MODE_H
#define ABUSE_SDLPORT_VIDEO_MODE_H

bool video_set_fullscreen(bool enabled);
void video_change_settings(int scale_add, bool toggle_fullscreen);
void video_update_mouse_confinement();

#endif
