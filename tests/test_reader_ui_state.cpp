#include "reader_ui_state.h"

#include <cassert>
#include <cstdio>
#include <initializer_list>

using namespace reader;

static void test_credits_input()
{
    Scene scene = Scene::LIBRARY;
    CreditsInputGate gate{};
    assert(handle_credits_input(scene, gate, true, false, true));
    assert(scene == Scene::CREDITS);
    assert(handle_credits_input(scene, gate, true, true, true));
    assert(scene == Scene::CREDITS); // Held entry chord cannot immediately close.
    assert(handle_credits_input(scene, gate, false, false, false));
    assert(handle_credits_input(scene, gate, false, true, true));
    assert(scene == Scene::LIBRARY);
    assert(handle_credits_input(scene, gate, true, false, true)); // Release tail.
    assert(scene == Scene::LIBRARY);
    assert(handle_credits_input(scene, gate, false, false, false));
    assert(!handle_credits_input(scene, gate, false, false, true)); // Normal A/navigation.
    assert(handle_credits_input(scene, gate, true, false, true));
    assert(handle_credits_input(scene, gate, false, false, false));
    assert(handle_credits_input(scene, gate, true, false, true));
    assert(scene == Scene::LIBRARY); // Start also closes.
    assert(handle_credits_input(scene, gate, false, false, false));
    for(Scene other : {Scene::READER, Scene::SETTINGS}) {
        scene = other;
        assert(!handle_credits_input(scene, gate, true, false, true));
        assert(scene == other); // Existing save/settings Start dispatcher runs.
    }
}

int main()
{
    test_credits_input();
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
