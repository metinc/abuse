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

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <string_view>

#include "common.h"

#include "dev.h"
#include "general_settings.h"
#include "id.h"
#include "jwindow.h"
#include "lisp.h"
#include "scroller.h"

#include "sdlport/setup.h"

extern Settings settings;

namespace
{
struct language_option
{
    const char *label_symbol;
    const char *value;
};

constexpr std::array<language_option, 3> languages = {
    {{"language_english", "english"}, {"language_german", "german"}, {"language_french", "french"}}};

class language_picker : public pick_list
{
    int minimum_item_width;

  public:
    language_picker(int X, int Y, int ID, char **labels, int selected, int MinimumItemWidth, ifield *Next)
        : pick_list(X, Y, ID, static_cast<int>(languages.size()), labels, static_cast<int>(languages.size()), selected,
                    Next, nullptr, false),
          minimum_item_width(MinimumItemWidth)
    {
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
        const int previous = get_selection();
        pick_list::set_x(x, screen);
        if (get_selection() != previous)
            wm->PushMessage(id, this);
    }

    void handle_up(image *screen, InputManager *inm) override
    {
        if (cur_sel > 0)
        {
            cur_sel--;
            scroll_event(0, screen);
            note_new_current(screen, inm, cur_sel);
        }
    }

    void handle_down(image *screen, InputManager *inm) override
    {
        if (cur_sel + 1 < total())
        {
            cur_sel++;
            scroll_event(0, screen);
            note_new_current(screen, inm, cur_sel);
        }
    }
};

int text_width(std::string_view text)
{
    return static_cast<int>(JCFont::EncodeForFont(text).size()) * wm->font()->Size().x;
}

bool apply_language(const std::string &language)
{
    const auto option = std::find_if(languages.begin(), languages.end(),
                                     [&language](const language_option &entry) { return entry.value == language; });
    if (option == languages.end())
        return false;

    LSpace *previous_space = LSpace::Current;
    LSpace::Current = &LSpace::Perm;

    LSymbol *section = LSymbol::FindOrCreate("section");
    LObject *previous_section = section->GetValue();
    section->SetValue(LSymbol::FindOrCreate("game_section"));

    std::string command = "(load \"lisp/" + language + ".lsp\")";
    char const *command_source = command.c_str();
    const bool loaded = leval(LObject::Compile(command_source)) != nullptr;
    section->SetValue(previous_section);
    if (loaded)
    {
        LSymbol *current_language = LSymbol::FindOrCreate("current_language");
        current_language->SetValue(LString::Create(language.c_str()));
    }

    LSpace::Current = previous_space;
    return loaded;
}
}

void show_general_settings()
{
    while (true)
    {
        std::array<char *, languages.size()> labels;
        int selected = 0;
        int longest_label = 0;
        for (size_t index = 0; index < languages.size(); ++index)
        {
            labels[index] = const_cast<char *>(symbol_str(languages[index].label_symbol));
            longest_label = std::max(longest_label, text_width(labels[index]));
            if (settings.language == languages[index].value)
                selected = static_cast<int>(index);
        }

        const int font_height = wm->font()->Size().y;
        const int padding = 8;
        const int content_x = padding - 2;
        const int label_y = padding / 2;
        const int picker_y = label_y + font_height + 3;
        const int natural_width = longest_label + padding * 2 + 8;
        const int client_width = std::max(settings.big_font ? 240 : 180, natural_width);
        const int picker_width = client_width - padding * 2;

        language_picker *picker = new language_picker(content_x, picker_y, ID_LANGUAGE_PICKER, labels.data(), selected,
                                                      picker_width - 4, nullptr);
        info_field *fields = new info_field(content_x, label_y, ID_NULL, symbol_str("language"), picker);

        const int content_bottom = picker_y + static_cast<int>(languages.size()) * (font_height + 1) + padding;
        const ivec2 client_size(client_width, content_bottom);
        const ivec2 total_size = client_size + ivec2(Jwindow::left_border() + Jwindow::right_border(),
                                                     Jwindow::top_border() + Jwindow::bottom_border());
        Jwindow *window = wm->CreateWindow(ivec2((xres - total_size.x) / 2, (yres - total_size.y) / 2), client_size,
                                           fields, symbol_str("ic_general"));

        bool rebuild = false;
        bool close_requested = false;
        Event event;
        wm->flush_screen();
        while (!rebuild && !close_requested)
        {
            do
            {
                wm->get_event(event);
            } while (event.type == EV_MOUSE_MOVE && wm->IsPending());

            if (event.type == EV_CLOSE_WINDOW || (event.type == EV_KEY && event.key == JK_ESC))
            {
                close_requested = true;
                continue;
            }

            if (event.type == EV_MESSAGE && event.message.id == ID_LANGUAGE_PICKER)
            {
                const std::string selected_language = languages[picker->get_selection()].value;
                if (selected_language != settings.language)
                {
                    if (!apply_language(selected_language))
                    {
                        fprintf(stderr, "Unable to load language '%s'\n", selected_language.c_str());
                    }
                    else
                    {
                        settings.language = selected_language;
                        if (!settings.Save())
                            fprintf(stderr, "Unable to save language setting\n");
                    }
                    rebuild = true;
                    continue;
                }
            }

            wm->flush_screen();
        }

        wm->close_window(window);
        if (!close_requested)
            continue;
        wm->flush_screen();
        return;
    }
}
