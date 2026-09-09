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
    assert(gate.page == 0);
    // Entry chord is fully released before any page turns.
    handle_credits_input(scene, gate, false, false, false);
    for(int i=1; i<CREDITS_PAGE_COUNT; ++i) {
        handle_credits_input(scene, gate, false, false, true, false, true);
        assert(gate.page == i && scene == Scene::CREDITS);
    }
    handle_credits_input(scene, gate, false, false, true, false, true);
    assert(gate.page == CREDITS_PAGE_COUNT - 1);
    for(int i=CREDITS_PAGE_COUNT-2; i>=0; --i) {
        handle_credits_input(scene, gate, false, false, true, true, false);
        assert(gate.page == i);
    }
    handle_credits_input(scene, gate, false, false, true, true, false);
    assert(gate.page == 0);
    gate.waiting_for_release = true;
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
    assert(LIBRARY_VISIBLE_ROWS == 5);
    assert(library_first_row(0, 0) == 0); // Empty or unavailable storage.
    for(int count = 1; count <= 64; ++count) {
        for(int selected = 0; selected < count; ++selected) {
            const int first = library_first_row(selected, count);
            assert(first >= 0 && first <= selected && selected < first + 5);
            assert(first == 0 || first + 5 <= count);
            if(count <= 5) assert(first == 0);
        }
        assert(library_first_row(0, count) == 0);
        assert(library_first_row(count - 1, count) == (count > 5 ? count - 5 : 0));
    }
    assert(library_first_row(2, 6) == 1); // Preserve selection-relative scrolling.
    assert(library_first_row(63, 64) == 59);
    Scene s = Scene::LIBRARY;
    CreditsInputGate gate{};
    int p = 0;
    assert(handle_controls_input(s, gate, p, true, false, false, false, true));
    assert(s == Scene::CONTROLS && p == 0);
    handle_controls_input(s, gate, p, false, false, false, true, true);
    assert(p == 0); // Entry release tail is swallowed.
    handle_controls_input(s, gate, p, false, false, false, false, false);
    for(int i = 1; i < CONTROLS_PAGE_COUNT; ++i) {
        handle_controls_input(s, gate, p, false, false, false, true, true);
        assert(p == i);
    }
    handle_controls_input(s, gate, p, false, false, false, true, true);
    assert(p == CONTROLS_PAGE_COUNT - 1);
    handle_controls_input(s, gate, p, false, false, true, false, true);
    assert(p == CONTROLS_PAGE_COUNT - 2);
    handle_controls_input(s, gate, p, false, true, false, false, true);
    assert(s == Scene::LIBRARY && gate.waiting_for_release);
    for(Scene other : {Scene::READER, Scene::SETTINGS, Scene::CREDITS}) {
        s = other;
        assert(!handle_controls_input(s, gate, p, true, false, false, false, true));
        assert(s == other);
    }
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
