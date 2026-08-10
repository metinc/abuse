/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *  Copyright (c) 2016 Antonio Radojkovic <antonior.software@gmail.com>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>

#include "common.h"

#include "volumewindow.h" // class VolumeWindow
#include "property.h" // class property_manager
#include "gui.h" // ico_button
#include "scroller.h" // class pick_list

//AR
#include "sdlport/setup.h"
extern Settings settings;
//

namespace
{
struct soundfont_option
{
    std::string label;
    std::string value;
};

class soundfont_picker : public pick_list
{
  public:
    using pick_list::pick_list;

    void note_new_current(image *screen, InputManager *inm, int x) override
    {
        wm->Push(new Event(id, (char *)this));
    }

    void set_x(int x, image *screen) override
    {
        const int previous = first_selected();
        pick_list::set_x(x, screen);
        if (first_selected() != previous)
            wm->Push(new Event(id, (char *)this));
    }

    void set_x_silently(int x, image *screen)
    {
        pick_list::set_x(x, screen);
    }
};

std::vector<soundfont_option> available_soundfonts()
{
    std::vector<soundfont_option> options;
    const std::filesystem::path directory = std::filesystem::path(get_filename_prefix()) / "soundfonts";
    std::error_code error;
    for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error))
    {
        if (!it->is_regular_file(error))
            continue;

        std::string extension = it->path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (extension == ".sf2" || extension == ".sf3")
        {
            const std::string filename = it->path().filename().string();
            options.push_back({it->path().stem().string(), filename});
        }
    }

    const bool configured_is_listed = std::any_of(options.begin(), options.end(), [](const soundfont_option &option) {
        return option.value == settings.soundfont;
    });
    if (!settings.soundfont.empty() && !configured_is_listed)
    {
        std::filesystem::path configured_path = settings.soundfont;
        if (configured_path.is_relative())
            configured_path = directory / configured_path;
        if (std::filesystem::is_regular_file(configured_path))
        {
            const std::filesystem::path label_path = settings.soundfont;
            const std::string label = label_path.has_extension() ? label_path.stem().string()
                                                                  : label_path.filename().string();
            options.push_back({label, settings.soundfont});
        }
    }

    std::sort(options.begin(), options.end(),
              [](const soundfont_option &a, const soundfont_option &b) { return a.label < b.label; });
    return options;
}
}

VolumeWindow::VolumeWindow() : Jwindow(symbol_str("ic_volume"))
{
    char const *ff = "art/frame.spe";

    u_u = cache.reg(ff, "u_u", SPEC_IMAGE, 1), u_d = cache.reg(ff, "u_u", SPEC_IMAGE, 1),
    u_ua = cache.reg(ff, "u_ua", SPEC_IMAGE, 1), u_da = cache.reg(ff, "u_da", SPEC_IMAGE, 1),
    d_u = cache.reg(ff, "d_u", SPEC_IMAGE, 1), d_d = cache.reg(ff, "d_u", SPEC_IMAGE, 1),
    d_ua = cache.reg(ff, "d_ua", SPEC_IMAGE, 1), d_da = cache.reg(ff, "d_da", SPEC_IMAGE, 1),
    slider = cache.reg(ff, "volume_slide", SPEC_IMAGE, 1);
    const ivec2 volume_panel_size(41, 101);
    volume_origin = ivec2(left_border(), top_border());

    //AR center text
    int text_x = 8;
    if (settings.big_font)
        text_x = 1;

    inm->add(new ico_button(
        volume_origin.x + 10, volume_origin.y + 27, ID_SFX_DOWN, d_u, d_d, d_ua, d_da,
        new ico_button(
            volume_origin.x + 21, volume_origin.y + 27, ID_SFX_UP, u_u, u_d, u_ua, u_da,
            new info_field(volume_origin.x + text_x, volume_origin.y + 38, 0, symbol_str("SFXv"),
                           new ico_button(volume_origin.x + 10, volume_origin.y + 72, ID_MUSIC_DOWN, d_u, d_d, d_ua,
                                          d_da,
                                          new ico_button(volume_origin.x + 21, volume_origin.y + 72, ID_MUSIC_UP, u_u,
                                                         u_d, u_ua, u_da,
                                                         new info_field(volume_origin.x + text_x, volume_origin.y + 82,
                                                                        0, symbol_str("MUSICv"), NULL)))))));

    std::vector<soundfont_option> options = available_soundfonts();
    soundfont_values.reserve(options.size());
    soundfont_labels.reserve(options.size());
    int selected_soundfont = -1;
    int default_soundfont = 0;
    for (const soundfont_option &option : options)
    {
        if (option.value == DEFAULT_SOUNDFONT)
            default_soundfont = static_cast<int>(soundfont_values.size());
        if (option.value == settings.soundfont)
            selected_soundfont = static_cast<int>(soundfont_values.size());
        soundfont_values.push_back(option.value);
        soundfont_labels.push_back(option.label);
    }
    if (selected_soundfont < 0)
        selected_soundfont = default_soundfont;
    soundfont_label_ptrs.reserve(soundfont_labels.size());
    for (std::string &label : soundfont_labels)
        soundfont_label_ptrs.push_back(label.data());

    const int soundfont_x = volume_origin.x + volume_panel_size.x + 8;
    const int soundfont_label_y = volume_origin.y + 10;
    const int soundfont_picker_y = soundfont_label_y + wm->font()->Size().y + 3;
    const int soundfont_rows = std::min(5, std::max(1, static_cast<int>(soundfont_labels.size())));
    inm->add(new info_field(soundfont_x, soundfont_label_y, 0, symbol_str("soundfont"), nullptr));
    if (!soundfont_labels.empty())
        inm->add(new soundfont_picker(soundfont_x, soundfont_picker_y, ID_SOUNDFONT_PICKER, soundfont_rows,
                                      soundfont_label_ptrs.data(), static_cast<int>(soundfont_label_ptrs.size()),
                                      selected_soundfont, nullptr));
    else
        inm->add(new info_field(soundfont_x, soundfont_picker_y, 0, symbol_str("soundfont_none"), nullptr));

    size_t longest_label = 0;
    for (const std::string &label : soundfont_labels)
        longest_label = std::max(longest_label, label.size());
    if (soundfont_labels.empty())
        longest_label = strlen(symbol_str("soundfont_none"));
    const int scrollbar_width = soundfont_labels.size() > static_cast<size_t>(soundfont_rows) ? 16 : 0;
    const int content_right =
        soundfont_x + static_cast<int>(longest_label) * wm->font()->Size().x + scrollbar_width + 8;
    const int content_bottom = std::max(volume_origin.y + volume_panel_size.y,
                                        soundfont_picker_y + soundfont_rows * (wm->font()->Size().y + 1) + 6);
    m_size.x = std::max(volume_origin.x + volume_panel_size.x, content_right) + right_border();
    m_size.y = content_bottom + bottom_border();
    backg = wm->dark_color();
    _x2 = m_size.x - right_border() - 1;
    _y2 = m_size.y - bottom_border() - 1;
    m_pos.x = prop->getd("volume_x", (xres - m_size.x) / 2);
    m_pos.y = prop->getd("volume_y", (yres - m_size.y) / 2);
    m_pos.x = std::max(0, std::min(m_pos.x, xres - m_size.x));
    m_pos.y = std::max(0, std::min(m_pos.y, yres - m_size.y));
    m_surf = new image(m_size, NULL, 2);
    image_list.unlink(m_surf);
    redraw();
}

void VolumeWindow::redraw()
{
    Jwindow::redraw();
    draw_music_vol();
    draw_sfx_vol();
    inm->redraw();
}

std::string VolumeWindow::selected_soundfont()
{
    pick_list *picker = static_cast<pick_list *>(inm->get(ID_SOUNDFONT_PICKER));
    const int selection = picker ? picker->get_selection() : -1;
    if (selection < 0 || selection >= static_cast<int>(soundfont_values.size()))
        return {};
    return soundfont_values[selection];
}

void VolumeWindow::select_soundfont(const std::string &soundfont)
{
    const auto selection = std::find(soundfont_values.begin(), soundfont_values.end(), soundfont);
    if (selection == soundfont_values.end())
        return;

    soundfont_picker *picker = static_cast<soundfont_picker *>(inm->get(ID_SOUNDFONT_PICKER));
    if (picker)
        picker->set_x_silently(static_cast<int>(selection - soundfont_values.begin()), m_surf);
}

void VolumeWindow::draw_vol(int x1, int y1, int x2, int y2, int t, int max, int c1, int c2)
{
    x1 += volume_origin.x;
    x2 += volume_origin.x;
    y1 += volume_origin.y;
    y2 += volume_origin.y;
    int dx = x1 + t * (x2 - x1) / max;
    if (t != 0)
    {
        m_surf->PutImage(cache.img(slider), ivec2(x1, y1));
        //      m_surf->bar(x1,y1,dx,y2,c1);
    }
    else
        dx--;

    if (dx < x2)
        m_surf->Bar(ivec2(dx + 1, y1), ivec2(x2, y2), c2);
}

void VolumeWindow::draw_sfx_vol()
{
    draw_vol(6, 16, 34, 22, sfx_volume, 127, pal->find_closest(200, 75, 19), pal->find_closest(40, 0, 0));
}

void VolumeWindow::draw_music_vol()
{
    draw_vol(6, 61, 34, 67, music_volume, 127, pal->find_closest(255, 0, 0), pal->find_closest(40, 0, 0));
}
