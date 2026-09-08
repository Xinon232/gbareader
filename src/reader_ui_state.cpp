#include "reader_ui_state.h"

namespace reader {
int library_first_row(int selected, int count) {
    int first = selected > 1 ? selected - 1 : 0;
    if(first + LIBRARY_VISIBLE_ROWS > count)
        first = count > LIBRARY_VISIBLE_ROWS ? count - LIBRARY_VISIBLE_ROWS : 0;
    return first;
}
bool handle_controls_input(Scene& scene, CreditsInputGate& gate, int& page,
                           bool select, bool back, bool left, bool right, bool held)
{
    if(scene != Scene::LIBRARY && scene != Scene::CONTROLS) return false;
    if(gate.waiting_for_release) {
        if(!held) gate.waiting_for_release = false;
        return true;
    }
    if(scene == Scene::LIBRARY && select) {
        scene = Scene::CONTROLS; page = 0; gate.waiting_for_release = true;
        return true;
    }
    if(scene != Scene::CONTROLS) return false;
    if(back) { scene = Scene::LIBRARY; gate.waiting_for_release = true; }
    else if(left && page > 0) --page;
    else if(right && page + 1 < CONTROLS_PAGE_COUNT) ++page;
    return true;
}

bool handle_credits_input(Scene& scene, CreditsInputGate& gate,
                          bool start_pressed, bool b_pressed, bool any_held)
{
    if(scene != Scene::LIBRARY && scene != Scene::CREDITS) return false;
    if(gate.waiting_for_release) {
        if(!any_held) gate.waiting_for_release = false;
        return true;
    }
    if(scene == Scene::LIBRARY && start_pressed) {
        scene = Scene::CREDITS;
        gate.waiting_for_release = true;
        return true;
    }
    if(scene == Scene::CREDITS) {
        if(start_pressed || b_pressed) {
            scene = Scene::LIBRARY;
            gate.waiting_for_release = true;
        }
        return true;
    }
    return false;
}

void start_save_message(SaveMessageTimer& timer)
{
    timer.frames_remaining = SAVE_MESSAGE_FRAMES;
}

void cancel_save_message(SaveMessageTimer& timer)
{
    timer.frames_remaining = 0;
}

bool save_message_visible(const SaveMessageTimer& timer)
{
    return timer.frames_remaining > 0;
}

bool tick_save_message(SaveMessageTimer& timer)
{
    if(timer.frames_remaining <= 0) return false;
    --timer.frames_remaining;
    return timer.frames_remaining == 0;
}

}
