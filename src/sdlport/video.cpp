/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 2001 Anthony Kruize <trandor@labyrinth.net.au>
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *	Copyright (c) 2016 Antonio Radojkovic <antonior.software@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 */

#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include <windows.h>
#endif

#include <SDL3/SDL.h>

#include "common.h"

#include "filter.h"
#include "video.h"
#include "image.h"
#include "setup.h"
#include "errorui.h"

SDL_Window *window = NULL;
SDL_Surface *surface = NULL;
SDL_Surface *screen = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *game_texture = NULL;
image *main_screen = NULL;

int mouse_xpad, mouse_ypad, mouse_xscale, mouse_yscale;
int xres, yres;

extern palette *lastl;
extern Settings settings;

void calculate_mouse_scaling();

SDL_DisplayMode desktop;
int window_w = 320, window_h = 200;
bool ar_fullscreen = false;
int ogl_scale = 1;
int ogl_w = 320, ogl_h = 200;

void video_change_settings(int scale_add, bool toggle_fullscreen);

//
// set_mode()
// Set the video mode
//
void set_mode(int argc, char **argv)
{
    int displayIndex = 0;
    desktop.w = 320;
    desktop.h = 200;

    const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode(displayIndex);
    if (mode)
    {
        desktop = *mode;
    }
    else
    {
        printf("Failed to get desktop display mode: %s\n", SDL_GetError());
    }

    // Scale window
    window_w = xres * scale;
    window_h = yres * scale;

    // Fullscreen "scale"
    ogl_w = window_w;
    ogl_h = window_h;

    SDL_WindowFlags window_flags = 0;

    // if (settings.fullscreen == 1)
    //     window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    // else
    if (settings.fullscreen == 2)
        window_flags |= SDL_WINDOW_FULLSCREEN;

    if (settings.borderless)
        window_flags |= SDL_WINDOW_BORDERLESS;

    window = SDL_CreateWindow("Abuse", window_w, window_h, window_flags);

    if (!window)
    {
        show_startup_error("Video: Unable to create window : %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    // Load the window icon
    std::string tmp_name = std::string(get_filename_prefix()) + "icon.bmp";

    if (SDL_Surface *icon = SDL_LoadBMP(tmp_name.c_str()); icon != nullptr)
    {
        SDL_SetWindowIcon(window, icon);
        SDL_DestroySurface(icon);
    }

    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer)
    {
        show_startup_error("Video: Unable to create renderer : %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    // Set renderer flags
    SDL_SetRenderVSync(renderer, true);

    const char *rendererName = SDL_GetRendererName(renderer);
    printf("Renderer: %s\n", rendererName);

    // Set renderer scaling quality
    SDL_SetHint("SDL_RENDER_SCALE_QUALITY", settings.linear_filter == 1 ? "linear" : "nearest");

    // Create our 32-bit surface for texture conversion
    screen = SDL_CreateSurface(xres, yres, SDL_PIXELFORMAT_RGBA32);
    if (screen == NULL)
    {
        show_startup_error("Video: Unable to create 32-bit surface: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    // Create texture for rendering
    game_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, xres, yres);
    if (game_texture == NULL)
    {
        show_startup_error("Video: Unable to create texture: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    // Create our 8-bit surface
    surface = SDL_CreateSurface(xres, yres, SDL_PIXELFORMAT_INDEX8);
    if (surface == NULL)
    {
        // Our surface is no good, we have to bail.
        show_startup_error("Video: Unable to create 8-bit surface: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    // Create the screen image
    main_screen = new image(ivec2(xres, yres), NULL, 2);
    if (main_screen == NULL)
    {
        // Our screen image is no good, we have to bail.
        show_startup_error("Video: Unable to create screen image.");
        exit(EXIT_FAILURE);
    }
    main_screen->clear();

    // Hide the mouse cursor and set up the mouse
    if (settings.grab_input)
        SDL_SetWindowMouseGrab(window, true);
    SDL_HideCursor();
    calculate_mouse_scaling();

    if (settings.fullscreen != 0)
        video_change_settings(0, true);

    update_dirty(main_screen);
}

void video_change_settings(int scale_add, bool toggle_fullscreen)
{
    if (toggle_fullscreen)
    {
        ar_fullscreen = !ar_fullscreen;
        if (ar_fullscreen)
        {
            SDL_SetWindowFullscreen(window, true);
            SDL_GetWindowSize(window, &window_w, &window_h);

            // Texture rendering size, scale while if fits
            int scl = 1;
            while (1 != 0)
                if (xres * scl < desktop.w && yres * scl < desktop.h)
                {
                    ogl_scale = scl;
                    ogl_w = xres * ogl_scale;
                    ogl_h = yres * ogl_scale;
                    scl++;
                }
                else
                    break;
        }
        else
            SDL_SetWindowFullscreen(window, false);
    }

    static int overscale = 0;

    if (!ar_fullscreen)
    {
        // Scale window
        int new_scale = scale + scale_add;

        if (new_scale > 0 && xres * new_scale <= desktop.w && yres * new_scale <= desktop.h)
        {
            // Scale windows if it fits on screen
            scale = new_scale;
            SDL_SetWindowSize(window, xres * scale, yres * scale);
        }
    }
    else
    {
        // Scale texture rendering size
        int new_scale = ogl_scale + scale_add;

        if (overscale == 2 && scale_add == -1)
            overscale = 0;

        if (new_scale > 0)
        {
            if (xres * new_scale >= desktop.w || yres * new_scale >= desktop.h)
            {
                if (overscale == 0)
                {
                    if (yres * ((float)desktop.w / xres) < desktop.h)
                    {
                        // Limit scale by monitor width
                        ogl_w = desktop.w;
                        ogl_h = yres * ((float)desktop.w / xres);
                    }
                    else
                    {
                        // Limit scale by monitor height
                        ogl_w = xres * ((float)desktop.h / yres);
                        ogl_h = desktop.h;
                    }
                    overscale = 1;
                    ogl_scale = new_scale;
                }
                else if (overscale == 1)
                {
                    // Match screen to desktop/monitor size
                    ogl_w = desktop.w;
                    ogl_h = desktop.h;

                    overscale = 2;
                    ogl_scale = new_scale;
                }
            }
            else if (xres * new_scale <= desktop.w && yres * new_scale <= desktop.h)
            {
                // Scale and keep aspect
                ogl_scale = new_scale;
                ogl_w = xres * ogl_scale;
                ogl_h = yres * ogl_scale;
                overscale = 0;
            }
        }
    }

    // Update size and position
    SDL_GetWindowSize(window, &window_w, &window_h);
    SDL_SetWindowPosition(window, desktop.w / 2 - window_w / 2, desktop.h / 2 - window_h / 2);

    calculate_mouse_scaling();
}

void calculate_mouse_scaling()
{
    if (!ar_fullscreen || settings.mouse_scale == 0)
    {
        // We need to determine the appropriate mouse scaling
        float scale_x = window_w / xres;
        float scale_y = window_h / yres;

        // Re-calculate the mouse scaling
        mouse_xscale = (window_w << 16) / xres;
        mouse_yscale = (window_h << 16) / yres;

        // And calculate the padding
        mouse_xpad = scale_x;
        mouse_ypad = scale_y;
    }
    else
    {
        // We need to determine the appropriate mouse scaling
        float scale_x = ogl_w / xres;
        float scale_y = ogl_h / yres;

        // Re-calculate the mouse scaling
        mouse_xscale = (ogl_w << 16) / xres;
        mouse_yscale = (ogl_h << 16) / yres;

        // And calculate the padding
        mouse_xpad = scale_x;
        mouse_ypad = scale_y;
    }
}

//
// close_graphics()
// Shutdown the video mode
//
void close_graphics()
{
    if (lastl)
        delete lastl;
    lastl = NULL;

    // Free our surfaces, texture and renderer
    if (surface)
        SDL_DestroySurface(surface);
    if (screen)
        SDL_DestroySurface(screen);
    if (game_texture)
        SDL_DestroyTexture(game_texture);
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);

    delete main_screen;
}

// put_part_image()
// Draw only dirty parts of the image
//
void put_part_image(image *im, int x, int y, int x1, int y1, int x2, int y2)
{
    int xe, ye;
    SDL_Rect srcrect, dstrect;
    int ii, jj;
    int srcx, srcy, xstep, ystep;
    Uint8 *dpixel;
    Uint16 dinset;

    if (y > yres || x > xres)
        return;

    CHECK(x1 >= 0 && x2 >= x1 && y1 >= 0 && y2 >= y1);

    // Adjust if we are trying to draw off the screen
    if (x < 0)
    {
        x1 += -x;
        x = 0;
    }
    srcrect.x = x1;
    if (x + (x2 - x1) >= xres)
        xe = xres - x + x1 - 1;
    else
        xe = x2;

    if (y < 0)
    {
        y1 += -y;
        y = 0;
    }
    srcrect.y = y1;
    if (y + (y2 - y1) >= yres)
        ye = yres - y + y1 - 1;
    else
        ye = y2;

    if (srcrect.x >= xe || srcrect.y >= ye)
        return;

    // Scale the image onto the surface
    srcrect.w = xe - srcrect.x;
    srcrect.h = ye - srcrect.y;
    dstrect.x = x;
    dstrect.y = y;
    dstrect.w = srcrect.w;
    dstrect.h = srcrect.h;

    xstep = (srcrect.w << 16) / dstrect.w;
    ystep = (srcrect.h << 16) / dstrect.h;

    srcy = ((srcrect.y) << 16);
    dinset = ((surface->w - dstrect.w)) * SDL_BYTESPERPIXEL(surface->format);

    // Lock the surface if necessary
    if (SDL_MUSTLOCK(surface))
        SDL_LockSurface(surface);

    dpixel = (Uint8 *)surface->pixels;
    dpixel += (dstrect.x + ((dstrect.y) * surface->w)) * SDL_BYTESPERPIXEL(surface->format);

    // Update surface part
    srcy = srcrect.y;
    dpixel = ((Uint8 *)surface->pixels) + y * surface->w + x;
    for (ii = 0; ii < srcrect.h; ii++)
    {
        memcpy(dpixel, im->scan_line(srcy) + srcrect.x, srcrect.w);
        dpixel += surface->w;
        srcy++;
    }

    // Unlock the surface if we locked it.
    if (SDL_MUSTLOCK(surface))
        SDL_UnlockSurface(surface);
}

//
// load()
// Set the palette
//
void palette::load()
{
    if (lastl)
        delete lastl;
    lastl = copy();

    // Force to only 256 colours.
    // Shouldn't be needed, but best to be safe.
    if (ncolors > 256)
        ncolors = 256;

#ifdef WIN32
    // FIXME: Really, this applies to anything that doesn't allow dynamic stack allocation
    SDL_Color colors[256];
#else
    SDL_Color colors[ncolors];
#endif
    for (int ii = 0; ii < ncolors; ii++)
    {
        colors[ii].r = red(ii);
        colors[ii].g = green(ii);
        colors[ii].b = blue(ii);
        colors[ii].a = 255;
    }
    SDL_Palette *palette = SDL_CreateSurfacePalette(surface);
    SDL_SetPaletteColors(palette, colors, 0, ncolors);

    // Now redraw the surface
    update_window_done();
}

//
// load_nice()
//
void palette::load_nice()
{
    load();
}

// ---- support functions ----

void update_window_done()
{
    // Convert 8-bit surface to 32-bit
    SDL_BlitSurfaceUnchecked(surface, NULL, screen, NULL);

    // Update the SDL texture with our pixel data
    SDL_UpdateTexture(game_texture, NULL, screen->pixels, screen->pitch);

    // Clear renderer
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (ar_fullscreen)
    {
        // Center the texture with proper aspect ratio
        SDL_FRect dest_rect;
        dest_rect.x = (window_w - ogl_w) / 2;
        dest_rect.y = (window_h - ogl_h) / 2;
        dest_rect.w = ogl_w;
        dest_rect.h = ogl_h;
        SDL_RenderTexture(renderer, game_texture, NULL, &dest_rect);
    }
    else
    {
        // Fill the window with the texture
        SDL_RenderTexture(renderer, game_texture, NULL, NULL);
    }

    // Present the renderer
    SDL_RenderPresent(renderer);
}