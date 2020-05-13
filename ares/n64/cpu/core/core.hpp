//{
  //core.cpp
  auto powerR4300() -> void;

  //memory.cpp
  auto readAddress (u32 address) -> maybe<u32>;
  auto readByte    (u32 address) -> maybe<u32>;
  auto readHalf    (u32 address) -> maybe<u32>;
  auto readWord    (u32 address) -> maybe<u32>;
  auto readDouble  (u32 address) -> maybe<u64>;

  auto writeAddress(u32 address) -> maybe<u32>;
  auto writeByte   (u32 address,  u8 data) -> void;
  auto writeHalf   (u32 address, u16 data) -> void;
  auto writeWord   (u32 address, u32 data) -> void;
  auto writeDouble (u32 address, u64 data) -> void;

  enum Interrupt : uint {
    Software0 = 0,
    Software1 = 1,
    RCP       = 2,
    Cartridge = 3,
    Reset     = 4,
    ReadRDB   = 5,
    WriteRDB  = 6,
    Timer     = 7,
  };

  //serialization.cpp
  auto serializeR4300(serializer&) -> void;

  struct Context {
    CPU& self;
    Context(CPU& self) : self(self) {}

    enum Mode : uint { Kernel, Supervisor, User, Undefined };
    enum Segment : uint { Invalid, Mapped, Cached, Uncached };
    uint mode;
    uint segment[8];  //512_MiB chunks

    //core.cpp
    auto setMode() -> void;
  } context{*this};

  #include "tlb.hpp"
  #include "instruction.hpp"
  #include "exception.hpp"
  #include "pipeline.hpp"
  #include "cpu-registers.hpp"
  #include "scc-registers.hpp"
  #include "fpu-registers.hpp"
  #include "disassembler.hpp"

  static constexpr bool Endian = 1;  //0 = little, 1 = big
  static constexpr uint FlipLE = (Endian == 0 ? 7 : 0);
  static constexpr uint FlipBE = (Endian == 1 ? 7 : 0);
//};
