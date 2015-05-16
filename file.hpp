#ifndef NALL_FILE_HPP
#define NALL_FILE_HPP

#include <nall/platform.hpp>
#include <nall/stdint.hpp>
#include <nall/storage.hpp>
#include <nall/string.hpp>
#include <nall/utility.hpp>
#include <nall/varint.hpp>
#include <nall/hash/sha256.hpp>
#include <nall/stream/memory.hpp>

namespace nall {

struct file : storage, varint {
  enum class mode : unsigned { read, write, modify, append, readwrite = modify, writeread = append };
  enum class index : unsigned { absolute, relative };

  static auto copy(const string& sourcename, const string& targetname) -> bool {
    file rd, wr;
    if(rd.open(sourcename, mode::read) == false) return false;
    if(wr.open(targetname, mode::write) == false) return false;
    for(unsigned n = 0; n < rd.size(); n++) wr.write(rd.read());
    return true;
  }

  //attempt to rename file first
  //this will fail if paths point to different file systems; fall back to copy+remove in this case
  static auto move(const string& sourcename, const string& targetname) -> bool {
    if(rename(sourcename, targetname)) return true;
    if(!writable(sourcename)) return false;
    if(copy(sourcename, targetname)) {
      remove(sourcename);
      return true;
    }
    return false;
  }

  static auto truncate(const string& filename, unsigned size) -> bool {
    #if !defined(_WIN32)
    return truncate(filename, size) == 0;
    #else
    bool result = false;
    FILE* fp = _wfopen(utf16_t(filename), L"rb+");
    if(fp) {
      result = _chsize(fileno(fp), size) == 0;
      fclose(fp);
    }
    return result;
    #endif
  }

  //specialization of storage::exists(); returns false for folders
  static auto exists(const string& filename) -> bool {
    #if !defined(_WIN32)
    struct stat data;
    if(stat(filename, &data) != 0) return false;
    #else
    struct __stat64 data;
    if(_wstat64(utf16_t(filename), &data) != 0) return false;
    #endif
    return !(data.st_mode & S_IFDIR);
  }

  static auto size(const string& filename) -> uintmax_t {
    #if !defined(_WIN32)
    struct stat data;
    stat(filename, &data);
    #else
    struct __stat64 data;
    _wstat64(utf16_t(filename), &data);
    #endif
    return S_ISREG(data.st_mode) ? data.st_size : 0u;
  }

  static auto read(const string& filename) -> vector<uint8_t> {
    vector<uint8_t> memory;
    file fp;
    if(fp.open(filename, mode::read)) {
      memory.resize(fp.size());
      fp.read(memory.data(), memory.size());
    }
    return memory;
  }

  static auto read(const string& filename, uint8_t* data, unsigned size) -> bool {
    file fp;
    if(fp.open(filename, mode::read) == false) return false;
    fp.read(data, size);
    fp.close();
    return true;
  }

  static auto write(const string& filename, const string& text) -> bool {
    file fp;
    if(fp.open(filename, mode::write) == false) return false;
    fp.print(text);
    fp.close();
    return true;
  }

  static auto write(const string& filename, const vector<uint8_t>& buffer) -> bool {
    file fp;
    if(fp.open(filename, mode::write) == false) return false;
    fp.write(buffer.data(), buffer.size());
    fp.close();
    return true;
  }

  static auto write(const string& filename, const uint8_t* data, unsigned size) -> bool {
    file fp;
    if(fp.open(filename, mode::write) == false) return false;
    fp.write(data, size);
    fp.close();
    return true;
  }

  static auto create(const string& filename) -> bool {
    //create an empty file (will replace existing files)
    file fp;
    if(fp.open(filename, mode::write) == false) return false;
    fp.close();
    return true;
  }

  static auto sha256(const string& filename) -> string {
    auto buffer = read(filename);
    return Hash::SHA256(buffer.data(), buffer.size()).digest();
  }

  auto read() -> uint8_t {
    if(!fp) return 0xff;                       //file not open
    if(file_mode == mode::write) return 0xff;  //reads not permitted
    if(file_offset >= file_size) return 0xff;  //cannot read past end of file
    buffer_sync();
    return buffer[(file_offset++) & buffer_mask];
  }

  auto readl(unsigned length = 1) -> uintmax_t {
    uintmax_t data = 0;
    for(int i = 0; i < length; i++) {
      data |= (uintmax_t)read() << (i << 3);
    }
    return data;
  }

  auto readm(unsigned length = 1) -> uintmax_t {
    uintmax_t data = 0;
    while(length--) {
      data <<= 8;
      data |= read();
    }
    return data;
  }

  auto read(uint8_t* buffer, unsigned length) -> void {
    while(length--) *buffer++ = read();
  }

  auto write(uint8_t data) -> void {
    if(!fp) return;                      //file not open
    if(file_mode == mode::read) return;  //writes not permitted
    buffer_sync();
    buffer[(file_offset++) & buffer_mask] = data;
    buffer_dirty = true;
    if(file_offset > file_size) file_size = file_offset;
  }

  auto writel(uintmax_t data, unsigned length = 1) -> void {
    while(length--) {
      write(data);
      data >>= 8;
    }
  }

  auto writem(uintmax_t data, unsigned length = 1) -> void {
    for(int i = length - 1; i >= 0; i--) {
      write(data >> (i << 3));
    }
  }

  auto write(const uint8_t* buffer, unsigned length) -> void {
    while(length--) write(*buffer++);
  }

  template<typename... Args> auto print(Args... args) -> void {
    string data(args...);
    const char* p = data;
    while(*p) write(*p++);
  }

  auto flush() -> void {
    buffer_flush();
    fflush(fp);
  }

  auto seek(signed offset, index index_ = index::absolute) -> void {
    if(!fp) return;  //file not open
    buffer_flush();

    intmax_t req_offset = file_offset;
    switch(index_) {
    case index::absolute: req_offset  = offset; break;
    case index::relative: req_offset += offset; break;
    }

    if(req_offset < 0) req_offset = 0;  //cannot seek before start of file
    if(req_offset > file_size) {
      if(file_mode == mode::read) {     //cannot seek past end of file
        req_offset = file_size;
      } else {                          //pad file to requested location
        file_offset = file_size;
        while(file_size < req_offset) write(0x00);
      }
    }

    file_offset = req_offset;
  }

  auto offset() const -> unsigned {
    if(!fp) return 0;  //file not open
    return file_offset;
  }

  auto size() const -> unsigned {
    if(!fp) return 0;  //file not open
    return file_size;
  }

  auto truncate(unsigned size) -> bool {
    if(!fp) return false;  //file not open
    #if !defined(_WIN32)
    return ftruncate(fileno(fp), size) == 0;
    #else
    return _chsize(fileno(fp), size) == 0;
    #endif
  }

  auto end() -> bool {
    if(!fp) return true;  //file not open
    return file_offset >= file_size;
  }

  auto open() const -> bool {
    return fp;
  }

  explicit operator bool() const {
    return open();
  }

  auto open(const string& filename, mode mode_) -> bool {
    if(fp) return false;

    switch(file_mode = mode_) {
    #if !defined(_WIN32)
    case mode::read:      fp = fopen(filename, "rb" ); break;
    case mode::write:     fp = fopen(filename, "wb+"); break;  //need read permission for buffering
    case mode::readwrite: fp = fopen(filename, "rb+"); break;
    case mode::writeread: fp = fopen(filename, "wb+"); break;
    #else
    case mode::read:      fp = _wfopen(utf16_t(filename), L"rb" ); break;
    case mode::write:     fp = _wfopen(utf16_t(filename), L"wb+"); break;
    case mode::readwrite: fp = _wfopen(utf16_t(filename), L"rb+"); break;
    case mode::writeread: fp = _wfopen(utf16_t(filename), L"wb+"); break;
    #endif
    }
    if(!fp) return false;
    buffer_offset = -1;  //invalidate buffer
    file_offset = 0;
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    return true;
  }

  auto close() -> void {
    if(!fp) return;
    buffer_flush();
    fclose(fp);
    fp = nullptr;
  }

  auto operator=(const file&) -> file& = delete;
  file(const file&) = delete;
  file() = default;

  file(const string& filename, mode mode_) {
    open(filename, mode_);
  }

  ~file() {
    close();
  }

private:
  enum { buffer_size = 1 << 12, buffer_mask = buffer_size - 1 };
  char buffer[buffer_size] = {0};
  int buffer_offset = -1;  //invalidate buffer
  bool buffer_dirty = false;
  FILE* fp = nullptr;
  unsigned file_offset = 0;
  unsigned file_size = 0;
  mode file_mode = mode::read;

  auto buffer_sync() -> void {
    if(!fp) return;  //file not open
    if(buffer_offset != (file_offset & ~buffer_mask)) {
      buffer_flush();
      buffer_offset = file_offset & ~buffer_mask;
      fseek(fp, buffer_offset, SEEK_SET);
      unsigned length = (buffer_offset + buffer_size) <= file_size ? buffer_size : (file_size & buffer_mask);
      if(length) unsigned unused = fread(buffer, 1, length, fp);
    }
  }

  auto buffer_flush() -> void {
    if(!fp) return;                      //file not open
    if(file_mode == mode::read) return;  //buffer cannot be written to
    if(buffer_offset < 0) return;        //buffer unused
    if(buffer_dirty == false) return;    //buffer unmodified since read
    fseek(fp, buffer_offset, SEEK_SET);
    unsigned length = (buffer_offset + buffer_size) <= file_size ? buffer_size : (file_size & buffer_mask);
    if(length) unsigned unused = fwrite(buffer, 1, length, fp);
    buffer_offset = -1;                  //invalidate buffer
    buffer_dirty = false;
  }
};

}

#endif
