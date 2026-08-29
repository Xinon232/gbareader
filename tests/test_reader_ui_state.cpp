#include "reader_ui_state.h"

#include <cassert>
#include <cstdio>

using namespace reader;

int main()
{
    SaveMessageTimer timer{};
    assert(! save_message_visible(timer));

    start_save_message(timer);
    assert(save_message_visible(timer));
    for(int frame = 1; frame < SAVE_MESSAGE_FRAMES; ++frame) {
        assert(! tick_save_message(timer));
        assert(save_message_visible(timer));
    }
    assert(tick_save_message(timer));
    assert(! save_message_visible(timer));
    assert(! tick_save_message(timer));

    start_save_message(timer);
    cancel_save_message(timer);
    assert(! save_message_visible(timer));
    std::puts("PASS: transient save message timer");
}
