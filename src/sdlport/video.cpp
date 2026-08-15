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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

#include <SDL3/SDL.h>

#include "common.h"

#include "filter.h"
#include "video.h"
#include "image.h"
#include "setup.h"
#include "errorui.h"

image *main_screen = nullptr;

int xres, yres;

extern palette *lastl;
extern Settings settings;

namespace
{
SDL_Window *window = nullptr;
SDL_Surface *surface = nullptr;
SDL_Renderer *renderer = nullptr;
SDL_Texture *game_texture = nullptr;

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

bool current_display_size(int &width, int &height)
{
    const SDL_DisplayID display = window ? SDL_GetDisplayForWindow(window) : SDL_GetPrimaryDisplay();
    const SDL_DisplayMode *mode = display ? SDL_GetDesktopDisplayMode(display) : nullptr;
    if (!mode)
        return false;

    width = mode->w;
    height = mode->h;
    return width > 0 && height > 0;
}

}

static void put_part_image(image *im, int x, int y, int x1, int y1, int x2, int y2);
static void put_image(image *im, int x, int y);
static void update_window_done();

// VGA's 320x200 mode filled a 4:3 CRT, so its pixels were 5:6 rather
// than square. Widescreen mode keeps those pixels at the original DPI,
// while legacy explicit resolutions remain square-pixel unless they use
// the original 8:5 framebuffer ratio.
static int display_height_for(int framebuffer_width, int framebuffer_height)
{
    constexpr int vga_storage_width = 8;
    constexpr int vga_storage_height = 5;

    if (settings.widescreen_support || static_cast<int64_t>(framebuffer_width) * vga_storage_height ==
                                           static_cast<int64_t>(framebuffer_height) * vga_storage_width)
        return (framebuffer_height * 6 + 2) / 5;

    return framebuffer_height;
}

static int display_height()
{
    return display_height_for(xres, yres);
}

SDL_Window *video_window()
{
    return window;
}

//
// set_mode()
// Set the video mode
//
void set_mode()
{
    const bool create_window = window == nullptr;
    const int requested_window_w = xres * settings.scale;
    const int requested_window_h = display_height() * settings.scale;

    if (create_window)
    {
        SDL_WindowFlags window_flags = 0;

        if (settings.borderless)
            window_flags |= SDL_WINDOW_BORDERLESS;

        window = SDL_CreateWindow("Abuse", requested_window_w, requested_window_h, window_flags);

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
        remember_windowed_bounds();
    }

    if (!renderer)
    {
        renderer = SDL_CreateRenderer(window, nullptr);
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

    // Create texture for rendering
    game_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, xres, yres);
    if (game_texture == nullptr)
    {
        show_startup_error("Video: Unable to create texture: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    SDL_SetTextureScaleMode(game_texture, settings.linear_filter ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);

    // The software renderer draws directly into this 8-bit image. Let SDL
    // wrap the same pixels for palette conversion instead of maintaining a
    // second copy of the framebuffer.
    main_screen = new image(ivec2(xres, yres), nullptr, 2);
    surface = SDL_CreateSurfaceFrom(xres, yres, SDL_PIXELFORMAT_INDEX8, main_screen->scan_line(0), xres);
    if (surface == nullptr)
    {
        show_startup_error("Video: Unable to create 8-bit surface: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    main_screen->clear();

    // Hide the mouse cursor and set up the mouse
    SDL_HideCursor();

    if (settings.fullscreen && create_window)
        video_set_fullscreen(true);

    video_update_mouse_confinement();
}

void video_update_mouse_confinement()
{
    if (!window)
        return;

    if (!SDL_SetWindowMouseGrab(window, settings.grab_input))
        fprintf(stderr, "Video: Unable to change mouse grab: %s\n", SDL_GetError());

    if (!renderer || (!settings.grab_input && !window_is_fullscreen()))
    {
        if (!SDL_SetWindowMouseRect(window, nullptr))
            fprintf(stderr, "Video: Unable to remove mouse confinement: %s\n", SDL_GetError());
        return;
    }

    int logical_width;
    int logical_height;
    SDL_RendererLogicalPresentation mode;
    float left, top, right, bottom;
    if (!SDL_GetRenderLogicalPresentation(renderer, &logical_width, &logical_height, &mode) || logical_width <= 0 ||
        logical_height <= 0 || !SDL_RenderCoordinatesToWindow(renderer, 0.0f, 0.0f, &left, &top) ||
        !SDL_RenderCoordinatesToWindow(renderer, static_cast<float>(logical_width), static_cast<float>(logical_height),
                                       &right, &bottom))
    {
        fprintf(stderr, "Video: Unable to calculate mouse confinement: %s\n", SDL_GetError());
        SDL_SetWindowMouseRect(window, nullptr);
        return;
    }

    SDL_Rect rect;
    rect.x = static_cast<int>(std::ceil(left));
    rect.y = static_cast<int>(std::ceil(top));
    rect.w = std::max(1, static_cast<int>(std::floor(right)) - rect.x);
    rect.h = std::max(1, static_cast<int>(std::floor(bottom)) - rect.y);
    if (!SDL_SetWindowMouseRect(window, &rect))
        fprintf(stderr, "Video: Unable to confine mouse to game area: %s\n", SDL_GetError());
}

ivec2 video_window_to_game(float window_x, float window_y)
{
    if (!renderer || !main_screen)
        return ivec2(0);

    float game_x;
    float game_y;
    int logical_width;
    int logical_height;
    SDL_RendererLogicalPresentation mode;
    if (!SDL_GetRenderLogicalPresentation(renderer, &logical_width, &logical_height, &mode) || logical_width <= 0 ||
        logical_height <= 0 || !SDL_RenderCoordinatesFromWindow(renderer, window_x, window_y, &game_x, &game_y))
        return ivec2(0);

    game_x *= static_cast<float>(main_screen->Size().x) / logical_width;
    game_y *= static_cast<float>(main_screen->Size().y) / logical_height;
    return ivec2(std::clamp(static_cast<int>(std::lround(game_x)), 0, main_screen->Size().x - 1),
                 std::clamp(static_cast<int>(std::lround(game_y)), 0, main_screen->Size().y - 1));
}

void video_warp_mouse(ivec2 position)
{
    if (!window || !renderer || !main_screen)
        return;

    int logical_width;
    int logical_height;
    SDL_RendererLogicalPresentation mode;
    if (!SDL_GetRenderLogicalPresentation(renderer, &logical_width, &logical_height, &mode) || logical_width <= 0 ||
        logical_height <= 0)
        return;

    const float logical_x = static_cast<float>(position.x) * logical_width / main_screen->Size().x;
    const float logical_y = static_cast<float>(position.y) * logical_height / main_screen->Size().y;
    float window_x;
    float window_y;
    if (SDL_RenderCoordinatesToWindow(renderer, logical_x, logical_y, &window_x, &window_y))
        SDL_WarpMouseInWindow(window, window_x, window_y);
}

bool video_start_text_input()
{
    return window && SDL_StartTextInput(window);
}

void video_stop_text_input()
{
    if (window && SDL_TextInputActive(window))
        SDL_StopTextInput(window);
}

bool video_save_screenshot(char const *filename)
{
    return surface && filename && SDL_SaveBMP(surface, filename);
}

bool video_set_fullscreen(bool enabled)
{
    if (!window)
        return false;

    const bool was_fullscreen = window_is_fullscreen();
    if (!enabled)
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
        if (!SDL_SetWindowFullscreenMode(window, nullptr))
        {
            fprintf(stderr, "Video: Unable to select borderless desktop fullscreen mode: %s\n", SDL_GetError());
            return false;
        }
        if (!SDL_SetWindowFullscreen(window, true))
        {
            fprintf(stderr, "Video: Unable to enter fullscreen mode: %s\n", SDL_GetError());
            return false;
        }
        SDL_SyncWindow(window);
    }

    video_update_mouse_confinement();
    return window_is_fullscreen() == enabled;
}

void video_change_settings(int scale_add, bool toggle_fullscreen)
{
    if (toggle_fullscreen)
    {
        const bool target_fullscreen = !window_is_fullscreen();
        if (video_set_fullscreen(target_fullscreen))
            settings.fullscreen = target_fullscreen;
    }

    if (scale_add != 0 && !window_is_fullscreen())
    {
        // Scale window
        const int new_scale = settings.scale + scale_add;
        const int corrected_height = display_height();
        int desktop_width = 320;
        int desktop_height = 240;
        if (!current_display_size(desktop_width, desktop_height))
            fprintf(stderr, "Video: Unable to get current display size: %s\n", SDL_GetError());

        if (new_scale > 0 && new_scale <= 20 && xres * new_scale <= desktop_width &&
            corrected_height * new_scale <= desktop_height)
        {
            // Scale windows if it fits on screen
            int old_x;
            int old_y;
            int old_w;
            int old_h;
            SDL_GetWindowPosition(window, &old_x, &old_y);
            SDL_GetWindowSize(window, &old_w, &old_h);

            settings.scale = static_cast<short>(new_scale);
            const int new_w = xres * settings.scale;
            const int new_h = corrected_height * settings.scale;
            SDL_SetWindowSize(window, new_w, new_h);
            SDL_SetWindowPosition(window, old_x + (old_w - new_w) / 2, old_y + (old_h - new_h) / 2);
            SDL_SyncWindow(window);
            remember_windowed_bounds();
        }
    }

    video_update_mouse_confinement();
}

bool resize_framebuffer(int width, int height)
{
    if (!window || !renderer || !main_screen)
        return false;
    if (width < 320 || height < 200 || width > std::numeric_limits<int16_t>::max() ||
        height > std::numeric_limits<int16_t>::max())
    {
        fprintf(stderr, "Video: Invalid framebuffer size %dx%d\n", width, height);
        return false;
    }
    if (width == xres && height == yres)
        return true;

    uint8_t *new_pixels = static_cast<uint8_t *>(std::calloc(static_cast<size_t>(width), static_cast<size_t>(height)));
    SDL_Surface *new_surface =
        new_pixels ? SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_INDEX8, new_pixels, width) : nullptr;
    SDL_Texture *new_texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!new_pixels || !new_surface || !new_texture)
    {
        fprintf(stderr, "Video: Unable to resize framebuffer to %dx%d: %s\n", width, height, SDL_GetError());
        if (new_texture)
            SDL_DestroyTexture(new_texture);
        if (new_surface)
            SDL_DestroySurface(new_surface);
        std::free(new_pixels);
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
        std::free(new_pixels);
        return false;
    }

    SDL_DestroyTexture(game_texture);
    SDL_DestroySurface(surface);
    main_screen->SetSize(ivec2(width, height), new_pixels);
    game_texture = new_texture;
    surface = new_surface;
    xres = width;
    yres = height;
    main_screen->clear();

    if (!window_is_fullscreen())
    {
        int old_x, old_y, old_w, old_h;
        SDL_GetWindowPosition(window, &old_x, &old_y);
        SDL_GetWindowSize(window, &old_w, &old_h);
        const int new_w = width * settings.scale;
        const int new_h = new_display_height * settings.scale;
        SDL_SetWindowSize(window, new_w, new_h);
        SDL_SetWindowPosition(window, old_x + (old_w - new_w) / 2, old_y + (old_h - new_h) / 2);
        SDL_SyncWindow(window);
        remember_windowed_bounds();
    }

    video_update_mouse_confinement();
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
    if (game_texture)
        SDL_DestroyTexture(game_texture);
    surface = nullptr;
    game_texture = nullptr;

    delete main_screen;
    main_screen = nullptr;
}

void close_graphics()
{
    if (lastl)
        delete lastl;
    lastl = nullptr;

    close_framebuffer();

    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    renderer = nullptr;
    window = nullptr;

    windowed_bounds = {};
}

void update_dirty(image *im, int xoff, int yoff)
{
    CHECK(im->m_special);

    if (im->m_special->keep_dirt == 0)
    {
        put_image(im, xoff, yoff);
    }
    else
    {
        int count = im->m_special->dirties.Count();
        dirty_rect *dr = static_cast<dirty_rect *>(im->m_special->dirties.first());
        while (count > 0)
        {
            put_part_image(im, xoff + dr->m_aa.x, yoff + dr->m_aa.y, dr->m_aa.x, dr->m_aa.y, dr->m_bb.x + 1,
                           dr->m_bb.y + 1);
            dirty_rect *tmp = dr;
            dr = static_cast<dirty_rect *>(dr->Next());
            im->m_special->dirties.unlink(tmp);
            delete tmp;
            --count;
        }
    }

    update_window_done();
}

static void put_image(image *im, int x, int y)
{
    put_part_image(im, x, y, 0, 0, im->Size().x, im->Size().y);
}

// put_part_image()
// Draw only dirty parts of the image
//
static void put_part_image(image *im, int x, int y, int x1, int y1, int x2, int y2)
{
    if (!im || !main_screen || x >= xres || y >= yres)
        return;

    CHECK(x1 >= 0 && x2 >= x1 && x2 <= im->Size().x && y1 >= 0 && y2 >= y1 && y2 <= im->Size().y);

    if (x < 0)
    {
        x1 -= x;
        x = 0;
    }
    if (y < 0)
    {
        y1 -= y;
        y = 0;
    }

    const int copy_width = std::min(x2 - x1, xres - x);
    const int copy_height = std::min(y2 - y1, yres - y);
    if (copy_width <= 0 || copy_height <= 0)
        return;
    if (im == main_screen && x == x1 && y == y1)
        return;

    uint8_t *destination = main_screen->scan_line(y) + x;
    for (int row = 0; row < copy_height; row++)
    {
        std::memmove(destination, im->scan_line(y1 + row) + x1, static_cast<size_t>(copy_width));
        destination += xres;
    }
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

    std::array<SDL_Color, 256> colors{};
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
    SDL_Palette *surface_palette = SDL_CreateSurfacePalette(surface);
    if (!surface_palette || !SDL_SetPaletteColors(surface_palette, colors.data(), 0, ncolors))
    {
        fprintf(stderr, "Video: Unable to set palette: %s\n", SDL_GetError());
        return;
    }

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

static void update_window_done()
{
    if (!surface || !game_texture || !renderer)
        return;

    SDL_Surface *texture_surface = nullptr;
    if (!SDL_LockTextureToSurface(game_texture, nullptr, &texture_surface))
    {
        fprintf(stderr, "Video: Unable to lock framebuffer texture: %s\n", SDL_GetError());
        return;
    }
    const bool converted = SDL_BlitSurface(surface, nullptr, texture_surface, nullptr);
    SDL_UnlockTexture(game_texture);
    if (!converted)
    {
        fprintf(stderr, "Video: Unable to convert framebuffer: %s\n", SDL_GetError());
        return;
    }

    // Clear renderer
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    const SDL_FRect dest_rect = {0.0f, 0.0f, static_cast<float>(xres), static_cast<float>(display_height())};
    SDL_RenderTexture(renderer, game_texture, nullptr, &dest_rect);

    // Present the renderer
    SDL_RenderPresent(renderer);
}
