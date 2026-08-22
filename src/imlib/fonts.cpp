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

#include <ctype.h>

#include "common.h"

#include "fonts.h"

unsigned char JCFont::MapUTF8Char(std::string_view::iterator &it, const std::string_view::iterator &end)
{
    unsigned char ch = static_cast<unsigned char>(*it);

    // Handle ASCII characters (0-127)
    if (ch < 0x80)
    {
        ++it;
        return ch;
    }

    // Handle the two-byte UTF-8 sequences supported by the bitmap font.
    if (ch == 0xC3)
    {
        if ((it + 1) == end)
        {
            ++it;
            return 0;
        }

        unsigned char next_byte = static_cast<unsigned char>(*(it + 1));
        it += 2; // Consume both bytes

        switch (next_byte)
        {
        // German special characters
        case 0xA4:
            return 132; // ä
        case 0x84:
            return 142; // Ä
        case 0xB6:
            return 148; // ö
        case 0x96:
            return 153; // Ö
        case 0xBC:
            return 129; // ü
        case 0x9C:
            return 154; // Ü
        case 0x9F:
            return 225; // ß

        // French special characters
        case 0xA2:
            return 131; // â
        case 0x82:
            return 131; // Â
        case 0xB9:
            return 151; // ù
        case 0x99:
            return 151; // Ù
        case 0xA9:
            return 130; // é
        case 0x89:
            return 130; // É
        case 0xA8:
            return 138; // è
        case 0x88:
            return 138; // È
        case 0xAA:
            return 136; // ê
        case 0x8A:
            return 136; // Ê
        case 0xA0:
            return 133; // à
        case 0x80:
            return 133; // À
        case 0xA7:
            return 135; // ç
        case 0x87:
            return 135; // Ç
        case 0xB4:
            return 147; // ô
        case 0x94:
            return 147; // Ô
        case 0xBB:
            return 150; // û
        case 0x9B:
            return 150; // Û
        case 0xAB:
            return 137; // ë
        case 0x8B:
            return 137; // Ë
        case 0xAF:
            return 139; // ï
        case 0x8F:
            return 139; // Ï

        default:
            return 0;
        }
    }

    // Ignore unsupported, well-formed UTF-8 characters as a whole. This keeps
    // pasted characters such as emoji from turning into several font glyphs.
    int sequence_length = 0;
    if (ch >= 0xC2 && ch <= 0xDF)
        sequence_length = 2;
    else if (ch >= 0xE0 && ch <= 0xEF)
        sequence_length = 3;
    else if (ch >= 0xF0 && ch <= 0xF4)
        sequence_length = 4;

    if (sequence_length > 0)
    {
        ++it;
        for (int i = 1; i < sequence_length && it != end; ++i)
        {
            const unsigned char continuation = static_cast<unsigned char>(*it);
            if (continuation < 0x80 || continuation > 0xBF)
                break;
            ++it;
        }
        return 0;
    }

    // Preserve standalone bytes that have already been converted to the
    // font's legacy encoding, as used by chat messages over the network.
    ++it;
    return ch;
}

std::string JCFont::EncodeForFont(std::string_view text)
{
    std::string encoded;
    encoded.reserve(text.size());

    auto it = text.begin();
    const auto end = text.end();
    while (it != end)
    {
        const unsigned char mapped = MapUTF8Char(it, end);
        if (mapped > 0)
            encoded.push_back(static_cast<char>(mapped));
    }
    return encoded;
}

void JCFont::PutString(image *screen, ivec2 pos, std::string_view text, int color)
{
    for (const unsigned char mapped : EncodeForFont(text))
    {
        PutChar(screen, pos, mapped, color);
        pos.x += m_size.x;
    }
}

void JCFont::PutChar(image *screen, ivec2 pos, unsigned char ch, int color)
{
    if (!m_data[ch])
        return;

    if (color >= 0)
        m_data[ch]->PutColor(screen, pos, color);
    else
        m_data[ch]->PutImage(screen, pos);
}

JCFont::JCFont(image *letters)
{
    m_size = (letters->Size() + ivec2(1)) / ivec2(32, 8);

    image tmp(m_size);

    for (int ch = 0; ch < 256; ch++)
    {
        tmp.clear();
        tmp.PutPart(letters, ivec2(0), ivec2(ch % 32, ch / 32) * m_size, ivec2(ch % 32 + 1, ch / 32 + 1) * m_size, 1);
        m_data[ch] = new TransImage(&tmp, "JCfont");
    }
}

JCFont::~JCFont()
{
    for (int ch = 0; ch < 256; ch++)
        delete m_data[ch];
}
