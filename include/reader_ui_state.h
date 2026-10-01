#pragma once

namespace reader {

// IMPORT lists folders from the card root; IMPORT_CONFIRM asks before copying.
enum class Scene { LIBRARY, READER, SETTINGS, ABOUT, IMPORT, IMPORT_CONFIRM };

constexpr int SAVE_MESSAGE_FRAMES = 90;

struct SaveMessageTimer {
    int frames_remaining;
};

void start_save_message(SaveMessageTimer& timer);
void cancel_save_message(SaveMessageTimer& timer);
bool save_message_visible(const SaveMessageTimer& timer);
bool tick_save_message(SaveMessageTimer& timer);

}
