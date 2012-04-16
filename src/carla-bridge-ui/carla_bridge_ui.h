/*
 * Carla UI bridge code
 * Copyright (C) 2011-2012 Filipe Coelho <falktx@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the COPYING file
 */

#ifndef CARLA_BRIDGE_UI_H
#define CARLA_BRIDGE_UI_H

#include "carla_includes.h"

#include <pthread.h>

void toolkit_init();
void toolkit_loop(const char* ui_title, bool reparent);
void toolkit_quit();
void toolkit_window_show();
void toolkit_window_hide();
void toolkit_window_resize(int width, int height);

// -------------------------------------------------------------------------

const unsigned int MAX_BRIDGE_MESSAGES = 256;

enum BridgeMessageType {
    BRIDGE_MESSAGE_NULL         = 0,
    BRIDGE_MESSAGE_PARAMETER    = 1, // index, 0, value
    BRIDGE_MESSAGE_PROGRAM      = 2, // index, 0, 0
    BRIDGE_MESSAGE_MIDI_PROGRAM = 3, // bank, program, 0
    BRIDGE_MESSAGE_NOTE_ON      = 4, // note, velocity, 0
    BRIDGE_MESSAGE_NOTE_OFF     = 5, // note, 0, 0
    BRIDGE_MESSAGE_SHOW_GUI     = 6, // show, 0, 0
    BRIDGE_MESSAGE_RESIZE_GUI   = 7, // width, height, 0
    BRIDGE_MESSAGE_QUIT         = 8
};

struct QuequeBridgeMessage {
    bool valid;
    BridgeMessageType type;
    int value1;
    int value2;
    double value3;
};

// -------------------------------------------------------------------------

class UiData
{
public:
    UiData()
    {
        m_lib  = nullptr;
        pthread_mutex_init(&m_lock, nullptr);

        for (unsigned int i=0; i < MAX_BRIDGE_MESSAGES; i++)
        {
            QuequeBridgeMessages[i].valid  = false;
            QuequeBridgeMessages[i].type   = BRIDGE_MESSAGE_NULL;
            QuequeBridgeMessages[i].value1 = 0;
            QuequeBridgeMessages[i].value2 = 0;
            QuequeBridgeMessages[i].value3 = 0.0;
        }
    }

    ~UiData()
    {
        pthread_mutex_destroy(&m_lock);
    }

    void queque_message(BridgeMessageType type, int value1, int value2, double value3)
    {
        pthread_mutex_lock(&m_lock);
        for (unsigned int i=0; i<MAX_BRIDGE_MESSAGES; i++)
        {
            if (QuequeBridgeMessages[i].valid == false)
            {
                QuequeBridgeMessages[i].valid  = true;
                QuequeBridgeMessages[i].type   = type;
                QuequeBridgeMessages[i].value1 = value1;
                QuequeBridgeMessages[i].value2 = value2;
                QuequeBridgeMessages[i].value3 = value3;
                break;
            }
        }
        pthread_mutex_unlock(&m_lock);
    }

    bool run_messages()
    {
        pthread_mutex_lock(&m_lock);
        for (unsigned int i=0; i < MAX_BRIDGE_MESSAGES; i++)
        {
            if (QuequeBridgeMessages[i].valid)
            {
                QuequeBridgeMessage* m = &QuequeBridgeMessages[i];

                switch (m->type)
                {
                case BRIDGE_MESSAGE_NULL:
                    break;
                case BRIDGE_MESSAGE_PARAMETER:
                    update_parameter(m->value1, m->value3);
                    break;
                case BRIDGE_MESSAGE_PROGRAM:
                    update_program(m->value1);
                    break;
                case BRIDGE_MESSAGE_MIDI_PROGRAM:
                    update_midi_program(m->value1, m->value2);
                    break;
                case BRIDGE_MESSAGE_NOTE_ON:
                    send_note_on(m->value1, m->value2);
                    break;
                case BRIDGE_MESSAGE_NOTE_OFF:
                    send_note_off(m->value1);
                    break;
                case BRIDGE_MESSAGE_SHOW_GUI:
                    if (m->value1)
                        toolkit_window_show();
                    else
                        toolkit_window_hide();
                    break;
                case BRIDGE_MESSAGE_RESIZE_GUI:
                    toolkit_window_resize(m->value1, m->value2);
                    break;
                case BRIDGE_MESSAGE_QUIT:
                    toolkit_quit();
                    pthread_mutex_unlock(&m_lock);
                    return false;
                default:
                    break;
                }
                m->valid = false;
            }
            else
                break;
        }
        pthread_mutex_unlock(&m_lock);
        return true;
    }

    virtual bool init(const char*, const char*, const char*, bool) = 0;
    virtual void close() = 0;

    virtual void update_parameter(int index, double value) = 0;
    virtual void update_program(int index) = 0;
    virtual void update_midi_program(int bank, int program) = 0;
    virtual void send_note_on(int note, int velocity) = 0;
    virtual void send_note_off(int note) = 0;

    virtual void* get_widget() = 0;
    virtual bool is_resizable() = 0;

    bool lib_open(const char* filename)
    {
#ifdef Q_OS_WIN
        m_lib = LoadLibraryA(filename);
#else
        m_lib = dlopen(filename, RTLD_NOW);
#endif
        return bool(m_lib);
    }

    bool lib_close()
    {
        if (m_lib)
#ifdef Q_OS_WIN
            return FreeLibrary((HMODULE)m_lib) != 0;
#else
            return dlclose(m_lib) != 0;
#endif
        else
            return false;
    }

    void* lib_symbol(const char* symbol)
    {
        if (m_lib)
#ifdef Q_OS_WIN
            return (void*)GetProcAddress((HMODULE)m_lib, symbol);
#else
            return dlsym(m_lib, symbol);
#endif
        else
            return nullptr;
    }

    const char* lib_error()
    {
#ifdef Q_OS_WIN
        static char libError[2048];
        memset(libError, 0, sizeof(char)*2048);

        LPVOID winErrorString;
        DWORD  winErrorCode = GetLastError();
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |  FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, winErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&winErrorString, 0, nullptr);

        snprintf(libError, 2048, "%s: error code %i: %s", m_filename, winErrorCode, (const char*)winErrorString);
        LocalFree(winErrorString);

        return libError;
#else
        return dlerror();
#endif
    }

private:
    void* m_lib;
    pthread_mutex_t m_lock;
    QuequeBridgeMessage QuequeBridgeMessages[MAX_BRIDGE_MESSAGES];
};

// -------------------------------------------------------------------------

extern UiData* ui;

#endif // CARLA_BRIDGE_UI_H