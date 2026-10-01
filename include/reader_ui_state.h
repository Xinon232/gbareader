#pragma once

// make DEMO=1 builds demo_gbareader.gba: a start screen, no position saving
// and no Go to (each shows a notice pointing to the full version).
#ifndef GBAREADER_DEMO
#define GBAREADER_DEMO 0
#endif

namespace reader {

// IMPORT lists folders from the card root; IMPORT_CONFIRM asks before copying.
// DEMO_INTRO / DEMO_NOTICE exist only in the demo build's flow.
enum class Scene { LIBRARY, READER, SETTINGS, ABOUT, IMPORT, IMPORT_CONFIRM, DEMO_INTRO, DEMO_NOTICE };

constexpr int SAVE_MESSAGE_FRAMES = 90;

struct SaveMessageTimer {
    int frames_remaining;
};

void start_save_message(SaveMessageTimer& timer);
void cancel_save_message(SaveMessageTimer& timer);
bool save_message_visible(const SaveMessageTimer& timer);
bool tick_save_message(SaveMessageTimer& timer);

}
