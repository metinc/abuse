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

#include "cache.h"
#include "clisp.h"
#include "dev.h"
#include "general_settings.h"
#include "id.h"
#include "jwindow.h"
#include "lisp.h"
#include "netcfg.h"
#include "scroller.h"
#include "view.h"

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

constexpr std::array<const char *, PLAYER_SKIN_COUNT> skin_label_symbols = {
    "skin_standard", "skin_blue",   "skin_yellow", "skin_fire",   "skin_olive", "skin_pink",
    "skin_darkblue", "skin_purple", "skin_africa", "skin_gold",   "skin_land"};

constexpr int skin_picker_rows = 6;
constexpr int skin_preview_width = 54;
constexpr int skin_preview_height = 58;

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

class skin_picker : public pick_list
{
    int minimum_item_width;

  public:
    skin_picker(int X, int Y, int ID, char **labels, int selected, int MinimumItemWidth, ifield *Next)
        : pick_list(X, Y, ID, skin_picker_rows, labels, PLAYER_SKIN_COUNT, selected, Next, nullptr, false),
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
};

class skin_preview : public ifield
{
    int bottom_sprite_id;
    int top_sprite_id;

  public:
    skin_preview(int X, int Y, ifield *Next)
        : bottom_sprite_id(cache.reg("art/cop.spe", "stopped0001.pcx", SPEC_CHARACTER, 1)),
          top_sprite_id(cache.reg("art/coptop.spe", "4gma0001.pcx", SPEC_CHARACTER, 1))
    {
        m_pos = ivec2(X, Y);
        id = ID_NULL;
        next = Next;
    }

    void area(int &x1, int &y1, int &x2, int &y2) override
    {
        x1 = m_pos.x;
        y1 = m_pos.y;
        x2 = m_pos.x + skin_preview_width - 1;
        y2 = m_pos.y + skin_preview_height - 1;
    }

    void draw_first(image *screen) override
    {
        uint8_t *tint = nullptr;
        if (settings.player_skin != 0)
        {
            const int tint_id = lnumber_value(
                ((LArray *)((LSymbol *)l_player_tints)->GetValue())->Get(settings.player_skin));
            tint = cache.ctint(tint_id)->data;
        }

        const ivec2 anchor(m_pos.x + skin_preview_width / 2,
                           m_pos.y + (skin_preview_height + 29) / 2 - 1);
        auto draw_part = [screen, anchor, tint](figure *part) {
            TransImage *sprite = part->forward;
            const ivec2 position(anchor.x - part->xcfg, anchor.y - sprite->Size().y + 1);
            if (tint)
                sprite->PutRemap(screen, position, tint);
            else
                sprite->PutImage(screen, position);
        };

        draw_part(cache.fig(bottom_sprite_id));
        draw_part(cache.fig(top_sprite_id));
    }

    void draw(int active, image *screen) override
    {
        (void)active;
        draw_first(screen);
    }

    void handle_event(Event &event, image *screen, InputManager *input) override
    {
        (void)event;
        (void)screen;
        (void)input;
    }

    int selectable() override
    {
        return 0;
    }

    char *read() override
    {
        return nullptr;
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
        std::array<char *, skin_label_symbols.size()> skin_labels;
        int selected = 0;
        int longest_label = 0;
        const std::string effective_language = settings.GetEffectiveLanguage();
        for (size_t index = 0; index < languages.size(); ++index)
        {
            labels[index] = const_cast<char *>(symbol_str(languages[index].label_symbol));
            longest_label = std::max(longest_label, text_width(labels[index]));
            if (effective_language == languages[index].value)
                selected = static_cast<int>(index);
        }
        int longest_skin_label = 0;
        for (size_t index = 0; index < skin_label_symbols.size(); ++index)
        {
            skin_labels[index] = const_cast<char *>(symbol_str(skin_label_symbols[index]));
            longest_skin_label = std::max(longest_skin_label, text_width(skin_labels[index]));
        }

        const int font_height = wm->font()->Size().y;
        const int padding = 8;
        const int content_x = padding - 2;
        const int label_y = padding / 2;
        const int picker_y = label_y + font_height + 3;
        const int language_natural_width = longest_label + padding * 2 + 8;
        const int skin_natural_width = longest_skin_label + skin_preview_width + padding * 3 + 20;
        const int client_width = std::max({settings.big_font ? 260 : 220, language_natural_width, skin_natural_width});
        const int language_picker_width = client_width - padding * 2;

        const int language_picker_bottom = picker_y + static_cast<int>(languages.size()) * (font_height + 1) + 4;
        const int skin_label_y = language_picker_bottom + 6;
        const int skin_picker_y = skin_label_y + font_height + 3;
        const int preview_x = client_width - padding - skin_preview_width;
        const int skin_picker_width = preview_x - content_x - padding - 12;

        skin_preview *preview = new skin_preview(preview_x, skin_picker_y, nullptr);
        skin_picker *skin = new skin_picker(content_x, skin_picker_y, ID_PLAYER_SKIN_PICKER, skin_labels.data(),
                                            settings.player_skin, skin_picker_width - 4, preview);
        info_field *skin_label =
            new info_field(content_x, skin_label_y, ID_NULL, symbol_str("player_skin"), skin);
        language_picker *picker = new language_picker(content_x, picker_y, ID_LANGUAGE_PICKER, labels.data(), selected,
                                                      language_picker_width - 4, skin_label);
        info_field *fields = new info_field(content_x, label_y, ID_NULL, symbol_str("language"), picker);

        const int skin_picker_bottom = skin_picker_y + skin_picker_rows * (font_height + 1) + 4;
        const int content_bottom = std::max(skin_picker_bottom, skin_picker_y + skin_preview_height) + padding;
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
                    const std::string previous_language = settings.language;
                    settings.language = selected_language;
                    const std::string effective_language = settings.GetEffectiveLanguage();
                    if (!apply_language(effective_language))
                    {
                        settings.language = previous_language;
                        fprintf(stderr, "Unable to load language '%s'\n", effective_language.c_str());
                    }
                    else
                    {
                        if (!settings.Save())
                            fprintf(stderr, "Unable to save language setting\n");
                    }
                    rebuild = true;
                    continue;
                }
            }

            if (event.type == EV_MESSAGE && event.message.id == ID_PLAYER_SKIN_PICKER)
            {
                const int selected_skin = skin->get_selection();
                if (selected_skin != settings.player_skin)
                {
                    settings.player_skin = selected_skin;
                    if (!main_net_cfg || main_net_cfg->state == net_configuration::SINGLE_PLAYER ||
                        main_net_cfg->state == net_configuration::RESTART_SINGLE)
                    {
                        for (view *current = player_list; current; current = current->next)
                            if (current->local_player() && current->m_focus)
                                current->set_tint(selected_skin);
                    }
                    if (!settings.Save())
                        fprintf(stderr, "Unable to save player skin setting\n");
                    window->redraw();
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
