#include "reader_file.h"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <cstdio>
using namespace reader;
static std::vector<std::string> entries;
static unsigned cursor;
static bool missing, broken, close_error;
static std::string opened;
extern "C" {
static int opens;
FRESULT f_opendir(DIR*, const TCHAR* path) { opened=path; cursor=0; ++opens; return missing?FR_NO_PATH:FR_OK; }
FRESULT f_readdir(DIR*, FILINFO* out) {
 if(broken && cursor==1) return FR_DISK_ERR;
 *out={}; if(cursor==entries.size()) return FR_OK;
 const auto& n=entries[cursor++]; std::strcpy(out->fname,n.c_str());
 if(n=="folder.txt") out->fattrib=AM_DIR;
 return FR_OK;
}
FRESULT f_closedir(DIR*) { return close_error?FR_DISK_ERR:FR_OK; }
}
int main() {
 entries={"root.txt","Book.EPUB","notes.TxT","folder.txt","ignore.pdf"};
 assert(scan_library()); assert(opened=="/gbareader"); assert(library_count()==3);
 assert(std::string(library_name(1))=="Book.EPUB");
 char path[LIBRARY_PATH_MAX]; assert(library_path(1,path));
 assert(std::string(path)=="/gbareader/Book.EPUB");
 assert(!library_path(-1,path)); assert(!library_path(3,path));
 missing=true; assert(!scan_library()); assert(library_count()==0); missing=false;
 entries.clear(); assert(scan_library()); assert(library_count()==0);
 entries={"one.txt","two.epub"}; broken=true;
 assert(!scan_library()); assert(library_count()==0); broken=false;
 close_error=true; assert(!scan_library()); assert(library_count()==0); close_error=false;
 entries.assign(70,"book.txt"); entries[0]=std::string(251,'x')+".txt";
 assert(scan_library()); assert(library_count()==70); assert(library_path(0,path));
 assert(std::strlen(path)==entries[0].size()+std::strlen("/gbareader/"));
 assert(std::string(path).substr(std::strlen("/gbareader/"))==entries[0]);
 assert(supported_book_name(path)); assert(txt_book_name(path));
 assert(std::string(library_name(69))=="book.txt"); assert(!library_name(70));
 // No file limit: 1,000 books (plus folders and other files) are all listed in
 // folder order; names outside the 64-name window are reread only when needed.
 entries.clear();
 for(int i=0;i<1000;++i){entries.push_back("b"+std::to_string(i)+(i%2?".txt":".epub")); if(i%97==0){entries.push_back("skip.pdf");entries.push_back("folder.txt");}}
 assert(scan_library()); assert(library_count()==1000);
 opens=0;
 for(int i=0;i<1000;++i){const char* n=library_name(i);assert(n&&std::string(n)=="b"+std::to_string(i)+(i%2?".txt":".epub"));}
 assert(opens<=1000/32+1); // walking down rereads the folder about once per 32 books
 opens=0;for(int i=999;i>=990;--i)assert(library_name(i));for(int i=990;i<1000;++i)assert(library_name(i));assert(opens<=1);
 assert(library_path(999,path)&&std::string(path)=="/gbareader/b999.txt");
 // A failed reread is reported, not shown as another book.
 broken=true;cursor=0;assert(!library_name(0));broken=false;assert(std::string(library_name(0))=="b0.epub");
 std::puts("PASS: scoped library scan, filtering, failures, no file limit (paged names) and full-length paths");
}
