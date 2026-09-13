#include "reader_file.h"
#include "epub_document.h"
#include <cassert>
#include <fstream>
#include <iterator>
int main(int argc,char** argv) {
    assert(argc==3);std::ifstream input(argv[1],std::ios::binary);
    std::vector<unsigned char> original{std::istreambuf_iterator<char>(input),{}};
    reader::MemorySource source(original.data(),original.size());reader::EpubDocument doc;
    assert(doc.open(source));auto output=original;
    reader::TxtSaveFooter state{};state.byte_offset=30;state.settings={1,2,3,true};
    assert(reader::append_epub_transaction_for_tests(output,&doc,state).success);
    std::ofstream file(argv[2],std::ios::binary);file.write(reinterpret_cast<const char*>(output.data()),output.size());
    assert(file.good());
}
