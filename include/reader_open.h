#pragma once
#include "reader_txt_save.h"
#include "reader_arabic.h"

namespace reader {
enum class OpenResult { FAILED, OPENED, SAVED, SAVE_FAILED };
using OpeningSave = bool (*)(void*, const TxtSaveFooter&);

// Scan only accepted provisional opening-page bytes, never rejected wrap
// candidates or later pages. OFF navigation has no discovery work.
inline OpenResult open_document_page(const ByteSource& source, const TxtSaveFooter* saved,
        Settings& settings, GlyphWidth width, PageHistory& history, Page& page,
        PageHistoryRebuild& rebuild, bool prepare_cache, OpeningSave persist, void* context)
{
    if(saved) settings = saved->settings;
    settings.arabic_shaping = saved && saved->settings.arabic_shaping;
    const uint32_t offset = saved && saved->byte_offset < source.size() ? saved->byte_offset : 0;
    bool detected = false;
    if(!layout_page(source, offset, settings, width, page)) return OpenResult::FAILED;
    if(!settings.arabic_shaping) {
        for(int line = 0; line < page.line_count && !detected; ++line)
            detected = arabic::contains(page.lines[line].text);
    }
    if(detected) {
        settings.arabic_shaping = true;
        if(!layout_page(source, page.start_offset, settings, width, page)) return OpenResult::FAILED;
    }
    history = {};
    rebuild = {};
    if(saved && offset == saved->byte_offset && !detected) {
        restore_saved_history(*saved, history, rebuild);
    } else {
        history.lazy = page.start_offset > 0;
        history.lazy_anchor = page.start_offset;
        begin_history_rebuild(page.start_offset, rebuild);
    }
    if(!detected && !prepare_cache) return OpenResult::OPENED;
    const TxtSaveFooter footer{page.start_offset, settings, history, rebuild};
    return persist && persist(context, footer) ? OpenResult::SAVED : OpenResult::SAVE_FAILED;
}
}
