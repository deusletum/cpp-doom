//
// Copyright(C) 2013 James Haley et al.
// Copyright(C) 2017 Alex Mayfield
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     Client Interface to Midi Server
//

#if _WIN32

#include <cstdlib>
#include <sys/stat.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "i_midipipe.hpp"

#include "config.h"
#include "i_sound.hpp"
#include "i_timer.hpp"
#include "m_misc.hpp"
#include "net_packet.hpp"

#include "../midiproc/proto.hpp"

#if defined(_DEBUG)
#define DEBUGOUT(s) puts(s)
#else
#define DEBUGOUT(s)
#endif

//=============================================================================
//
// Public Data
//

// True if the midi proces was initialized at least once and has not been
// explicitly shut down.  This remains true if the server is momentarily
// unreachable.
bool midi_server_initialized = false;

// True if the current track is being handled via the MIDI server.
bool midi_server_registered = false;

//=============================================================================
//
// Data
//

#define MIDIPIPE_MAX_WAIT 1000 // Max amount of ms to wait for expected data.

static HANDLE midi_process_in_reader; // Input stream for midi process.
static HANDLE midi_process_in_writer;
static HANDLE midi_process_out_reader; // Output stream for midi process.
static HANDLE midi_process_out_writer;

//=============================================================================
//
// Private functions
//

//
// FreePipes
//
// Free all pipes in use by this module.
//
static void FreePipes()
{
    if (midi_process_in_reader != nullptr)
    {
        CloseHandle(midi_process_in_reader);
        midi_process_in_reader = nullptr;
    }
    if (midi_process_in_writer != nullptr)
    {
        CloseHandle(midi_process_in_writer);
        midi_process_in_writer = nullptr;
    }
    if (midi_process_out_reader != nullptr)
    {
        CloseHandle(midi_process_out_reader);
        midi_process_in_reader = nullptr;
    }
    if (midi_process_out_writer != nullptr)
    {
        CloseHandle(midi_process_out_writer);
        midi_process_out_writer = nullptr;
    }
}

//
// UsingNativeMidi
//
// Enumerate all music decoders and return true if NATIVEMIDI is one of them.
//
// If this is the case, using the MIDI server is probably necessary.  If not,
// we're likely using Timidity and thus don't need to start the server.
//
static bool UsingNativeMidi()
{
    int i;
    int decoders = Mix_GetNumMusicDecoders();

    for (i = 0; i < decoders; i++)
    {
        if (strcmp(Mix_GetMusicDecoder(i), "NATIVEMIDI") == 0)
        {
            return true;
        }
    }

    return false;
}

//
// WritePipe
//
// Writes packet data to the subprocess' standard in.
//
static bool WritePipe(net_packet_t *packet)
{
    DWORD bytes_written;
    BOOL  ok = WriteFile(midi_process_in_writer, packet->data, static_cast<DWORD>(packet->len),
        &bytes_written, nullptr);

    return ok;
}

//
// ExpectPipe
//
// Expect the contents of a packet off of the subprocess' stdout.  If the
// response is unexpected, or doesn't arrive within a specific amuont of time,
// assume the subprocess is in an unknown state.
//
static bool ExpectPipe(net_packet_t *packet)
{
    int   start;
    BOOL  ok;
    CHAR  pipe_buffer[8192];
    DWORD pipe_buffer_read = 0;

    if (packet->len > sizeof(pipe_buffer))
    {
        // The size of the packet we're expecting is larger than our buffer
        // size, so bail out now.
        return false;
    }

    start = I_GetTimeMS();

    do
    {
        // Wait until we see exactly the amount of data we expect on the pipe.
        ok = PeekNamedPipe(midi_process_out_reader, nullptr, 0, nullptr,
            &pipe_buffer_read, nullptr);
        if (!ok)
        {
            break;
        }
        else if (pipe_buffer_read < packet->len)
        {
            I_Sleep(1);
            continue;
        }

        // Read precisely the number of bytes we're expecting, and no more.
        ok = ReadFile(midi_process_out_reader, pipe_buffer, static_cast<DWORD>(packet->len),
            &pipe_buffer_read, nullptr);
        if (!ok || pipe_buffer_read != packet->len)
        {
            break;
        }

        // Compare our data buffer to the packet.
        if (memcmp(packet->data, pipe_buffer, packet->len) != 0)
        {
            break;
        }

        return true;

        // Continue looping as long as we don't exceed our maximum wait time.
    } while (I_GetTimeMS() - start <= MIDIPIPE_MAX_WAIT);

    // TODO: Deal with the wedged process?
    return false;
}

//
// RemoveFileSpec
//
// A reimplementation of PathRemoveFileSpec that doesn't bring in Shlwapi
//
void RemoveFileSpec(TCHAR *path, size_t size)
{
    TCHAR *fp = nullptr;

    fp = &path[size];
    while (path <= fp && *fp != DIR_SEPARATOR)
    {
        fp--;
    }
    *(fp + 1) = '\0';
}

static bool BlockForAck()
{
    bool       ok;
    net_packet_t *packet;

    packet = NET_NewPacket(2);
    NET_WriteInt16(packet, MIDIPIPE_PACKET_TYPE_ACK);
    ok = ExpectPipe(packet);
    NET_FreePacket(packet);

    return ok;
}

//=============================================================================
//
// Protocol Commands
//

//
// I_MidiPipe_RegisterSong
//
// Tells the MIDI subprocess to load a specific filename for playing.  This
// function blocks until there is an acknowledgement from the server.
//
bool I_MidiPipe_RegisterSong(char *filename)
{
    bool       ok;
    net_packet_t *packet;

    packet = NET_NewPacket(64);
    NET_WriteInt16(packet, MIDIPIPE_PACKET_TYPE_REGISTER_SONG);
    NET_WriteString(packet, filename);
    ok = WritePipe(packet);
    NET_FreePacket(packet);

    midi_server_registered = false;

    ok = ok && BlockForAck();
    if (!ok)
    {
        DEBUGOUT("I_MidiPipe_RegisterSong failed");
        return false;
    }

    midi_server_registered = true;

    DEBUGOUT("I_MidiPipe_RegisterSong succeeded");
    return true;
}

//
// I_MidiPipe_UnregisterSong
//
// Tells the MIDI subprocess to unload the current song.
//
void I_MidiPipe_UnregisterSong()
{
    bool       ok;
    net_packet_t *packet;

    packet = NET_NewPacket(64);
    NET_WriteInt16(packet, MIDIPIPE_PACKET_TYPE_UNREGISTER_SONG);
    ok = WritePipe(packet);
    NET_FreePacket(packet);

    ok = ok && BlockForAck();
    if (!ok)
    {
        DEBUGOUT("I_MidiPipe_UnregisterSong failed");
        return;
    }

    midi_server_registered = false;

    DEBUGOUT("I_MidiPipe_UnregisterSong succeeded");
}

//
// I_MidiPipe_SetVolume
//
// Tells the MIDI subprocess to set a specific volume for the song.
//
void I_MidiPipe_SetVolume(int vol)
{
    bool       ok;
    net_packet_t *packet;

    packet = NET_NewPacket(6);
    NET_WriteInt16(packet, MIDIPIPE_PACKET_TYPE_SET_VOLUME);
    NET_WriteInt32(packet, vol);
    ok = WritePipe(packet);
    NET_FreePacket(packet);

    ok = ok && BlockForAck();
    if (!ok)
    {
        DEBUGOUT("I_MidiPipe_SetVolume failed");
        return;
    }

    DEBUGOUT("I_MidiPipe_SetVolume succeeded");
}

//
// I_MidiPipe_PlaySong
//
// Tells the MIDI subprocess to play the currently loaded song.
//
void I_MidiPipe_PlaySong(int loops)
{
    bool       ok;
    net_packet_t *packet;

    packet = NET_NewPacket(6);
    NET_WriteInt16(packet, MIDIPIPE_PACKET_TYPE_PLAY_SONG);
    NET_WriteInt32(packet, loops);
    ok = WritePipe(packet);
    NET_FreePacket(packet);

    ok = ok && BlockForAck();
    if (!ok)
    {
        DEBUGOUT("I_MidiPipe_PlaySong failed");
        return;
    }

    DEBUGOUT("I_MidiPipe_PlaySong succeeded");
}

//
// I_MidiPipe_StopSong
//
// Tells the MIDI subprocess to stop playing the currently loaded song.
//
void I_MidiPipe_StopSong()
{
    bool       ok;
    net_packet_t *packet;

    packet = NET_NewPacket(2);
    NET_WriteInt16(packet, MIDIPIPE_PACKET_TYPE_STOP_SONG);
    ok = WritePipe(packet);
    NET_FreePacket(packet);

    ok = ok && BlockForAck();
    if (!ok)
    {
        DEBUGOUT("I_MidiPipe_StopSong failed");
        return;
    }

    DEBUGOUT("I_MidiPipe_StopSong succeeded");
}

//
// I_MidiPipe_ShutdownServer
//
// Tells the MIDI subprocess to shutdown.
//
void I_MidiPipe_ShutdownServer()
{
    bool       ok;
    net_packet_t *packet;

    packet = NET_NewPacket(2);
    NET_WriteInt16(packet, MIDIPIPE_PACKET_TYPE_SHUTDOWN);
    ok = WritePipe(packet);
    NET_FreePacket(packet);

    ok = ok && BlockForAck();
    FreePipes();

    midi_server_initialized = false;

    if (!ok)
    {
        DEBUGOUT("I_MidiPipe_ShutdownServer failed");
        return;
    }

    DEBUGOUT("I_MidiPipe_ShutdownServer succeeded");
}

//=============================================================================
//
// Public Interface
//

//
// I_MidiPipeInitServer
//
// Start up the MIDI server.
//
bool I_MidiPipe_InitServer()
{
    TCHAR               dirname[MAX_PATH + 1];
    DWORD               dirname_len;
    char *              module  = nullptr;
    char *              cmdline = nullptr;
    char                params_buf[128];
    SECURITY_ATTRIBUTES sec_attrs;
    PROCESS_INFORMATION proc_info;
    STARTUPINFO         startup_info;
    BOOL                ok;

    if (!UsingNativeMidi() || strlen(g_i_sound_globals->snd_musiccmd) > 0)
    {
        // If we're not using native MIDI, or if we're playing music through
        // an exteranl program, we don't need to start the server.
        return false;
    }

    // Get directory name
    std::memset(dirname, 0, sizeof(dirname));
    dirname_len = GetModuleFileName(nullptr, dirname, MAX_PATH);
    if (dirname_len == 0)
    {
        return false;
    }
    RemoveFileSpec(dirname, dirname_len);

    // Define the module.
    module = const_cast<char *>(PROGRAM_PREFIX "midiproc.exe");

    // Set up pipes
    std::memset(&sec_attrs, 0, sizeof(SECURITY_ATTRIBUTES));
    sec_attrs.nLength              = sizeof(SECURITY_ATTRIBUTES);
    sec_attrs.bInheritHandle       = TRUE;
    sec_attrs.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&midi_process_in_reader, &midi_process_in_writer, &sec_attrs, 0))
    {
        DEBUGOUT("Could not initialize midiproc stdin");
        return false;
    }

    if (!SetHandleInformation(midi_process_in_writer, HANDLE_FLAG_INHERIT, 0))
    {
        DEBUGOUT("Could not disinherit midiproc stdin");
        return false;
    }

    if (!CreatePipe(&midi_process_out_reader, &midi_process_out_writer, &sec_attrs, 0))
    {
        DEBUGOUT("Could not initialize midiproc stdout/stderr");
        return false;
    }

    if (!SetHandleInformation(midi_process_out_reader, HANDLE_FLAG_INHERIT, 0))
    {
        DEBUGOUT("Could not disinherit midiproc stdin");
        return false;
    }

    // Define the command line.  Version, Sample Rate, and handles follow
    // the executable name.
    M_snprintf(params_buf, sizeof(params_buf), "%d %Iu %Iu",
        g_i_sound_globals->snd_samplerate, (size_t)midi_process_in_reader, (size_t)midi_process_out_writer);
    cmdline = M_StringJoin(module, " \"" PACKAGE_STRING "\"", " ", params_buf, nullptr);

    // Launch the subprocess
    std::memset(&proc_info, 0, sizeof(proc_info));
    std::memset(&startup_info, 0, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);

    ok = CreateProcess(TEXT(module), TEXT(cmdline), nullptr, nullptr, TRUE,
        0, nullptr, dirname, &startup_info, &proc_info);

    if (!ok)
    {
        FreePipes();
        free(cmdline);

        return false;
    }

    // Since the server has these handles, we don't need them anymore.
    CloseHandle(midi_process_in_reader);
    midi_process_in_reader = nullptr;
    CloseHandle(midi_process_out_writer);
    midi_process_out_writer = nullptr;

    midi_server_initialized = true;
    return true;
}

#endif
