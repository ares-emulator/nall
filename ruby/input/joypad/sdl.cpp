#pragma once

struct InputJoypadSDL {
  Input& input;
  InputJoypadSDL(Input& input) : input(input) {}

  struct Joypad {
    shared_pointer<HID::Joypad> hid{new HID::Joypad};

    uint id = 0;
    SDL_Joystick* handle = nullptr;
  };
  vector<Joypad> joypads;

  auto assign(shared_pointer<HID::Joypad> hid, uint groupID, uint inputID, int16_t value) -> void {
    auto& group = hid->group(groupID);
    if(group.input(inputID).value() == value) return;
    input.doChange(hid, groupID, inputID, group.input(inputID).value(), value);
    group.input(inputID).setValue(value);
  }

  auto poll(vector<shared_pointer<HID::Device>>& devices) -> void {
    SDL_JoystickUpdate();
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
      if(event.type == SDL_JOYDEVICEADDED || event.type == SDL_JOYDEVICEREMOVED) {
        enumerate();
      }
    }

    #if defined(PLATFORM_BSD)
    //SDL 2.0 on FreeBSD never sends SDL_JOYDEVICE(ADDED|REMOVED) events, nor updates SDL_NumJoysticks()
    //the only way to support hotplugging here is to manually restart the SDL driver periodically to poll for new joypads
    uint64_t thisInitialize = chrono::millisecond();
    if(thisInitialize - lastInitialize > 5000) {
      lastInitialize = thisInitialize;
      initialize();
    }
    #endif

    for(auto& jp : joypads) {
      for(auto n : range(jp.hid->axes().size())) {
        assign(jp.hid, HID::Joypad::GroupID::Axis, n, (int16_t)SDL_JoystickGetAxis(jp.handle, n));
      }

      for(int n = 0; n < (int)jp.hid->hats().size() - 1; n += 2) {
        uint8_t state = SDL_JoystickGetHat(jp.handle, n >> 1);
        assign(jp.hid, HID::Joypad::GroupID::Hat, n + 0, state & SDL_HAT_LEFT ? -32767 : state & SDL_HAT_RIGHT ? +32767 : 0);
        assign(jp.hid, HID::Joypad::GroupID::Hat, n + 1, state & SDL_HAT_UP   ? -32767 : state & SDL_HAT_DOWN  ? +32767 : 0);
      }

      for(auto n : range(jp.hid->buttons().size())) {
        assign(jp.hid, HID::Joypad::GroupID::Button, n, (bool)SDL_JoystickGetButton(jp.handle, n));
      }

      devices.append(jp.hid);
    }
  }

  auto initialize() -> bool {
    terminate();
    SDL_Init(SDL_INIT_EVENTS);
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    SDL_JoystickEventState(SDL_ENABLE);
    enumerate();
    lastInitialize = chrono::millisecond();
    return true;
  }

  auto terminate() -> void {
    for(auto& jp : joypads) {
      SDL_JoystickClose(jp.handle);
    }
    joypads.reset();
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
  }

private:
  auto enumerate() -> void {
    for(auto& joypad : joypads) {
      SDL_JoystickClose(joypad.handle);
    }
    joypads.reset();

    for(uint id : range(SDL_NumJoysticks())) {
      Joypad jp;
      jp.id = id;
      jp.handle = SDL_JoystickOpen(jp.id);

      uint axes = SDL_JoystickNumAxes(jp.handle);
      uint hats = SDL_JoystickNumHats(jp.handle) * 2;
      uint buttons = SDL_JoystickNumButtons(jp.handle);

      jp.hid->setVendorID(HID::Joypad::GenericVendorID);
      jp.hid->setProductID(HID::Joypad::GenericProductID);
      jp.hid->setPathID(jp.id);
      for(uint n : range(axes)) jp.hid->axes().append(n);
      for(uint n : range(hats)) jp.hid->hats().append(n);
      for(uint n : range(buttons)) jp.hid->buttons().append(n);
      jp.hid->setRumble(false);

      joypads.append(jp);
    }

    SDL_JoystickUpdate();
    input.doHotplug();
  }

  uint64_t lastInitialize = 0;
};
