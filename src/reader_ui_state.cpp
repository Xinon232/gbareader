#include "reader_ui_state.h"

namespace reader {

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
