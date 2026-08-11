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

#include <algorithm>
#include <cmath>

#include <SDL3/SDL.h>

#include "common.h"

#include "filter.h"
#include "video.h"
#include "image.h"
#include "setup.h"
#include "video_mode.h"
#include "errorui.h"

SDL_Window *window = NULL;
SDL_Surface *surface = NULL;
SDL_Surface *screen = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *game_texture = NULL;
image *main_screen = NULL;

int xres, yres;

extern palette *lastl;
extern Settings settings;

SDL_DisplayMode desktop;
int window_w = 320, window_h = 240;
bool fullscreen = false;

namespace
{
struct WindowedBounds
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool valid = false;
};

WindowedBounds windowed_bounds;

bool window_is_fullscreen()
{
    return window && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}

void remember_windowed_bounds()
{
    if (!window || window_is_fullscreen())
        return;

    windowed_bounds.valid = SDL_GetWindowPosition(window, &windowed_bounds.x, &windowed_bounds.y) &&
                            SDL_GetWindowSize(window, &windowed_bounds.w, &windowed_bounds.h);
}

bool configure_fullscreen_mode(int fullscreen_mode)
{
    // A null mode requests borderless desktop fullscreen.
    if (fullscreen_mode != 2)
    {
        if (SDL_SetWindowFullscreenMode(window, nullptr))
            return true;
        fprintf(stderr, "Video: Unable to select borderless desktop fullscreen mode: %s\n", SDL_GetError());
        return false;
    }

    const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    const SDL_DisplayMode *desktop_mode = display ? SDL_GetDesktopDisplayMode(display) : nullptr;
    SDL_DisplayMode mode{};
    if (!desktop_mode || !SDL_GetClosestFullscreenDisplayMode(display, desktop_mode->w, desktop_mode->h,
                                                              desktop_mode->refresh_rate, true, &mode))
    {
        fprintf(stderr, "Video: No exclusive fullscreen mode matching the desktop; using borderless desktop mode: %s\n",
                SDL_GetError());
        if (SDL_SetWindowFullscreenMode(window, nullptr))
            return true;
        fprintf(stderr, "Video: Unable to select fallback borderless desktop fullscreen mode: %s\n", SDL_GetError());
        return false;
    }

    if (!SDL_SetWindowFullscreenMode(window, &mode))
    {
        fprintf(stderr, "Video: Unable to select exclusive fullscreen mode %dx%d: %s\n", mode.w, mode.h,
                SDL_GetError());
        return false;
    }

    printf("Video: Exclusive fullscreen mode %dx%d at %.2f Hz\n", mode.w, mode.h, mode.refresh_rate);
    return true;
}
}

// VGA's 320x200 mode filled a 4:3 CRT, so its pixels were 5:6 rather
// than square. Aspect-ratio mode keeps those pixels at the original DPI,
// while legacy explicit resolutions remain square-pixel unless they use
// the original 8:5 framebuffer ratio.
static int display_height_for(int framebuffer_width, int framebuffer_height)
{
    constexpr int vga_storage_width = 8;
    constexpr int vga_storage_height = 5;

    const bool aspect_ratio_mode = !settings.editor && SDL_strcasecmp(settings.aspect_ratio.c_str(), "custom") != 0;
    if (aspect_ratio_mode ||
        static_cast<int64_t>(framebuffer_width) * vga_storage_height ==
            static_cast<int64_t>(framebuffer_height) * vga_storage_width)
        return (framebuffer_height * 6 + 2) / 5;

    return framebuffer_height;
}

static int display_height()
{
    return display_height_for(xres, yres);
}

//
// set_mode()
// Set the video mode
//
void set_mode(int argc, char **argv)
{
    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    desktop.w = 320;
    desktop.h = 240;

    const SDL_DisplayMode *mode = display ? SDL_GetDesktopDisplayMode(display) : nullptr;
    if (mode)
    {
        desktop = *mode;
    }
    else
    {
        printf("Failed to get desktop display mode: %s\n", SDL_GetError());
    }

    const bool create_window = window == NULL;
    const int requested_window_w = xres * scale;
    const int requested_window_h = display_height() * scale;

    if (create_window)
    {
        window_w = requested_window_w;
        window_h = requested_window_h;
        SDL_WindowFlags window_flags = 0;

        if (settings.borderless)
            window_flags |= SDL_WINDOW_BORDERLESS;

        window = SDL_CreateWindow("Abuse", window_w, window_h, window_flags);

        if (!window)
        {
            show_startup_error("Video: Unable to create window : %s", SDL_GetError());
            exit(EXIT_FAILURE);
        }

        // Set this after creation so SDL preserves the exact configured initial
        // size instead of constraining it to the compositor's recommended size.
        if (!SDL_SetWindowResizable(window, true))
            fprintf(stderr, "Video: Unable to make the window resizable: %s\n", SDL_GetError());

        // Load the window icon
        std::string tmp_name = std::string(get_filename_prefix()) + "icon.bmp";

        if (SDL_Surface *icon = SDL_LoadBMP(tmp_name.c_str()); icon != nullptr)
        {
            SDL_SetWindowIcon(window, icon);
            SDL_DestroySurface(icon);
        }
    }
    else if (!window_is_fullscreen())
    {
        int old_x, old_y, old_w, old_h;
        SDL_GetWindowPosition(window, &old_x, &old_y);
        SDL_GetWindowSize(window, &old_w, &old_h);
        SDL_SetWindowSize(window, requested_window_w, requested_window_h);
        SDL_SetWindowPosition(window, old_x + (old_w - requested_window_w) / 2,
                              old_y + (old_h - requested_window_h) / 2);
        SDL_SyncWindow(window);
        window_w = requested_window_w;
        window_h = requested_window_h;
        remember_windowed_bounds();
    }

    if (!renderer)
    {
        renderer = SDL_CreateRenderer(window, NULL);
        if (!renderer)
        {
            show_startup_error("Video: Unable to create renderer : %s", SDL_GetError());
            exit(EXIT_FAILURE);
        }

        const char *rendererName = SDL_GetRendererName(renderer);
        printf("Renderer: %s\n", rendererName);
    }

    // Set renderer flags
    SDL_SetRenderVSync(renderer, true);

    // Present the original 320x200 framebuffer through a 320x240 logical
    // canvas. SDL uniformly fits that corrected 4:3 image to the window and
    // handles letterboxing, resizing and high-DPI output for us.
    if (!SDL_SetRenderLogicalPresentation(renderer, xres, display_height(), SDL_LOGICAL_PRESENTATION_LETTERBOX))
    {
        show_startup_error("Video: Unable to configure logical presentation: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

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
    SDL_SetTextureScaleMode(game_texture, settings.linear_filter ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);

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

    if (settings.fullscreen != 0 && create_window)
        video_set_fullscreen_mode(settings.fullscreen);

    update_dirty(main_screen);
}

bool video_set_fullscreen_mode(int mode)
{
    if (!window || mode < 0 || mode > 2)
        return false;

    const bool was_fullscreen = window_is_fullscreen();
    if (mode == 0)
    {
        if (was_fullscreen)
        {
            if (!SDL_SetWindowFullscreen(window, false))
            {
                fprintf(stderr, "Video: Unable to leave fullscreen mode: %s\n", SDL_GetError());
                return false;
            }
            SDL_SyncWindow(window);
            if (!window_is_fullscreen() && windowed_bounds.valid)
            {
                SDL_SetWindowSize(window, windowed_bounds.w, windowed_bounds.h);
                SDL_SetWindowPosition(window, windowed_bounds.x, windowed_bounds.y);
                SDL_SyncWindow(window);
            }
        }
    }
    else
    {
        if (!was_fullscreen)
            remember_windowed_bounds();
        if (!configure_fullscreen_mode(mode))
            return false;
        if (!SDL_SetWindowFullscreen(window, true))
        {
            fprintf(stderr, "Video: Unable to enter fullscreen mode: %s\n", SDL_GetError());
            return false;
        }
        SDL_SyncWindow(window);
    }

    fullscreen = window_is_fullscreen();
    SDL_GetWindowSize(window, &window_w, &window_h);
    return mode == 0 ? !fullscreen : fullscreen;
}

void video_change_settings(int scale_add, bool toggle_fullscreen)
{
    if (toggle_fullscreen)
    {
        const int target_mode = window_is_fullscreen() ? 0 : (settings.fullscreen == 2 ? 2 : 1);
        if (video_set_fullscreen_mode(target_mode))
            settings.fullscreen = target_mode;
    }

    if (scale_add != 0 && !window_is_fullscreen())
    {
        // Scale window
        int new_scale = scale + scale_add;
        const int corrected_height = display_height();

        if (new_scale > 0 && xres * new_scale <= desktop.w && corrected_height * new_scale <= desktop.h)
        {
            // Scale windows if it fits on screen
            int old_x;
            int old_y;
            int old_w;
            int old_h;
            SDL_GetWindowPosition(window, &old_x, &old_y);
            SDL_GetWindowSize(window, &old_w, &old_h);

            scale = new_scale;
            const int new_w = xres * scale;
            const int new_h = corrected_height * scale;
            SDL_SetWindowSize(window, new_w, new_h);
            SDL_SetWindowPosition(window, old_x + (old_w - new_w) / 2, old_y + (old_h - new_h) / 2);
            SDL_SyncWindow(window);
            remember_windowed_bounds();
        }
    }

    // Cache the actual size accepted by the window system.
    SDL_GetWindowSize(window, &window_w, &window_h);
}

bool resize_framebuffer(int width, int height)
{
    if (!window || !renderer || !main_screen || width <= 0 || height <= 0)
        return false;
    if (width == xres && height == yres)
        return true;

    SDL_Surface *new_screen = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
    SDL_Surface *new_surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_INDEX8);
    SDL_Texture *new_texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!new_screen || !new_surface || !new_texture)
    {
        fprintf(stderr, "Video: Unable to resize framebuffer to %dx%d: %s\n", width, height, SDL_GetError());
        if (new_texture)
            SDL_DestroyTexture(new_texture);
        if (new_surface)
            SDL_DestroySurface(new_surface);
        if (new_screen)
            SDL_DestroySurface(new_screen);
        return false;
    }

    SDL_SetTextureScaleMode(new_texture, settings.linear_filter ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    const int new_display_height = display_height_for(width, height);
    if (!SDL_SetRenderLogicalPresentation(renderer, width, new_display_height, SDL_LOGICAL_PRESENTATION_LETTERBOX))
    {
        fprintf(stderr, "Video: Unable to resize logical presentation to %dx%d: %s\n", width, new_display_height,
                SDL_GetError());
        SDL_DestroyTexture(new_texture);
        SDL_DestroySurface(new_surface);
        SDL_DestroySurface(new_screen);
        return false;
    }

    SDL_DestroyTexture(game_texture);
    SDL_DestroySurface(surface);
    SDL_DestroySurface(screen);
    game_texture = new_texture;
    surface = new_surface;
    screen = new_screen;
    xres = width;
    yres = height;
    main_screen->SetSize(ivec2(width, height));
    main_screen->clear();

    if (!window_is_fullscreen())
    {
        int old_x, old_y, old_w, old_h;
        SDL_GetWindowPosition(window, &old_x, &old_y);
        SDL_GetWindowSize(window, &old_w, &old_h);
        const int new_w = width * scale;
        const int new_h = new_display_height * scale;
        SDL_SetWindowSize(window, new_w, new_h);
        SDL_SetWindowPosition(window, old_x + (old_w - new_w) / 2, old_y + (old_h - new_h) / 2);
        SDL_SyncWindow(window);
        remember_windowed_bounds();
    }

    SDL_GetWindowSize(window, &window_w, &window_h);
    return true;
}

//
// close_graphics()
// Shutdown the video mode
//
void close_framebuffer()
{
    if (surface)
        SDL_DestroySurface(surface);
    if (screen)
        SDL_DestroySurface(screen);
    if (game_texture)
        SDL_DestroyTexture(game_texture);
    surface = NULL;
    screen = NULL;
    game_texture = NULL;

    delete main_screen;
    main_screen = NULL;
}

void close_graphics()
{
    if (lastl)
        delete lastl;
    lastl = NULL;

    close_framebuffer();

    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    renderer = NULL;
    window = NULL;

    fullscreen = false;
    windowed_bounds = {};
}

// put_part_image()
// Draw only dirty parts of the image
//
void put_part_image(image *im, int x, int y, int x1, int y1, int x2, int y2)
{
    int xe, ye;

    if (y > yres || x > xres)
        return;

    CHECK(x1 >= 0 && x2 >= x1 && y1 >= 0 && y2 >= y1);

    // Adjust if we are trying to draw off the screen
    if (x < 0)
    {
        x1 += -x;
        x = 0;
    }
    if (x + (x2 - x1) >= xres)
        xe = xres - x + x1 - 1;
    else
        xe = x2;

    if (y < 0)
    {
        y1 += -y;
        y = 0;
    }
    if (y + (y2 - y1) >= yres)
        ye = yres - y + y1 - 1;
    else
        ye = y2;

    if (x1 >= xe || y1 >= ye)
        return;

    const int copy_width = xe - x1;
    const int copy_height = ye - y1;

    // Lock the surface if necessary
    if (SDL_MUSTLOCK(surface))
        SDL_LockSurface(surface);

    // SDL may pad rows for alignment. Advancing by surface->w only works
    // when the framebuffer width happens to equal its allocated pitch.
    Uint8 *dpixel = static_cast<Uint8 *>(surface->pixels) + y * surface->pitch + x;
    for (int row = 0; row < copy_height; row++)
    {
        memcpy(dpixel, im->scan_line(y1 + row) + x1, copy_width);
        dpixel += surface->pitch;
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
    const double inverse_gamma = 1.0 / std::clamp(settings.gamma, 0.5, 2.0);
    auto apply_gamma = [inverse_gamma](unsigned int channel) {
        return static_cast<Uint8>(std::lround(std::pow(channel / 255.0, inverse_gamma) * 255.0));
    };

    for (int ii = 0; ii < ncolors; ii++)
    {
        colors[ii].r = apply_gamma(red(ii));
        colors[ii].g = apply_gamma(green(ii));
        colors[ii].b = apply_gamma(blue(ii));
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
    // The SDL3 unchecked blitter requires non-null, pre-clipped rectangles.
    SDL_BlitSurface(surface, NULL, screen, NULL);

    // Update the SDL texture with our pixel data
    SDL_UpdateTexture(game_texture, NULL, screen->pixels, screen->pitch);

    // Clear renderer
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    const SDL_FRect dest_rect = {0.0f, 0.0f, static_cast<float>(xres), static_cast<float>(display_height())};
    SDL_RenderTexture(renderer, game_texture, NULL, &dest_rect);

    // Present the renderer
    SDL_RenderPresent(renderer);
}
