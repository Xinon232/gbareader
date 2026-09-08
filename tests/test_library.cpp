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
FRESULT f_opendir(DIR*, const TCHAR* path) { opened=path; cursor=0; return missing?FR_NO_PATH:FR_OK; }
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
 assert(scan_library()); assert(library_count()==64); assert(library_path(0,path));
 assert(std::strlen(path)==entries[0].size()+std::strlen("/gbareader/"));
 assert(std::string(path).substr(std::strlen("/gbareader/"))==entries[0]);
 assert(supported_book_name(path)); assert(txt_book_name(path));
 assert(!library_name(64));
 std::puts("PASS: scoped library scan, filtering, failures, cap and full-length paths");
}
