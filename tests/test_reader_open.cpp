#include "reader_open.h"
#include "reader_arabic.h"
#include <string>
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace reader;
struct Save {
    int calls = 0;
    bool succeeds = true;
    TxtSaveFooter stored{};
    static bool write(void* context, const TxtSaveFooter& footer) {
        auto& self = *static_cast<Save*>(context);
        ++self.calls;
        if(!self.succeeds) return false;
        unsigned char bytes[TXT_SAVE_FOOTER_SIZE];
        make_txt_save_footer(footer, bytes);
        return parse_txt_save_footer(bytes, sizeof(bytes), self.stored);
    }
};
int main() {
    const char* text = u8"لالالالالالالالالالالالالالالالالالالالالا";
    MemorySource source(reinterpret_cast<const unsigned char*>(text), std::strlen(text));
    Settings settings = default_settings();
    Page page{}; PageHistory history{}; PageHistoryRebuild rebuild{}; Save save;
    assert(open_document_page(source, nullptr, settings, nullptr, history, page, rebuild,
                              false, Save::write, &save) == OpenResult::SAVED);
    assert(settings.arabic_shaping && page.line_count == 1);
    assert(save.calls == 1 && save.stored.settings.arabic_shaping);
    assert(save.stored.byte_offset == 0);
    puts("PASS: opening Arabic enables, relayouts and immediately persists ON");

    // A different file must not inherit ON; later Arabic deliberately stays OFF.
    std::string mixed;
    for(int i = 0; i < 20; ++i) mixed += "Latin page line\n";
    const uint32_t anchor = mixed.size();
    mixed += text;
    MemorySource book(reinterpret_cast<const unsigned char*>(mixed.data()), mixed.size());
    save.calls = 0;
    assert(open_document_page(book, nullptr, settings, nullptr, history, page, rebuild,
                              false, Save::write, &save) == OpenResult::OPENED);
    assert(!settings.arabic_shaping && save.calls == 0);
    arabic::scratch.count = 123; // OFF layout must never invoke shaping.
    Page next{};
    while(!page.eof) {
        assert(next_page(book, settings, nullptr, history, page, next)); page = next;
    }
    assert(!settings.arabic_shaping && arabic::scratch.count == 123);
    assert(save.calls == 0);

    // Reopening a saved Arabic byte anchor enables ON and discards old geometry.
    TxtSaveFooter old{anchor, settings, history, rebuild};
    old.display_layout = 2;
    old.history.count = 1; old.history.offsets[0] = 10;
    old.history_rebuild.state = HistoryRebuildState::BUILDING;
    old.history_rebuild.initialized = true;
    old.history_rebuild.scan.next_offset = 77;
    assert(open_document_page(book, &old, settings, nullptr, history, page, rebuild,
                              false, Save::write, &save) == OpenResult::SAVED);
    assert(page.start_offset == anchor && save.stored.byte_offset == anchor);
    assert(settings.arabic_shaping && history.count == 0 && history.lazy_anchor == anchor);
    assert(rebuild.anchor == anchor && !rebuild.initialized);
    while(rebuild.state == HistoryRebuildState::BUILDING)
        assert(step_history_rebuild(book, settings, nullptr, rebuild) != HistoryRebuildState::FAILED);
    assert(adopt_rebuilt_history(rebuild, history));
    assert(previous_page(book, settings, nullptr, history, next));
    assert(next.start_offset < anchor && next.next_offset >= anchor);

    // A sticky file stays ON when reopened at a wholly Latin opening page.
    auto sticky = save.stored;
    sticky.byte_offset = 0; sticky.history = {}; sticky.history_rebuild = {};
    save.calls = 0;
    assert(open_document_page(book, &sticky, settings, nullptr, history, page, rebuild,
                              false, Save::write, &save) == OpenResult::OPENED);
    assert(settings.arabic_shaping && save.calls == 0);
    adjust_setting(settings, SettingField::TOP_MARGIN, 1);
    assert(settings.arabic_shaping);

    // Failed immediate persistence is explicit; ON and the usable page stay in RAM.
    save.succeeds = false;
    assert(open_document_page(source, nullptr, settings, nullptr, history, page, rebuild,
                              false, Save::write, &save) == OpenResult::SAVE_FAILED);
    assert(settings.arabic_shaping && page.line_count == 1 && page.eof);
    save.succeeds = true;
    assert(Save::write(&save, {page.start_offset, settings, history, rebuild}));
    assert(save.stored.settings.arabic_shaping);

    // Cache creation and automatic ON use one persistence callback, not two.
    save.calls = 0;
    assert(open_document_page(source, nullptr, settings, nullptr, history, page, rebuild,
                              true, Save::write, &save) == OpenResult::SAVED);
    assert(save.calls == 1 && save.stored.settings.arabic_shaping);

    // Arabic wholly excluded by the final OFF wrap is not on the opening page.
    // Only accepted provisional PageLine bytes may enable the document.
    std::string boundary;
    for(int i = 0; i < 8; ++i) boundary += "x\n";
    boundary += std::string(27, 'a') + u8"لا";
    MemorySource edge(reinterpret_cast<const unsigned char*>(boundary.data()), boundary.size());
    settings = default_settings();
    auto wide_arabic = [](uint32_t cp) { return cp < 128 ? 8 : 16; };
    assert(layout_page(edge, 0, settings, wide_arabic, page));
    assert(page.line_count == 9 && !arabic::contains(page.lines[8].text));
    assert(open_document_page(edge, nullptr, settings, wide_arabic, history, page, rebuild,
                              false, Save::write, &save) == OpenResult::OPENED);
    assert(!settings.arabic_shaping && page.start_offset == 0);
    boundary.erase(boundary.size() - std::strlen(u8"لا") - 2, 2);
    MemorySource included(reinterpret_cast<const unsigned char*>(boundary.data()), boundary.size());
    assert(layout_page(included, 0, settings, wide_arabic, page));
    assert(arabic::contains(page.lines[8].text));
    assert(open_document_page(included, nullptr, settings, wide_arabic, history, page, rebuild,
                              false, Save::write, &save) == OpenResult::SAVED);
    assert(settings.arabic_shaping && page.start_offset == 0);

    puts("PASS: per-document reset, delayed discovery, sticky resume, mode migration, failure RAM, one cache save and wrap-edge probe");
}
