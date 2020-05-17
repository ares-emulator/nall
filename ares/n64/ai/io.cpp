static const vector<string> registerNames = {
  "AI_DRAM_ADDRESS",
  "AI_LENGTH",
  "AI_CONTROL",
  "AI_STATUS",
  "AI_DACRATE",
  "AI_BITRATE",
};

auto AI::readIO(u32 address) -> u32 {
  address = (address & 0xfffff) >> 2;
  uint32 data;

  if(address != 3) {
    //AI_LENGTH (mirrored)
    data.bit(0,17) = io.dmaLength[0];
  }

  if(address == 3) {
    //AI_STATUS
    data.bit( 0) = io.dmaCount > 1;
    data.bit(20) = 1;
    data.bit(24) = 1;
    data.bit(30) = io.dmaCount > 0;
    data.bit(31) = io.dmaCount > 1;
  }

//print("* ", registerNames(address, "AI_UNKNOWN"), " => ", hex(data, 8L), "\n");
  return data;
}

auto AI::writeIO(u32 address, uint32 data) -> void {
  address = (address & 0xfffff) >> 2;

  if(address == 0) {
    //AI_DRAM_ADDRESS
    if(io.dmaCount < 2) {
      io.dmaAddress[io.dmaCount] = data.bit(0,23) & ~7;
    }
  }

  if(address == 1) {
    //AI_LENGTH
    uint18 length = data.bit(0,17) & ~7;
    if(io.dmaCount < 2 && length) {
      io.dmaLength[io.dmaCount] = length;
      io.dmaCount++;
    }
  }

  if(address == 2) {
    //AI_CONTROL
    io.dmaEnable = data.bit(0);
  }

  if(address == 3) {
    //AI_STATUS
    mi.lower(MI::IRQ::AI);
  }

  if(address == 4) {
    //AI_DACRATE
    auto frequency = dac.frequency;
    io.dacRate = data.bit(0,13);
    dac.frequency = max(1, 93'750'000 / 2 / (io.dacRate + 1)) * 1.037;
    dac.period = 93'750'000 / dac.frequency;
    if(frequency != dac.frequency) stream->setFrequency(dac.frequency);
  }

  if(address == 5) {
    //AI_BITRATE
    io.bitRate = data.bit(0,3);
    dac.precision = io.bitRate + 1;
  }

//print("* ", registerNames(address, "AI_UNKNOWN"), " <= ", hex(data, 8L), "\n");
}
