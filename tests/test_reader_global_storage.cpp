#include "global_fatfs_mock.h"
static void failed_boot_save_is_authoritative() {
    files.clear();reset_fault();
    auto previous=default_global_preferences();previous.line_spacing=2;
    remember_global_book(previous,"older.txt");files[paths[0]]=record(previous,1);
    previous.line_spacing=3;remember_global_book(previous,"previous.txt");files[paths[1]]=record(previous,2);
    GlobalPreferences pending;
    {
        GlobalSettingsStore app;fault='r';ordinal=1;persistent=true;
        assert(app.load()==GlobalLoadResult::ERROR);reset_fault();
        assert(remember_global_book(app.values,"newly-opened.txt"));pending=app.values;
        assert(app.save()&&!app.dirty());
    }
    {GlobalSettingsStore cold;assert(cold.load()==GlobalLoadResult::LOADED);
     assert(same_global_preferences(cold.values,pending));}
    files.clear();reset_fault();writes=0;
}
static void recovery_boundaries(const char* scenario) {
    files.clear();reset_fault();writes=0;
    auto previous=default_global_preferences();previous.line_spacing=2;
    remember_global_book(previous,"older.txt");files[paths[0]]=record(previous,1);
    previous.line_spacing=3;remember_global_book(previous,"previous.txt");files[paths[1]]=record(previous,2);
    const std::string which=scenario;
    if(which=="missing0")files.erase(paths[0]);
    if((which=="exhausted"||which=="partial-exhausted"))files[paths[1]]=record(previous,0xffffffffu);
    if(which=="future-target"||which=="damaged-target"||which=="damaged-only") {
        if(which=="future-target")files[paths[0]][8]=99;
        else files[paths[0]].back()^=1;
        if(which=="damaged-only")files.erase(paths[1]);
    }
    if(which=="future-partner") {files.erase(paths[0]);files[paths[1]][8]=99;}
    const auto before=files;
    GlobalPreferences pending;
    const bool blocked=(which=="exhausted"||which=="partial-exhausted")||which=="future-target"||which=="damaged-only";
    {
        GlobalSettingsStore app;
        if(which!="no-load"&&which!="no-load-defaults") {
            fault=which=="boot-close"?'c':'r';ordinal=(which=="partial"||which=="partial-exhausted")?2:1;
            persistent=which!="partial";
            const auto loaded=app.load();
            assert(loaded==((which=="partial"||which=="partial-exhausted")?GlobalLoadResult::RECOVERED:GlobalLoadResult::ERROR));
            reset_fault();
        }
        if(which!="no-load-defaults"&&which!="boot-defaults") {
            app.values.line_spacing=4;app.values.paragraph_gap=ParagraphGap::NONE;app.values.shoulder_startup=true;
            const std::string name=std::string(245,'x')+u8"ééé.txt";
            assert(name.size()==255&&remember_global_book(app.values,name.c_str()));
        }
        pending=app.values;
        if(which=="retry-read"||which=="retry-close") {
            fault=which=="retry-read"?'r':'c';ordinal=2;persistent=true;
            for(int n=0;n<3;++n) {
                assert(!app.save()&&app.dirty()&&files==before&&writes==0);
                assert(same_global_preferences(app.values,pending));
                assert(handles.size()==(which=="retry-close"?1u:0u));
            }
            reset_fault();
        }
        assert(app.save()!=blocked);
        assert(same_global_preferences(app.values,pending));
        if(blocked)assert(app.dirty()&&files==before&&writes==0);
        else assert(!app.dirty());
    }
    assert(handles.empty());
    if(!blocked) {
        GlobalSettingsStore cold;
        assert(cold.load()==(which=="future-partner"?GlobalLoadResult::RECOVERED:GlobalLoadResult::LOADED));
        assert(same_global_preferences(cold.values,pending));
        if(which=="future-partner")assert(files[paths[1]]==before.at(paths[1]));
    }
    files.clear();reset_fault();writes=0;
}
static void recovery_fault_ordinals() {
    files.clear();reset_fault();
    auto p=default_global_preferences();p.line_spacing=2;
    files[paths[0]]=record(p,1);p.line_spacing=3;files[paths[1]]=record(p,2);
    const auto baseline=files;
    std::map<char,int> normal;
    {GlobalSettingsStore s;fault='r';ordinal=1;persistent=true;assert(s.load()==GlobalLoadResult::ERROR);
     reset_fault();s.values.line_spacing=4;remember_global_book(s.values,"pending.epub");assert(s.save());normal=counts;}
    for(auto pair:normal)for(int nth=1;nth<=pair.second;++nth) {
        files=baseline;reset_fault();
        {GlobalSettingsStore s;fault='r';ordinal=1;persistent=true;assert(s.load()==GlobalLoadResult::ERROR);
         reset_fault();s.values.line_spacing=4;remember_global_book(s.values,"pending.epub");const auto pending=s.values;
         fault=pair.first;ordinal=nth;assert(!s.save()&&s.dirty());
         assert(same_global_preferences(s.values,pending)&&handles.size()<=1);
         reset_fault();assert(s.save()&&!s.dirty());}
        {GlobalSettingsStore cold;assert(cold.load()==GlobalLoadResult::LOADED);
         assert(cold.values.line_spacing==4&&!strcmp(cold.values.last_book,"pending.epub"));}
        assert(handles.empty());
    }
    // Recovery then a failed readback may have committed. A revert to the
    // newly discovered disk snapshot must not leave that uncertain value winning.
    files=baseline;reset_fault();
    {GlobalSettingsStore s;fault='r';ordinal=1;persistent=true;assert(s.load()==GlobalLoadResult::ERROR);
     reset_fault();s.values.line_spacing=4;fault='o';ordinal=5;assert(!s.save());
     reset_fault();s.values=p;assert(s.dirty()&&s.save()&&!s.dirty());}
    {GlobalSettingsStore cold;assert(cold.load()==GlobalLoadResult::LOADED&&same_global_preferences(cold.values,p));}
    files.clear();reset_fault();writes=0;
}
static void normal_path_stays_bounded() {
    files.clear();reset_fault();auto p=default_global_preferences();
    files[paths[0]]=record(p,1);files[paths[1]]=record(p,2);
    {GlobalSettingsStore s;assert(s.load()==GlobalLoadResult::LOADED);reset_fault();
     assert(s.save()&&counts.empty());s.values.line_spacing=4;
     assert(s.save()&&counts['o']==3&&counts['r']==2&&counts['w']==1);}
    files.clear();reset_fault();writes=0;
}
int main(int argc,char** argv){
    if(argc==2) {recovery_boundaries(argv[1]);puts("PASS: recovery boundary");return 0;}
    failed_boot_save_is_authoritative();
    for(const char* scenario:{"missing0","partial","no-load","no-load-defaults","boot-defaults","exhausted","partial-exhausted",
            "future-target","future-partner","damaged-target","damaged-only","retry-read","retry-close","boot-close"})
        recovery_boundaries(scenario);
    recovery_fault_ordinals();
    normal_path_stays_bounded();
    {GlobalSettingsStore s;assert(s.load()==GlobalLoadResult::MISSING);assert(!s.dirty());assert(s.save()&&writes==0);
     s.values.line_spacing=4;s.values.shoulder_startup=true;assert(remember_global_book(s.values,u8"Thé book.epub"));
     assert(s.dirty()&&s.save()&&!s.dirty()&&s.generation()==1&&s.active_slot()==0&&writes==1);
     s.values.line_spacing=1;s.values.line_spacing=4;assert(s.save()&&writes==1);
     s.values.paragraph_gap=ParagraphGap::NONE;assert(s.save()&&writes==2&&s.active_slot()==1);
    }
    {GlobalSettingsStore s;assert(s.load()==GlobalLoadResult::LOADED);assert(s.generation()==2&&s.values.line_spacing==4&&s.values.shoulder_startup);assert(!strcmp(s.values.last_book,u8"Thé book.epub"));}
    files[paths[1]].back()^=1;
    {GlobalSettingsStore s;assert(s.load()==GlobalLoadResult::RECOVERED);assert(s.generation()==1&&s.values.paragraph_gap==ParagraphGap::FULL);s.values.line_spacing=0;assert(s.save()&&s.active_slot()==1);}
    auto p=default_global_preferences();files[paths[0]]=record(p,17);p.line_spacing=3;files[paths[1]]=record(p,17);
    {GlobalSettingsStore s;assert(s.load()==GlobalLoadResult::LOADED&&s.active_slot()==0&&s.values.line_spacing==1);}
    files[paths[0]]=record(p,0xffffffffu);
    {GlobalSettingsStore s;assert(s.load()==GlobalLoadResult::LOADED);auto before=files;s.values.line_spacing=0;assert(!s.save()&&s.dirty()&&files==before);}
    // Unknown version, unrelated, empty previous-process artifacts are immutable.
    for(int kind=0;kind<4;++kind) {
        files.clear();files[paths[0]]=kind==0?Bytes{}:kind==1?Bytes(40,'?'):record(p,4);
        if(kind==2)files[paths[0]][8]=99;
        if(kind==3)files[paths[0]].back()^=1;
        auto before=files;
        {GlobalSettingsStore s;assert(s.load()==GlobalLoadResult::ERROR);s.values.line_spacing=4;
         assert(!s.save()&&s.dirty()&&files==before);}
    }
    // Every real API ordinal on fresh and alternating records, plus short and
    // altered readback. Failed close retains the same pointer until recovered.
    for(bool fresh:{false,true}) {
        files.clear();if(!fresh){files[paths[0]]=record(p,1);files[paths[1]]=record(p,2);}
        auto baseline=files;std::map<char,int> normal;
        {GlobalSettingsStore s;s.load();s.values.line_spacing=4;remember_global_book(s.values,"next.txt");reset_fault();assert(s.save());normal=counts;}
        for(auto pair:normal)for(int nth=1;nth<=pair.second;++nth)for(int mode=0;mode<3;++mode) {
            if(mode&&pair.first!='r'&&pair.first!='w')continue;
            if(mode==2&&pair.first=='w')continue;
            files=baseline;reset_fault();
            {GlobalSettingsStore s;s.load();s.values.line_spacing=4;remember_global_book(s.values,"next.txt");
             auto gen=s.generation();fault=pair.first;ordinal=nth;short_io=mode==1;corrupt=mode==2;
             assert(!s.save());assert(s.dirty()&&s.generation()==gen);
             assert(handles.size()<=1);reset_fault();assert(s.save()&&!s.dirty());}
            assert(handles.empty());
            {GlobalSettingsStore s;assert(s.load()==GlobalLoadResult::LOADED);assert(s.values.line_spacing==4&&!strcmp(s.values.last_book,"next.txt"));}
        }
    }
    files.clear();
    {GlobalSettingsStore s;s.load();s.values.line_spacing=4;fault='w';ordinal=1;
     assert(!s.save());reset_fault();assert(files[paths[0]].empty());}
    {GlobalSettingsStore s;assert(s.load()==GlobalLoadResult::ERROR);s.values.line_spacing=4;assert(!s.save());}
    files.clear();files[paths[0]]=record(p,1);
    {GlobalSettingsStore s;s.load();s.values.line_spacing=4;reset_fault();fault='c';ordinal=1;persistent=true;
     assert(!s.save()&&handles.size()==1);assert(!s.save()&&handles.size()==1);
     reset_fault();assert(s.save()&&handles.empty());}
    // A failed verification can leave a newer valid record on disk. Reverting
    // RAM to the old snapshot must still retire that uncertain attempted value.
    files.clear();files[paths[0]]=record(p,1);reset_fault();
    {GlobalSettingsStore s;s.load();auto original=s.values;s.values.line_spacing=4;
     fault='o';ordinal=3;assert(!s.save());reset_fault();s.values=original;
     assert(s.dirty());assert(s.save()&&!s.dirty());}
    {GlobalSettingsStore s;s.load();assert(s.values.line_spacing==p.line_spacing);}
    assert(handles.empty());puts("PASS: global alternating checked persistence, restart, fallback, equal generation, exhaustion, dirty/revert");
}
