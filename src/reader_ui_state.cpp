#include "reader_ui_state.h"

namespace reader {

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
