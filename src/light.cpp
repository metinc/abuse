/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "common.h"

#include "light.h"
#include "image.h"
#include "video.h"
#include "palette.h"
#include "timing.h"
#include "specs.h"
#include "filter.h"
#include "status.h"
#include "dev.h"

light_source *first_light_source = NULL;
uint8_t *white_light, *white_light_initial, *green_light, *trans_table;
short ambient_ramp = 0;
short shutdown_lighting_value, shutdown_lighting = 0;
extern char disable_autolight; // defined in dev.h

int light_detail = MEDIUM_DETAIL;

int32_t light_to_number(light_source *l)
{

    if (!l)
        return 0;
    int x = 1;
    for (light_source *s = first_light_source; s; s = s->next, x++)
        if (s == l)
            return x;
    return 0;
}

light_source *number_to_light(int32_t x)
{
    if (x == 0)
        return NULL;
    x--;
    light_source *s = first_light_source;
    for (; x && s; x--, s = s->next)
        ;
    return s;
}

light_source *light_source::copy()
{
    next = new light_source(type, x, y, inner_radius, outer_radius, xshift, yshift, next, tint, strength);
    return next;
}

void delete_all_lights()
{
    while (first_light_source)
    {
        if (dev_cont)
            dev_cont->notify_deleted_light(first_light_source);

        light_source *p = first_light_source;
        first_light_source = first_light_source->next;
        delete p;
    }
}

void delete_light(light_source *which)
{
    if (dev_cont)
        dev_cont->notify_deleted_light(which);

    if (which == first_light_source)
    {
        first_light_source = first_light_source->next;
        delete which;
    }
    else
    {
        light_source *f = first_light_source;
        for (; f->next != which && f; f = f->next)
            ;
        if (f)
        {
            f->next = which->next;
            delete which;
        }
    }
}

void light_source::calc_range()
{
    switch (type)
    {
    case 0: {
        x1 = x - (outer_radius >> xshift);
        y1 = y - (outer_radius >> yshift);
        x2 = x + (outer_radius >> xshift);
        y2 = y + (outer_radius >> yshift);
    }
    break;
    case 1: {
        x1 = x - (outer_radius >> xshift);
        y1 = y - (outer_radius >> yshift);
        x2 = x + (outer_radius >> xshift);
        y2 = y;
    }
    break;
    case 2: {
        x1 = x - (outer_radius >> xshift);
        y1 = y;
        x2 = x + (outer_radius >> xshift);
        y2 = y + (outer_radius >> yshift);
    }
    break;
    case 3: {
        x1 = x;
        y1 = y - (outer_radius >> yshift);
        x2 = x + (outer_radius >> xshift);
        y2 = y + (outer_radius >> yshift);
    }
    break;
    case 4: {
        x1 = x - (outer_radius >> xshift);
        y1 = y - (outer_radius >> yshift);
        x2 = x;
        y2 = y + (outer_radius >> yshift);
    }
    break;

    case 5: {
        x1 = x;
        y1 = y - (outer_radius >> yshift);
        x2 = x + (outer_radius >> xshift);
        y2 = y;
    }
    break;
    case 6: {
        x1 = x - (outer_radius >> xshift);
        y1 = y - (outer_radius >> yshift);
        x2 = x;
        y2 = y;
    }
    break;
    case 7: {
        x1 = x - (outer_radius >> xshift);
        y1 = y;
        x2 = x;
        y2 = y + (outer_radius >> yshift);
    }
    break;
    case 8: {
        x1 = x;
        y1 = y;
        x2 = x + (outer_radius >> xshift);
        y2 = y + (outer_radius >> yshift);
    }
    break;
    case LIGHT_TYPE_SOLID: {
        x1 = x;
        y1 = y;
        x2 = x + xshift;
        y2 = y + yshift;
    }
    break;
    case LIGHT_TYPE_LINE: {
        x1 = std::min(x, xshift) - outer_radius;
        y1 = std::min(y, yshift) - outer_radius;
        x2 = std::max(x, xshift) + outer_radius;
        y2 = std::max(y, yshift) + outer_radius;
        line_dx = static_cast<int64_t>(xshift) - x;
        line_dy = static_cast<int64_t>(yshift) - y;
        line_length_squared = line_dx * line_dx + line_dy * line_dy;
        line_length = std::max(std::abs(line_dx), std::abs(line_dy)) +
                      std::min(std::abs(line_dx), std::abs(line_dy)) / 2;
    }
    break;
    }
    mul_div = (1 << 16) / (outer_radius - inner_radius) * 64;
}

light_source::light_source(char Type, int32_t X, int32_t Y, int32_t Inner_radius, int32_t Outer_radius, int32_t Xshift,
                           int32_t Yshift, light_source *Next, int32_t Tint, int32_t Strength)
{
    type = Type;
    tint = std::clamp(Tint, 0, LIGHT_TINT_COUNT - 1);
    strength = std::clamp(Strength, 0, LIGHT_STRENGTH_MAX);
    x = X;
    y = Y;
    inner_radius = Inner_radius;
    outer_radius = Outer_radius;
    next = Next;
    known = 0;
    xshift = Xshift;
    yshift = Yshift;
    calc_range();
}

int count_lights()
{
    int t = 0;
    for (light_source *s = first_light_source; s; s = s->next)
        t++;
    return t;
}

light_source *add_light_source(char type, int32_t x, int32_t y, int32_t inner, int32_t outer, int32_t xshift,
                               int32_t yshift, int32_t tint)
{
    first_light_source = new light_source(type, x, y, inner, outer, xshift, yshift, first_light_source, tint);
    return first_light_source;
}

light_source *add_line_light_source(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t inner, int32_t outer,
                                    int32_t tint)
{
    return add_light_source(LIGHT_TYPE_LINE, x1, y1, inner, outer, x2, y2, tint);
}

#define TTINTS 9
uint8_t *tints[TTINTS];
uint8_t bright_tint[256];
constexpr int LIGHT_TINT_STEPS = 17;
std::vector<uint8_t> light_tint_table;

uint16_t light_table_crc(palette *pal)
{
    // Mix in a format revision so caches created before colored tint tables
    // were initialized correctly are rebuilt once.
    return calc_crc((uint8_t *)pal->addr(), 768) ^ 0x4c43;
}

void calc_tint(uint8_t *tint, int rs, int gs, int bs, int ra, int ga, int ba, palette *pal)
{
    palette npal;
    memset(npal.addr(), 0, 256);
    int i = 0;
    for (; i < 256; i++)
    {
        npal.set(i, (int)rs, (int)gs, (int)bs);
        rs += ra;
        if (rs > 255)
            rs = 255;
        else if (rs < 0)
            rs = 0;
        gs += ga;
        if (gs > 255)
            gs = 255;
        else if (gs < 0)
            gs = 0;
        bs += ba;
        if (bs > 255)
            bs = 255;
        else if (bs < 0)
            bs = 0;
    }
    Filter f(pal, &npal);
    Filter f2(&npal, pal);

    for (i = 0; i < 256; i++, tint++)
        *tint = f2.GetMapping(f.GetMapping(i));
}

void calc_colored_light_table(palette *pal)
{
    light_tint_table.resize(static_cast<size_t>(LIGHT_TINT_COUNT) * LIGHT_TINT_STEPS * 256);
    for (int tint = 0; tint < LIGHT_TINT_COUNT; ++tint)
    {
        for (int step = 0; step < LIGHT_TINT_STEPS; ++step)
        {
            const int weight = step * 63 / (LIGHT_TINT_STEPS - 1);
            for (int color = 0; color < 256; ++color)
            {
                uint8_t mapped = static_cast<uint8_t>(color);
                if (tint != LIGHT_TINT_WHITE && weight != 0)
                {
                    uint8_t base_r, base_g, base_b;
                    uint8_t tint_r, tint_g, tint_b;
                    pal->get(color, base_r, base_g, base_b);
                    pal->get(tints[tint][color], tint_r, tint_g, tint_b);
                    mapped = pal->find_closest((base_r * (63 - weight) + tint_r * weight + 31) / 63,
                                               (base_g * (63 - weight) + tint_g * weight + 31) / 63,
                                               (base_b * (63 - weight) + tint_b * weight + 31) / 63);
                }
                light_tint_table[(static_cast<size_t>(tint) * LIGHT_TINT_STEPS + step) * 256 + color] = mapped;
            }
        }
    }
}

void calc_light_table(palette *pal)
{
    white_light_initial = (uint8_t *)malloc(256 * 64);
    white_light = white_light_initial;

    //    green_light=(uint8_t *)malloc(256*64);
    int i = 0;
    for (; i < TTINTS; i++)
    {
        tints[i] = (uint8_t *)malloc(256);
    }

    char *lightpath;
    lightpath = (char *)malloc(strlen(get_save_filename_prefix()) + 9 + 1);
    sprintf(lightpath, "%slight.tbl", get_save_filename_prefix());

    bFILE *fp = open_file(lightpath, "rb");
    int recalc = 0;
    if (fp->open_failure())
    {
        recalc = 1;
    }
    else
    {
        if (fp->read_uint16() != light_table_crc(pal))
            recalc = 1;
        else
        {
            fp->read(white_light, 256 * 64);
            //            fp->read(green_light,256*64);
            for (i = 0; i < TTINTS; i++)
                fp->read(tints[i], 256);
            fp->read(bright_tint, 256);
            //            trans_table=(uint8_t *)malloc(256*256);
            //            fp.read(trans_table,256*256);
        }
    }
    delete fp;
    fp = NULL;

    if (recalc)
    {
        printf("Palette has changed, recalculating light table...\n");
        stack_stat status("white light");
        int color = 0;
        for (; color < 256; color++)
        {
            uint8_t r, g, b;
            pal->get(color, r, g, b);
            if (stat_man)
                stat_man->update(color * 100 / 256);
            for (int intensity = 63; intensity >= 0; intensity--)
            {
                if (r > 0 || g > 0 || b > 0)
                    white_light[intensity * 256 + color] = pal->find_closest(r, g, b);
                else
                    white_light[intensity * 256 + color] = 0;
                if (r)
                    r--;
                if (g)
                    g--;
                if (b)
                    b--;
            }
        }
        /*    stat_man->push("green light",NULL);
    for (color=0; color<256; color++)
    {
      stat_man->update(color*100/256);
      uint8_t r,g,b;
      pal->get(color,b,r,g);
      r=r*3/5; b=b*3/5; g+=7; if (g>255) g=255;

      for (int intensity=63; intensity>=0; intensity--)
      {
        if (r > 0 || g > 0 || b > 0)
          white_light[intensity * 256 + color] = pal->find_closest(r, g, b);
    else
          white_light[intensity * 256 + color] = 0;
        if (r)
          r--;
        if (g)
          g--;
        if (b)
          b--;
      }
    }
    stat_man->pop();*/

        stack_stat tint_status("tints");
        uint8_t t[TTINTS * 6] = {
            0, 0, 0, 0, 0, 0, // normal
            0, 0, 0, 1, 0, 0, // red
            0, 0, 0, 1, 1, 0, // yellow
            0, 0, 0, 1, 0, 1, // purple
            0, 0, 0, 1, 1, 1, // gray
            0, 0, 0, 0, 1, 0, // green
            0, 0, 0, 0, 0, 1, // blue
            0, 0, 0, 0, 1, 1, // cyan

            0, 0, 0, 0, 0, 0 // reverse green  (night vision effect)
        };
        uint8_t *ti = t + 6;
        uint8_t *c;
        for (i = 0, c = tints[0]; i < 256; i++, c++)
            *c = i; // make the normal tint (maps everthing to itself)
        for (i = 0, c = tints[TTINTS - 1]; i < 256; i++, c++) // reverse green
        {
            int r = pal->red(i) / 2, g = 255 - pal->green(i) - 30, b = pal->blue(i) * 3 / 5 + 50;
            if (g < 0)
                g = 0;
            if (b > 255)
                b = 0;
            *c = pal->find_closest(r, g, b);
        }
        for (i = 0; i < 256; i++)
        {
            int r = pal->red(i) + (255 - pal->red(i)) / 2, g = pal->green(i) + (255 - pal->green(i)) / 2,
                b = pal->blue(i) + (255 - pal->blue(i)) / 2;
            bright_tint[i] = pal->find_closest(r, g, b);
        }

        // make the colored tints
        for (i = 1; i < TTINTS - 1; i++)
        {
            if (stat_man)
                stat_man->update(i * 100 / (TTINTS - 1));
            calc_tint(tints[i], ti[0], ti[1], ti[2], ti[3], ti[4], ti[5], pal);
            ti += 6;
        }
        /*    fprintf(stderr,"calculating transparency tables (256 total)\n");
    trans_table=(uint8_t *)malloc(256*256);

    uint8_t *tp=trans_table;
    for (i=0; i<256; i++)
    {
      uint8_t r1,g1,b1,r2,g2,b2;
      pal->get(i,r1,g1,b1);
      if ((i%16)==0)
        fprintf(stderr,"%d ",i);
      for (int j=0; j<256; j++,tp++)
      {
    if (r1==0 && r2==0 && b2==0)
      *tp=j;
    else
    {
      pal->get(j,r2,g2,b2);
      *tp=pal->find_closest((r2-r1)*3/7+r1,(g2-g1)*3/7+g1,(b2-b1)*3/7+b1);
    }
      }
    }*/

        bFILE *f = open_file(lightpath, "wb");
        if (f->open_failure())
            printf("Unable to open file %s for writing\n", lightpath);
        else
        {
            f->write_uint16(light_table_crc(pal));
            f->write(white_light, 256 * 64);
            //      f->write(green_light,256*64);
            for (int i = 0; i < TTINTS; i++)
                f->write(tints[i], 256);
            f->write(bright_tint, 256);
            //    f.write(trans_table,256*256);
        }
        delete f;
    }
    calc_colored_light_table(pal);
    free(lightpath);
}

uint16_t min_light_level;

namespace
{
struct light_sample
{
    uint8_t intensity;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct light_grid
{
    std::vector<light_source *> radial;
    std::vector<light_source *> solid;
    std::vector<light_sample> samples;
};

constexpr uint8_t tint_channels[LIGHT_TINT_COUNT][3] = {
    {0, 0, 0}, // white does not tint the illuminated pixels
    {1, 0, 0}, // red
    {1, 1, 0}, // yellow
    {1, 0, 1}, // purple
    {1, 1, 1}, // gray
    {0, 1, 0}, // green
    {0, 0, 1}, // blue
    {0, 1, 1}, // cyan
};

// Keep samples aligned to world coordinates. Otherwise the interpolation grid
// would slide over a stationary light as the camera moved and make it shimmer.
int32_t align_down(int32_t value, int step)
{
    int32_t remainder = value % step;
    if (remainder < 0)
        remainder += step;
    return value - remainder;
}

bool lighting_sample_size(int &step_x, int &step_y)
{
    switch (light_detail)
    {
    case HIGH_DETAIL:
        step_x = 4;
        step_y = 2;
        return true;
    case MEDIUM_DETAIL:
        step_x = 8;
        step_y = 4;
        return true;
    case LOW_DETAIL:
        step_x = 16;
        step_y = 8;
        return true;
    case POOR_DETAIL:
    default:
        return false;
    }
}

light_sample radial_light_value(std::vector<light_source *> const &lights, int32_t world_x, int32_t world_y)
{
    int64_t value = min_light_level;
    int64_t red = 0;
    int64_t green = 0;
    int64_t blue = 0;

    for (light_source const *source : lights)
    {
        if (world_x < source->x1 || world_x > source->x2 || world_y < source->y1 || world_y > source->y2)
            continue;

        int64_t distance;
        if (source->type == LIGHT_TYPE_LINE)
        {
            const int64_t point_x = static_cast<int64_t>(world_x) - source->x;
            const int64_t point_y = static_cast<int64_t>(world_y) - source->y;
            const int64_t projection = point_x * source->line_dx + point_y * source->line_dy;

            int64_t distance_x;
            int64_t distance_y;
            if (source->line_length_squared == 0 || projection <= 0)
            {
                distance_x = std::abs(point_x);
                distance_y = std::abs(point_y);
                distance = std::max(distance_x, distance_y) + std::min(distance_x, distance_y) / 2;
            }
            else if (projection >= source->line_length_squared)
            {
                distance_x = std::abs(static_cast<int64_t>(world_x) - source->xshift);
                distance_y = std::abs(static_cast<int64_t>(world_y) - source->yshift);
                distance = std::max(distance_x, distance_y) + std::min(distance_x, distance_y) / 2;
            }
            else
            {
                const int64_t cross = std::abs(source->line_dx * point_y - source->line_dy * point_x);
                distance = (cross + source->line_length / 2) / source->line_length;
            }
        }
        else
        {
            const int64_t dx = std::abs(static_cast<int64_t>(source->x) - world_x) << source->xshift;
            const int64_t dy = std::abs(static_cast<int64_t>(source->y) - world_y) << source->yshift;
            distance = std::max(dx, dy) + std::min(dx, dy) / 2;
        }
        if (distance < source->outer_radius)
        {
            int64_t contribution = ((source->outer_radius - distance) * source->mul_div) >> 16;
            if (source->strength != LIGHT_STRENGTH_MAX)
                contribution = contribution * source->strength / LIGHT_STRENGTH_MAX;
            value += contribution;
            if (source->tint != LIGHT_TINT_WHITE)
            {
                red += contribution * tint_channels[source->tint][0];
                green += contribution * tint_channels[source->tint][1];
                blue += contribution * tint_channels[source->tint][2];
            }
        }
    }

    return {static_cast<uint8_t>(std::min<int64_t>(value, 63)), static_cast<uint8_t>(std::min<int64_t>(red, 63)),
            static_cast<uint8_t>(std::min<int64_t>(green, 63)), static_cast<uint8_t>(std::min<int64_t>(blue, 63))};
}

light_source const *solid_light_at(std::vector<light_source *> const &lights, int32_t world_x, int32_t world_y)
{
    for (light_source const *source : lights)
        if (world_x >= source->x1 && world_x <= source->x2 && world_y >= source->y1 && world_y <= source->y2)
            return source;
    return nullptr;
}

int tint_from_channels(int red, int green, int blue)
{
    const int strength = std::max({red, green, blue});
    if (strength == 0)
        return LIGHT_TINT_WHITE;

    const int mask = (red * 2 >= strength ? 4 : 0) | (green * 2 >= strength ? 2 : 0) | (blue * 2 >= strength ? 1 : 0);
    constexpr uint8_t mask_to_tint[8] = {LIGHT_TINT_WHITE, LIGHT_TINT_BLUE,   LIGHT_TINT_GREEN,  LIGHT_TINT_CYAN,
                                         LIGHT_TINT_RED,   LIGHT_TINT_PURPLE, LIGHT_TINT_YELLOW, LIGHT_TINT_GRAY};
    return mask_to_tint[mask];
}

uint8_t apply_colored_tint(int tint, int strength, uint8_t color)
{
    if (tint == LIGHT_TINT_WHITE || strength <= 0 || light_tint_table.empty())
        return color;
    const int step = std::clamp((strength * (LIGHT_TINT_STEPS - 1) + 31) / 63, 0, LIGHT_TINT_STEPS - 1);
    return light_tint_table[(static_cast<size_t>(tint) * LIGHT_TINT_STEPS + step) * 256 + color];
}

void copy_doubled(image *source, image *destination, ivec2 clip_min, ivec2 clip_max, int32_t out_x, int32_t out_y)
{
    for (int y = clip_min.y; y < clip_max.y; ++y)
    {
        uint8_t const *input = source->scan_line(y) + clip_min.x;
        uint8_t *output0 = destination->scan_line(out_y + y * 2) + out_x + clip_min.x * 2;
        uint8_t *output1 = output0 + destination->Size().x;
        for (int x = clip_min.x; x < clip_max.x; ++x)
        {
            const uint8_t color = *input++;
            *output0++ = color;
            *output0++ = color;
            *output1++ = color;
            *output1++ = color;
        }
    }
}

void apply_uniform_light(image *source, image *destination, ivec2 clip_min, ivec2 clip_max, uint8_t *remap,
                         int output_scale, int32_t out_x, int32_t out_y)
{
    for (int y = clip_min.y; y < clip_max.y; ++y)
    {
        uint8_t *input = source->scan_line(y) + clip_min.x;
        if (output_scale == 1)
        {
            for (int x = clip_min.x; x < clip_max.x; ++x, ++input)
                *input = remap[*input];
            continue;
        }

        uint8_t *output0 = destination->scan_line(out_y + y * 2) + out_x + clip_min.x * 2;
        uint8_t *output1 = output0 + destination->Size().x;
        for (int x = clip_min.x; x < clip_max.x; ++x)
        {
            const uint8_t color = remap[*input++];
            *output0++ = color;
            *output0++ = color;
            *output1++ = color;
            *output1++ = color;
        }
    }
}

void smooth_light_screen(image *source, int32_t screen_x, int32_t screen_y, uint8_t *light_lookup, uint16_t ambient,
                         image *destination, int output_scale, int32_t out_x, int32_t out_y)
{
    ivec2 clip_min, clip_max;
    source->GetClip(clip_min, clip_max);
    const int width = clip_max.x - clip_min.x;
    const int height = clip_max.y - clip_min.y;
    if (width <= 0 || height <= 0)
        return;

    int step_x = 0;
    int step_y = 0;
    const bool lighting_enabled = lighting_sample_size(step_x, step_y);

    const int adjusted_ambient = std::clamp(static_cast<int>(ambient) + ambient_ramp, 0, 63);
    min_light_level = static_cast<uint16_t>(adjusted_ambient);

    // The doubled path is also the scaler, so it must still copy pixels when
    // lighting is disabled or full-bright. The normal path already contains
    // the desired pixels and can return immediately.
    if (!lighting_enabled || adjusted_ambient == 63)
    {
        if (output_scale == 2)
            copy_doubled(source, destination, clip_min, clip_max, out_x, out_y);
        return;
    }

    // Reuse the small working buffers across frames. This avoids replacing the
    // old patch graph's many allocations with three new allocations per view.
    static thread_local light_grid grid;
    grid.radial.clear();
    grid.solid.clear();
    bool has_colored_lights = false;
    const int32_t world_right = screen_x + width - 1;
    const int32_t world_bottom = screen_y + height - 1;
    for (light_source *light = first_light_source; light; light = light->next)
    {
        if (light->x2 < screen_x || light->x1 > world_right || light->y2 < screen_y || light->y1 > world_bottom)
            continue;
        (light->type == LIGHT_TYPE_SOLID ? grid.solid : grid.radial).push_back(light);
        has_colored_lights |= light->tint != LIGHT_TINT_WHITE;
    }

    if (grid.radial.empty() && grid.solid.empty())
    {
        apply_uniform_light(source, destination, clip_min, clip_max, light_lookup + (adjusted_ambient << 8),
                            output_scale, out_x, out_y);
        return;
    }

    const int32_t first_world_x = align_down(screen_x, step_x);
    const int32_t first_world_y = align_down(screen_y, step_y);
    const int first_local_x = first_world_x - screen_x;
    const int first_local_y = first_world_y - screen_y;
    const int columns = (width - 1 - first_local_x) / step_x + 2;
    const int rows = (height - 1 - first_local_y) / step_y + 2;
    grid.samples.resize(static_cast<size_t>(columns) * rows);

    for (int row = 0; row < rows; ++row)
    {
        const int32_t world_y = first_world_y + row * step_y;
        for (int column = 0; column < columns; ++column)
        {
            const int32_t world_x = first_world_x + column * step_x;
            grid.samples[static_cast<size_t>(row) * columns + column] =
                radial_light_value(grid.radial, world_x, world_y);
        }
    }

    for (int row = 0; row + 1 < rows; ++row)
    {
        const int local_y0 = first_local_y + row * step_y;
        const int y_begin = std::max(0, local_y0);
        const int y_end = std::min(height, local_y0 + step_y);
        for (int local_y = y_begin; local_y < y_end; ++local_y)
        {
            const int y_fraction = local_y - local_y0;
            const int world_y = screen_y + local_y;
            light_sample const *top = grid.samples.data() + static_cast<size_t>(row) * columns;
            light_sample const *bottom = top + columns;
            uint8_t *input = source->scan_line(clip_min.y + local_y) + clip_min.x;
            uint8_t *output0 = nullptr;
            uint8_t *output1 = nullptr;
            if (output_scale == 2)
            {
                const int destination_y = out_y + (clip_min.y + local_y) * 2;
                output0 = destination->scan_line(destination_y) + out_x + clip_min.x * 2;
                output1 = output0 + destination->Size().x;
            }

            for (int column = 0; column + 1 < columns; ++column)
            {
                const int local_x0 = first_local_x + column * step_x;
                const int x_begin = std::max(0, local_x0);
                const int x_end = std::min(width, local_x0 + step_x);
                auto vertical = [step_y, y_fraction](uint8_t top_value, uint8_t bottom_value) {
                    return (top_value * (step_y - y_fraction) + bottom_value * y_fraction + step_y / 2) / step_y;
                };
                const int left = vertical(top[column].intensity, bottom[column].intensity);
                const int right = vertical(top[column + 1].intensity, bottom[column + 1].intensity);
                const int red_left = has_colored_lights ? vertical(top[column].red, bottom[column].red) : 0;
                const int red_right = has_colored_lights ? vertical(top[column + 1].red, bottom[column + 1].red) : 0;
                const int green_left = has_colored_lights ? vertical(top[column].green, bottom[column].green) : 0;
                const int green_right =
                    has_colored_lights ? vertical(top[column + 1].green, bottom[column + 1].green) : 0;
                const int blue_left = has_colored_lights ? vertical(top[column].blue, bottom[column].blue) : 0;
                const int blue_right = has_colored_lights ? vertical(top[column + 1].blue, bottom[column + 1].blue) : 0;

                for (int local_x = x_begin; local_x < x_end; ++local_x)
                {
                    const int world_x = screen_x + local_x;
                    light_source const *solid =
                        grid.solid.empty() ? nullptr : solid_light_at(grid.solid, world_x, world_y);
                    const int x_fraction = local_x - local_x0;
                    const int intensity =
                        solid ? std::clamp(adjusted_ambient + (solid->inner_radius - adjusted_ambient) *
                                                                  solid->strength / LIGHT_STRENGTH_MAX,
                                           0, 63)
                              : (left * (step_x - x_fraction) + right * x_fraction + step_x / 2) / step_x;
                    const uint8_t color = input[local_x];
                    uint8_t lit_color = light_lookup[(intensity << 8) + color];
                    if (has_colored_lights)
                    {
                        const int red =
                            solid ? tint_channels[solid->tint][0] * solid->strength
                                  : (red_left * (step_x - x_fraction) + red_right * x_fraction + step_x / 2) / step_x;
                        const int green =
                            solid
                                ? tint_channels[solid->tint][1] * solid->strength
                                : (green_left * (step_x - x_fraction) + green_right * x_fraction + step_x / 2) / step_x;
                        const int blue =
                            solid ? tint_channels[solid->tint][2] * solid->strength
                                  : (blue_left * (step_x - x_fraction) + blue_right * x_fraction + step_x / 2) / step_x;
                        lit_color = apply_colored_tint(tint_from_channels(red, green, blue),
                                                       std::max({red, green, blue}), lit_color);
                    }

                    if (output_scale == 1)
                    {
                        input[local_x] = lit_color;
                    }
                    else
                    {
                        output0[local_x * 2] = output0[local_x * 2 + 1] = lit_color;
                        output1[local_x * 2] = output1[local_x * 2 + 1] = lit_color;
                    }
                }
            }
        }
    }
}
} // namespace

void light_screen(image *sc, int32_t screenx, int32_t screeny, uint8_t *light_lookup, uint16_t ambient)
{
    if (shutdown_lighting && !disable_autolight)
        ambient = shutdown_lighting_value;
    smooth_light_screen(sc, screenx, screeny, light_lookup, ambient, sc, 1, 0, 0);
}

void double_light_screen(image *sc, int32_t screenx, int32_t screeny, uint8_t *light_lookup, uint16_t ambient,
                         image *out, int32_t out_x, int32_t out_y)
{
    ivec2 clip_min, clip_max;
    sc->GetClip(clip_min, clip_max);
    if (out_x < 0 || out_y < 0 || out_x + clip_max.x * 2 > out->Size().x || out_y + clip_max.y * 2 > out->Size().y)
        return;

    if (shutdown_lighting && !disable_autolight)
        ambient = shutdown_lighting_value;
    smooth_light_screen(sc, screenx, screeny, light_lookup, ambient, out, 2, out_x, out_y);
}

void add_light_spec(spec_directory *sd, char const *level_name)
{
    int32_t size = 4 + 4; // number of lights and minimum light levels
    for (light_source *f = first_light_source; f; f = f->next)
        size += 6 * 4 + 2;
    sd->add_by_hand(new spec_entry(SPEC_LIGHT_LIST, "lights", NULL, size, 0));
}

void write_lights(bFILE *fp)
{
    int t = 0;
    light_source *f = first_light_source;
    for (; f; f = f->next)
        t++;
    fp->write_uint32(t);
    fp->write_uint32(min_light_level);
    for (f = first_light_source; f; f = f->next)
    {
        fp->write_uint32(f->x);
        fp->write_uint32(f->y);
        fp->write_uint32(f->xshift);
        fp->write_uint32(f->yshift);
        fp->write_uint32(f->inner_radius);
        fp->write_uint32(f->outer_radius);
        fp->write_uint8(f->type);
        fp->write_uint8(f->tint);
    }
}

void read_lights(spec_directory *sd, bFILE *fp, char const *level_name)
{
    delete_all_lights();
    spec_entry *se = sd->find("lights");
    if (se)
    {
        fp->seek(se->offset, SEEK_SET);
        int32_t t = fp->read_uint32();
        const bool has_tints = se->size >= static_cast<unsigned long>(8 + t * (6 * 4 + 2));
        min_light_level = fp->read_uint32();
        light_source *last = NULL;
        while (t)
        {
            t--;
            int32_t x = fp->read_uint32();
            int32_t y = fp->read_uint32();
            int32_t xshift = fp->read_uint32();
            int32_t yshift = fp->read_uint32();
            int32_t ir = fp->read_uint32();
            int32_t ora = fp->read_uint32();
            int32_t ty = fp->read_uint8();
            int32_t tint = has_tints ? fp->read_uint8() : LIGHT_TINT_WHITE;

            light_source *p = new light_source(ty, x, y, ir, ora, xshift, yshift, NULL, tint);

            if (first_light_source)
                last->next = p;
            else
                first_light_source = p;
            last = p;
        }
    }
}
