#pragma once

namespace reader {

enum class Scene { LIBRARY, READER, SETTINGS, CREDITS, CONTROLS };
constexpr int CONTROLS_PAGE_COUNT = 7;
constexpr int CREDITS_PAGE_COUNT = 7;
constexpr int LIBRARY_VISIBLE_ROWS = 5;
int library_first_row(int selected, int count);
struct CreditsInputGate { bool waiting_for_release; int page; };
bool handle_controls_input(Scene& scene, CreditsInputGate& gate, int& page,
                           bool select, bool back, bool left, bool right, bool held);
// Returns true when this frame must not reach the normal scene dispatcher.
bool handle_credits_input(Scene& scene, CreditsInputGate& gate,
                          bool start_pressed, bool b_pressed, bool any_held,
                          bool left_pressed = false, bool right_pressed = false);

constexpr int SAVE_MESSAGE_FRAMES = 90;

struct SaveMessageTimer {
    int frames_remaining;
};

void start_save_message(SaveMessageTimer& timer);
void cancel_save_message(SaveMessageTimer& timer);
bool save_message_visible(const SaveMessageTimer& timer);
bool tick_save_message(SaveMessageTimer& timer);

}
