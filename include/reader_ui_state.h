#pragma once

namespace reader {

enum class Scene { LIBRARY, READER, SETTINGS, CREDITS };
struct CreditsInputGate { bool waiting_for_release; };
// Returns true when this frame must not reach the normal scene dispatcher.
bool handle_credits_input(Scene& scene, CreditsInputGate& gate,
                          bool start_pressed, bool b_pressed, bool any_held);

constexpr int SAVE_MESSAGE_FRAMES = 90;

struct SaveMessageTimer {
    int frames_remaining;
};

void start_save_message(SaveMessageTimer& timer);
void cancel_save_message(SaveMessageTimer& timer);
bool save_message_visible(const SaveMessageTimer& timer);
bool tick_save_message(SaveMessageTimer& timer);

}
