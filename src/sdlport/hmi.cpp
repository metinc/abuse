//
//  Abuse - dark 2D side-scrolling platform game
//
//  Copyright (c) 2011 Jochen Schleu <jjs@jjs.at>
//  Copyright (c) 2024 Andrej Pancik
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the Do What The Fuck You Want To
//   Public License, Version 2, as published by Sam Hocevar. See
//   http://sam.zoy.org/projects/COPYING.WTFPL for more details.
//

#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <array>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <memory>

#include "file_utils.h"
#include "common.h"

// Load Abuse HMI files and covert them to standard Midi format
//
// HMI files differ from Midi files in the following ways:
// - there is a header giving offsets to the tracks and various other
//   information (unknown)
// - note-on events include the duration of the note instead of dedicated
//   note-off events
// - additional 0xFE event with variable length, purpose unknown
//
// This converter does the bare minimum to get Abuse HMI files to convert.
// The bpm and header information is fixed and not read from the file (except
// the number of tracks). HMI files make use of running status notation, the
// converted files don't.

constexpr size_t MAX_NOTE_OFF_EVENTS = 30;
constexpr uint32_t INVALID_TIME = 0xFFFFFFFF;
constexpr uint32_t HMI_TRACK_DATA_OFFSET = 0x57;
constexpr uint32_t HMI_TRACK_OFFSET_POS = 0xE8;
constexpr uint32_t HMI_NEXT_CHUNK_POS = 0xF4;

struct NoteOffEvent
{
    uint32_t time{INVALID_TIME};
    uint8_t command{0};
    uint8_t note{0};

    bool operator<(const NoteOffEvent &other) const
    {
        return time < other.time;
    }
};

static uint32_t get_int_from_buffer(const uint8_t *buffer)
{
    return (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
}

static void write_big_endian_number(uint32_t value, uint8_t *buffer)
{
    buffer[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    buffer[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buffer[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buffer[3] = static_cast<uint8_t>(value & 0xFF);
}

// Variable length number code
// from: http://www.chriswareham.demon.co.uk/midifiles/variable_length.html
static uint32_t read_time_value(uint8_t *&buffer)
{
    uint32_t value;
    uint8_t c;

    if ((value = *buffer++) & 0x80)
    {
        value &= 0x7F;
        do
        {
            value = (value << 7) | ((c = *buffer++) & 0x7F);
        } while (c & 0x80);
    }

    return value;
}

static void write_time_value(uint32_t time, uint8_t *&buffer)
{
    std::vector<uint8_t> bytes;
    uint32_t value = time;

    bytes.push_back(value & 0x7F);
    while (value >>= 7)
    {
        bytes.push_back((value & 0x7F) | 0x80);
    }

    for (auto it = bytes.rbegin(); it != bytes.rend(); ++it)
    {
        *buffer++ = *it;
    }
}

static void remember_note_off_event(std::array<NoteOffEvent, MAX_NOTE_OFF_EVENTS> &note_off_events, uint32_t time,
                                    uint8_t cmd, uint8_t note)
{
    auto it = std::find_if(note_off_events.begin(), note_off_events.end(),
                           [](const auto &e) { return e.time == INVALID_TIME; });

    if (it != note_off_events.end())
    {
        *it = NoteOffEvent{time, cmd, note};
    }

    std::sort(note_off_events.begin(), note_off_events.end());
}

static void process_note_off_events(std::array<NoteOffEvent, MAX_NOTE_OFF_EVENTS> &note_off_events,
                                    uint32_t current_time, uint32_t &last_time, uint8_t *&buffer)
{
    for (auto &event : note_off_events)
    {
        if (event.time == INVALID_TIME)
            break;
        if (event.time >= current_time)
            continue;

        write_time_value(event.time - last_time, buffer);
        last_time = event.time;

        *buffer++ = event.command;
        *buffer++ = event.note;
        *buffer++ = 0x00;

        event.time = INVALID_TIME;
    }

    std::sort(note_off_events.begin(), note_off_events.end());
}

static void process_hmi_event(uint8_t value, uint8_t *&input)
{
    switch (value)
    {
    case 0x10:
        input += 2;
        input += *input;
        input += 5;
        break;
    case 0x14:
        input += 2;
        break;
    case 0x15:
        input += 6;
        break;
    }
}

static void convert_hmi_track(uint8_t *input, uint32_t input_size, uint8_t *&output)
{
    uint8_t *const start_of_buffer = output;
    uint8_t *const start_of_input = input;
    std::array<NoteOffEvent, MAX_NOTE_OFF_EVENTS> note_off_events{};

    input += input[HMI_TRACK_DATA_OFFSET];

    // Write track header
    const uint8_t track_header[] = {0x4D, 0x54, 0x72, 0x6B, 0x00, 0x00, 0x00, 0x00};
    std::memcpy(output, track_header, 8);
    output += 8;

    bool done = false;
    uint8_t current_command = 0;
    uint32_t current_time = 0;
    uint32_t last_time = 0;

    while (!done && static_cast<uint32_t>(input - start_of_input) < input_size)
    {
        current_time += read_time_value(input);

        uint8_t current_value = *input++;
        if (current_value >= 0x80)
        {
            current_command = current_value;
            current_value = *input++;
        }

        process_note_off_events(note_off_events, current_time, last_time, output);

        if (current_command != 0xFE)
        {
            write_time_value(current_time - last_time, output);
            last_time = current_time;
            *output++ = current_command;
        }

        switch (current_command & 0xF0)
        {
        case 0xC0: // Program change
        case 0xD0: // Channel aftertouch
            *output++ = current_value;
            break;

        case 0x80: // Note off
        case 0xA0: // Aftertouch
        case 0xB0: // Controller
        case 0xE0: // Pitch bend
            *output++ = current_value;
            *output++ = *input++;
            break;

        case 0x90: // Note on with duration
            *output++ = current_value;
            *output++ = *input++;
            remember_note_off_event(note_off_events, current_time + read_time_value(input), current_command,
                                    current_value);
            break;

        case 0xF0: // Meta event
            if (current_command == 0xFE)
            {
                process_hmi_event(current_value, input);
            }
            else
            {
                *output++ = current_value;
                *output++ = *input++;
                done = true;
            }
            break;
        }
    }

    if (!done)
    {
        const uint8_t end_marker[] = {0x00, 0xFF, 0x2F, 0x00};
        std::memcpy(output, end_marker, 4);
        output += 4;
    }

    write_big_endian_number(static_cast<uint32_t>(output - start_of_buffer - 8), &start_of_buffer[4]);
}

uint8_t *load_hmi(char const *filename, uint32_t &data_size)
{
    if (!filename)
        return nullptr;

    FILE *hmifile = prefix_fopen(filename, "rb");
    if (hmifile == nullptr)
        return nullptr;

    if (fseek(hmifile, 0, SEEK_END) != 0)
    {
        fclose(hmifile);
        return nullptr;
    }
    uint32_t buffer_size = ftell(hmifile);
    if (fseek(hmifile, 0, SEEK_SET) != 0)
    {
        fclose(hmifile);
        return nullptr;
    }

    auto input_buffer = std::make_unique<uint8_t[]>(buffer_size);
    if (fread(input_buffer.get(), 1, buffer_size, hmifile) != buffer_size)
    {
        fclose(hmifile);
        return nullptr;
    }
    fclose(hmifile);

    auto output_buffer = std::make_unique<uint8_t[]>(buffer_size * 10);
    uint8_t *output_buffer_ptr = output_buffer.get();

    uint32_t offset_tracks = get_int_from_buffer(&input_buffer[HMI_TRACK_OFFSET_POS]);
    uint32_t next_offset = get_int_from_buffer(&input_buffer[HMI_NEXT_CHUNK_POS]);
    uint8_t num_tracks = static_cast<uint8_t>((next_offset - offset_tracks) / sizeof(uint32_t));

    // Write MIDI header
    const uint8_t midi_header[] = {
        0x4D, 0x54,
        0x68, 0x64, // Magic "MThd"
        0x00, 0x00,
        0x00, 0x06, // Header length
        0x00, 0x01, // Format type
        0x00, static_cast<uint8_t>(num_tracks + 1), // Number of tracks
        0x00, 0xC0 // Time division
    };
    std::memcpy(output_buffer_ptr, midi_header, sizeof(midi_header));
    output_buffer_ptr += sizeof(midi_header);

    // Write tempo track
    const uint8_t tempo_track[] = {
        0x4D, 0x54, 0x72, 0x6B, // Magic "MTrk"
        0x00, 0x00, 0x00, 0x0B, // Track length
        0x00, 0xFF, 0x51, 0x03, // Tempo meta event
        0x18, 0x7F, 0xFF, // Tempo value
        0x00, 0xFF, 0x2F, 0x00 // End of track
    };
    std::memcpy(output_buffer_ptr, tempo_track, sizeof(tempo_track));
    output_buffer_ptr += sizeof(tempo_track);

    // Convert each track
    for (int i = 0; i < num_tracks; i++)
    {
        uint32_t track_position = get_int_from_buffer(&input_buffer[offset_tracks + i * sizeof(uint32_t)]);
        uint32_t track_size =
            (i == num_tracks - 1)
                ? buffer_size - track_position
                : get_int_from_buffer(&input_buffer[offset_tracks + (i + 1) * sizeof(uint32_t)]) - track_position;

        convert_hmi_track(&input_buffer[track_position], track_size, output_buffer_ptr);
    }

    data_size = static_cast<uint32_t>(output_buffer_ptr - output_buffer.get());
    uint8_t *final_buffer = static_cast<uint8_t *>(malloc(data_size));
    std::memcpy(final_buffer, output_buffer.get(), data_size);

    return final_buffer;
}