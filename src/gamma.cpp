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

#include <math.h>
#include <fstream>

#include "common.h"

#include "game.h"

#include "jwindow.h"
#include "lisp.h"
#include "scroller.h"
#include "id.h"
#include "cache.h"
#include "loader2.h"

extern int dev_ok;
palette *old_pal = nullptr;

//AR
#include "sdlport/setup.h"
extern Settings settings;
//

bool write_gamma_lsp(long darkest_gray)
{
    // Build output file path
    std::string gamma_path = std::string(get_save_filename_prefix()) + "gamma.lsp";

    // Attempt to open for binary write
    std::ofstream outfile(gamma_path, std::ios::binary);
    if (!outfile)
    {
        std::cerr << "Unable to write to file " << gamma_path << std::endl;
        return false;
    }

    outfile << "(setq darkest_gray " << darkest_gray << ")\n";
    outfile.close();

    // Store darkest_gray in LSpace
    LSpace *previous_space = LSpace::Current;
    LSpace::Current = &LSpace::Perm;
    LSymbol::FindOrCreate("darkest_gray")->SetNumber(darkest_gray);
    LSpace::Current = previous_space;

    return true;
}

class gray_picker : public spicker
{
  public:
    int sc;

    // Draws a single item (little bar) in the picker
    void draw_item(image *screen, int x, int y, int num, int active)
    {
        long x2 = x + item_width() - 1;
        long y2 = y + item_height() - 1;

        screen->Bar(ivec2(x, y), ivec2(x2, y2), 0);
        screen->Bar(ivec2(x, y), ivec2(x2 - 3, y2), sc + num);

        if (active)
        {
            screen->Rectangle(ivec2(x, y), ivec2(x2, y2), 255);
        }
    }

    void set_pos(int x)
    {
        cur_sel = x;
    }

    int total()
    {
        return 32;
    }

    int item_width()
    {
        return 12;
    }

    int item_height()
    {
        return 20;
    }

    int activate_on_mouse_move()
    {
        return 0;
    }

    // Constructor
    gray_picker(int X, int Y, int ID, int start, int current, ifield *Next)
        : spicker(X, Y, ID, 1, 20, 0, 0, Next), sc(start)
    {
        cur_sel = current;
        reconfigure();
        cur_sel = current;
    }
};

// Helper for retrieving language strings
static char const *lang_string(char const *symbol)
{
    LSymbol *v = LSymbol::Find(symbol);
    if (!v || !DEFINEDP(v->GetValue()))
        return "Language symbol missing!";
    return lstring_value(v->GetValue());
}

// Gamma correction function
void gamma_correct(palette *&pal, int force_menu)
{
    long darkest_gray = 0, darkest_gray_old = 0;
    bool abort_menu = false;

    // Check if user previously set darkest_gray
    LSymbol *gs = LSymbol::Find("darkest_gray");

    // If there's an old palette, restore it before modifying
    if (old_pal)
    {
        delete pal;
        pal = old_pal;
        old_pal = nullptr;
    }

    if (!force_menu)
    {
        // If darkest_gray is defined and menu wasn't forced, just use it
        if (gs && DEFINEDP(gs->GetValue()))
        {
            darkest_gray = lnumber_value(gs->GetValue());
        }
        else
        {
            darkest_gray = 16;
            if (!write_gamma_lsp(darkest_gray))
            {
                std::cerr << "Failed to write gamma.lsp\n";
            }
        }
    }
    else
    {
        // Remember old darkest_gray
        if (gs && DEFINEDP(gs->GetValue()))
        {
            darkest_gray = darkest_gray_old = lnumber_value(gs->GetValue());
        }

        // Create a temporary grayscale palette
        palette *gray_pal = pal->copy();
        const int total_colors = 32;
        for (int i = 0; i < total_colors; i++)
        {
            gray_pal->set(i, i * 4, i * 4, i * 4);
        }
        gray_pal->load();

        // Save current colors from window manager
        int wm_bc = wm->bright_color();
        int wm_mc = wm->medium_color();
        int wm_dc = wm->dark_color();

        // Adjust them for bright/medium/dark
        auto clamp = [](int val) { return std::max(0, std::min(255, val)); };
        int br_r = clamp(pal->red(wm_bc) + 20);
        int br_g = clamp(pal->green(wm_bc) + 20);
        int br_b = clamp(pal->blue(wm_bc) + 20);

        int md_r = clamp(pal->red(wm_mc) - 20);
        int md_g = clamp(pal->green(wm_mc) - 20);
        int md_b = clamp(pal->blue(wm_mc) - 20);

        int dr_r = clamp(pal->red(wm_dc) - 40);
        int dr_g = clamp(pal->green(wm_dc) - 40);
        int dr_b = clamp(pal->blue(wm_dc) - 40);

        wm->set_colors(gray_pal->find_closest(br_r, br_g, br_b), gray_pal->find_closest(md_r, md_g, md_b),
                       gray_pal->find_closest(dr_r, dr_g, dr_b));

        // Set window size
        int w_w = 254;
        int w_h = 100;
        if (settings.big_font)
        {
            w_w = 330;
            w_h = 110;
        }

        // Position for the gray_picker
        int px_gp = w_w / 2 - 244 / 2;
        int px_ok = w_w / 2 - 32 / 2;

        int sh = wm->font()->Size().y;

        // OK button
        button *but = new button(px_ok, sh * 10, ID_GAMMA_OK, cache.img(ok_button),
                                 new info_field(2, sh, ID_NULL, lang_string("gamma_msg"), 0));

        // Gray picker itself
        gray_picker *gp = new gray_picker(px_gp, sh * 4, ID_GREEN_PICKER, 0, darkest_gray / 4, but);
        gp->set_pos(darkest_gray / 4);

        // Create window
        Jwindow *gw = wm->CreateWindow(ivec2(xres / 2 - w_w / 2, yres / 2 - w_h / 2), ivec2(w_w, w_h), gp);

        // Event loop
        Event ev;
        wm->flush_screen();
        do
        {
            do
            {
                wm->get_event(ev);
            } while (ev.type == EV_MOUSE_MOVE && wm->IsPending());

            wm->flush_screen();

            if (ev.type == EV_CLOSE_WINDOW || (ev.type == EV_KEY && ev.key == JK_ESC))
                abort_menu = true;
        } while (!abort_menu && (ev.type != EV_MESSAGE || ev.message.id != ID_GAMMA_OK));

        // Retrieve new darkest_gray from picker
        darkest_gray = static_cast<spicker *>(gw->inm->get(ID_GREEN_PICKER))->first_selected() * 4;

        // Close the window
        wm->close_window(gw);
        wm->flush_screen();

        // Restore original colors and free the gray palette
        wm->set_colors(wm_bc, wm_mc, wm_dc);
        delete gray_pal;

        // If the user didn’t abort, write gamma.lsp
        if (!abort_menu)
        {
            if (!write_gamma_lsp(darkest_gray))
            {
                std::cerr << "Failed to write gamma.lsp\n";
            }
        }
    }

    // If user aborted, revert to old darkest_gray
    if (abort_menu)
        darkest_gray = darkest_gray_old;

    // Clamp darkest_gray
    if (darkest_gray < 1)
        darkest_gray = 1;
    else if (darkest_gray > 128)
        darkest_gray = 128;

    // A darkest_gray of 16 yields a neutral gamma of 1.0
    double gamma_val = std::log(darkest_gray / 255.0) / std::log(16.0 / 255.0);
    std::cout << "Setting gamma to " << gamma_val << " (darkest_gray=" << darkest_gray << ")\n";

    // Build the new gamma-corrected palette
    old_pal = pal; // keep pointer to old one
    pal = new palette;

    for (int i = 0; i < 256; i++)
    {
        uint8_t oldr, oldg, oldb;
        old_pal->get(i, oldr, oldg, oldb);

        int nr = static_cast<int>(std::pow(oldr / 255.0, gamma_val) * 255);
        int ng = static_cast<int>(std::pow(oldg / 255.0, gamma_val) * 255);
        int nb = static_cast<int>(std::pow(oldb / 255.0, gamma_val) * 255);

        pal->set(i, nr, ng, nb);
    }

    pal->load();
}
