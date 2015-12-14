#ifdef NALL_MOSAIC_INTERNAL_HPP

namespace nall {
namespace mosaic {

struct bitstream {
  ~bitstream() {
    close();
  }

  auto read(uint64 addr) const -> bool {
    if(data == nullptr || (addr >> 3) >= size) return 0;
    uint mask = endian == 0 ? (0x01 << (addr & 7)) : (0x80 >> (addr & 7));
    return data[addr >> 3] & mask;
  }

  auto write(uint64 addr, bool value) -> void {
    if(data == nullptr || readonly == true || (addr >> 3) >= size) return;
    uint mask = endian == 0 ? (0x01 << (addr & 7)) : (0x80 >> (addr & 7));
    if(value == 0) data[addr >> 3] &= ~mask;
    if(value == 1) data[addr >> 3] |=  mask;
  }

  auto open(const string& filename) -> bool {
    readonly = false;
    if(fp.open(filename, filemap::mode::readwrite) == false) {
      readonly = true;
      if(fp.open(filename, filemap::mode::read) == false) {
        return false;
      }
    }
    data = fp.data();
    size = fp.size();
    return true;
  }

  auto close() -> void {
    fp.close();
    data = nullptr;
  }

  filemap fp;
  uint8* data = nullptr;
  uint size = 0;
  bool readonly = false;
  bool endian = 1;
};

}
}

#endif
