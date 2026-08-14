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

#include "audio_volume.h"
#include "audio_settings.h" // class AudioSettingsWindow
#include "property.h" // class property_manager
#include "gui.h" // ico_button
#include "scroller.h" // class pick_list

//AR
#include "sdlport/setup.h"
extern Settings settings;
//

namespace
{
class volume_bar : public ifield
{
    int x2, y2;
    int slider;
    float *value;
    int empty_color;

  public:
    volume_bar(int x1, int y1, int X2, int Y2, int Slider, float *Value, int EmptyColor, ifield *Next)
        : x2(X2), y2(Y2), slider(Slider), value(Value), empty_color(EmptyColor)
    {
        m_pos = ivec2(x1, y1);
        id = ID_NULL;
        next = Next;
    }

    void Move(ivec2 pos) override
    {
        const ivec2 delta = pos - m_pos;
        m_pos = pos;
        x2 += delta.x;
        y2 += delta.y;
    }

    void area(int &x1, int &y1, int &X2, int &Y2) override
    {
        x1 = m_pos.x;
        y1 = m_pos.y;
        X2 = x2;
        Y2 = y2;
    }

    void draw_first(image *screen) override
    {
        const int width = x2 - m_pos.x + 1;
        const int height = y2 - m_pos.y + 1;
        const int level = audio_volume_level(*value);
        const int filled_width = level == 0 ? 0 : std::min(level * 2 - 1, width);

        screen->Bar(m_pos, ivec2(x2, y2), empty_color);
        if (filled_width > 0)
            screen->PutPart(cache.img(slider), m_pos, ivec2(0), ivec2(filled_width, height));
    }

    void draw(int active, image *screen) override
    {
        (void)active;
        draw_first(screen);
    }

    void handle_event(Event &ev, image *screen, InputManager *im) override
    {
        (void)ev;
        (void)screen;
        (void)im;
    }

    int selectable() override
    {
        return 0;
    }

    char *read() override
    {
        return reinterpret_cast<char *>(value);
    }
};

struct soundfont_option
{
    std::string label;
    std::string value;
};

class soundfont_picker : public pick_list
{
    int minimum_item_width;

  public:
    soundfont_picker(int X, int Y, int ID, int height, char **List, int num_entries, int start_yoffset,
                     int MinimumItemWidth, ifield *Next)
        : pick_list(X, Y, ID, height, List, num_entries, start_yoffset, Next), minimum_item_width(MinimumItemWidth)
    {
        const int selection = first_selected();
        reconfigure();
        cur_sel = selection;
        sx = std::min(cur_sel, max_scroll_position());
    }

    int item_width() override
    {
        return std::max(minimum_item_width, pick_list::item_width());
    }

    void note_new_current(image *screen, InputManager *inm, int x) override
    {
        wm->PushMessage(id, this);
    }

    void set_x(int x, image *screen) override
    {
        const int previous = first_selected();
        pick_list::set_x(x, screen);
        if (first_selected() != previous)
            wm->PushMessage(id, this);
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
            const std::string label =
                label_path.has_extension() ? label_path.stem().string() : label_path.filename().string();
            options.push_back({label, settings.soundfont});
        }
    }

    std::sort(options.begin(), options.end(),
              [](const soundfont_option &a, const soundfont_option &b) { return a.label < b.label; });
    return options;
}
}

AudioSettingsWindow::AudioSettingsWindow()
{
    char const *ff = "art/frame.spe";

    const int u_u = cache.reg(ff, "u_u", SPEC_IMAGE, 1);
    const int u_d = cache.reg(ff, "u_u", SPEC_IMAGE, 1);
    const int u_ua = cache.reg(ff, "u_ua", SPEC_IMAGE, 1);
    const int u_da = cache.reg(ff, "u_da", SPEC_IMAGE, 1);
    const int d_u = cache.reg(ff, "d_u", SPEC_IMAGE, 1);
    const int d_d = cache.reg(ff, "d_u", SPEC_IMAGE, 1);
    const int d_ua = cache.reg(ff, "d_ua", SPEC_IMAGE, 1);
    const int d_da = cache.reg(ff, "d_da", SPEC_IMAGE, 1);
    const int slider = cache.reg(ff, "volume_slide", SPEC_IMAGE, 1);

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

    size_t longest_soundfont = 0;
    for (const std::string &label : soundfont_labels)
        longest_soundfont = std::max(longest_soundfont, label.size());
    if (soundfont_labels.empty())
        longest_soundfont = strlen(symbol_str("soundfont_none"));

    const int font_width = wm->font()->Size().x;
    const int font_height = wm->font()->Size().y;
    const int client_left = Jwindow::left_border();
    const int client_top = Jwindow::top_border();
    const int padding = 8;
    const int control_gap = 4;
    const int row_gap = 7;
    const int section_gap = 11;
    const int content_x = client_left + padding - 2;

    const int label_width =
        std::max(static_cast<int>(strlen(symbol_str("SFXv"))), static_cast<int>(strlen(symbol_str("MUSICv")))) *
        font_width;
    const ivec2 down_size = cache.img(d_u)->Size();
    const ivec2 up_size = cache.img(u_u)->Size();
    const ivec2 bar_size = cache.img(slider)->Size();
    const int row_height = std::max({font_height, down_size.y, up_size.y, bar_size.y});
    const int down_x = content_x + label_width + padding;
    const int bar_x = down_x + down_size.x + control_gap;
    const int up_x = bar_x + bar_size.x + control_gap;
    const int row_right = up_x + up_size.x;

    const int soundfont_rows = std::min(5, std::max(1, static_cast<int>(soundfont_labels.size())));
    const int scrollbar_width = soundfont_labels.size() > static_cast<size_t>(soundfont_rows) ? 16 : 0;
    const int natural_soundfont_width = static_cast<int>(longest_soundfont) * font_width + scrollbar_width + 8;
    const int client_width = std::max(
        {settings.big_font ? 240 : 180, row_right - client_left + padding, natural_soundfont_width + padding * 2});

    const int sfx_row_y = client_top + padding;
    const int music_row_y = sfx_row_y + row_height + row_gap;
    const int soundfont_label_y = music_row_y + row_height + section_gap;
    const int soundfont_picker_y = soundfont_label_y + wm->font()->Size().y + 3;
    const int soundfont_x = content_x;
    const int picker_width = client_width - padding * 2;
    const int minimum_item_width = picker_width - scrollbar_width - 4;

    ifield *fields = nullptr;
    if (!soundfont_labels.empty())
        fields = new soundfont_picker(soundfont_x, soundfont_picker_y, ID_SOUNDFONT_PICKER, soundfont_rows,
                                      soundfont_label_ptrs.data(), static_cast<int>(soundfont_label_ptrs.size()),
                                      selected_soundfont, minimum_item_width, fields);
    else
        fields = new info_field(soundfont_x, soundfont_picker_y, 0, symbol_str("soundfont_none"), fields);
    fields = new info_field(soundfont_x, soundfont_label_y, 0, symbol_str("soundfont"), fields);

    const int music_controls_y = music_row_y + (row_height - down_size.y) / 2;
    const int music_bar_y = music_row_y + (row_height - bar_size.y) / 2;
    fields = new volume_bar(bar_x, music_bar_y, bar_x + bar_size.x - 1, music_bar_y + bar_size.y - 1, slider,
                            &music_volume, pal->find_closest(40, 0, 0), fields);
    const int sfx_controls_y = sfx_row_y + (row_height - down_size.y) / 2;
    const int sfx_bar_y = sfx_row_y + (row_height - bar_size.y) / 2;
    fields = new volume_bar(bar_x, sfx_bar_y, bar_x + bar_size.x - 1, sfx_bar_y + bar_size.y - 1, slider, &sfx_volume,
                            pal->find_closest(40, 0, 0), fields);
    fields = new info_field(
        content_x, sfx_row_y + (row_height - font_height) / 2, 0, symbol_str("SFXv"),
        new ico_button(
            down_x, sfx_controls_y, ID_SFX_DOWN, d_u, d_d, d_ua, d_da,
            new ico_button(up_x, sfx_controls_y, ID_SFX_UP, u_u, u_d, u_ua, u_da,
                           new info_field(content_x, music_row_y + (row_height - font_height) / 2, 0,
                                          symbol_str("MUSICv"),
                                          new ico_button(down_x, music_controls_y, ID_MUSIC_DOWN, d_u, d_d, d_ua, d_da,
                                                         new ico_button(up_x, music_controls_y, ID_MUSIC_UP, u_u, u_d,
                                                                        u_ua, u_da, fields))))));

    const int content_bottom = soundfont_picker_y + soundfont_rows * (font_height + 1) + 5 + padding * 2;
    const ivec2 client_size(client_width, content_bottom - client_top);
    const ivec2 total_size = client_size + ivec2(Jwindow::left_border() + Jwindow::right_border(),
                                                 Jwindow::top_border() + Jwindow::bottom_border());
    ivec2 position(prop->getd("volume_x", (xres - total_size.x) / 2),
                   prop->getd("volume_y", (yres - total_size.y) / 2));
    position.x = std::max(0, std::min(position.x, xres - total_size.x));
    position.y = std::max(0, std::min(position.y, yres - total_size.y));
    m_window = wm->CreateWindow(position, client_size, fields, symbol_str("ic_volume"));
}

std::string AudioSettingsWindow::selected_soundfont()
{
    pick_list *picker = static_cast<pick_list *>(m_window->inm->get(ID_SOUNDFONT_PICKER));
    const int selection = picker ? picker->get_selection() : -1;
    if (selection < 0 || selection >= static_cast<int>(soundfont_values.size()))
        return {};
    return soundfont_values[selection];
}

void AudioSettingsWindow::select_soundfont(const std::string &soundfont)
{
    const auto selection = std::find(soundfont_values.begin(), soundfont_values.end(), soundfont);
    if (selection == soundfont_values.end())
        return;

    soundfont_picker *picker = static_cast<soundfont_picker *>(m_window->inm->get(ID_SOUNDFONT_PICKER));
    if (picker)
        picker->set_x_silently(static_cast<int>(selection - soundfont_values.begin()), m_window->m_surf);
}

void AudioSettingsWindow::draw_sfx_vol()
{
    m_window->redraw();
    wm->flush_screen();
}

void AudioSettingsWindow::draw_music_vol()
{
    m_window->redraw();
    wm->flush_screen();
}
