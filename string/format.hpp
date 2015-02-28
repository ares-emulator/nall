#ifdef NALL_STRING_INTERNAL_HPP

namespace nall {

//nall::format is a vector<string> of parameters that can be applied to a string
//each {#} token will be replaced with its appropriate format parameter

auto string::format(const nall::format& params) -> type& {
  signed size = this->size();
  char* data = (char*)memory::allocate(size);
  memory::copy(data, this->data(), size);

  signed x = 0;
  while(x < size - 2) {  //2 = minimum tag length
    if(data[x] != '{') { x++; continue; }

    signed y = x + 1;
    while(y < size - 1) {  //-1 avoids going out of bounds on test after this loop
      if(data[y] != '}') { y++; continue; }
      break;
    }

    if(data[y++] != '}') { x++; continue; }

    static auto isNumeric = [](char* s, char* e) -> bool {
      if(s == e) return false;  //ignore empty tags: {}
      while(s < e) {
        if(*s >= '0' && *s <= '9') { s++; continue; }
        return false;
      }
      return true;
    };
    if(!isNumeric(&data[x + 1], &data[y - 1])) { x++; continue; }

    unsigned index = nall::decimal(&data[x + 1]);
    if(index >= params.size()) { x++; continue; }

    unsigned sourceSize = y - x;
    unsigned targetSize = params[index].size();
    unsigned remaining = size - x;

    if(sourceSize > targetSize) {
      unsigned difference = sourceSize - targetSize;
      memory::move(&data[x], &data[x + difference], remaining);
      size -= difference;
    } else if(targetSize > sourceSize) {
      unsigned difference = targetSize - sourceSize;
      data = (char*)realloc(data, size + difference);
      size += difference;
      memory::move(&data[x + difference], &data[x], remaining);
    }
    memory::copy(&data[x], params[index].data(), targetSize);
    x += targetSize;
  }

  resize(size);
  memory::copy(pointer(), data, size);
  memory::free(data);
  return *this;
}

template<typename T, typename... P> auto append(format& self, const T& value, P&&... p) -> format& {
  self.vector<string>::append(value);
  append(self, std::forward<P>(p)...);
}

auto append(format& self) -> format& {
  return self;
}

template<typename... P> auto print(P&&... p) -> void {
  string s{std::forward<P>(p)...};
  fputs(s.data(), stdout);
}

template<signed precision, char padchar> auto integer(intmax_t value) -> string {
  string buffer;
  buffer.resize(1 + sizeof(intmax_t) * 3);
  char* p = buffer.pointer();

  bool negative = value < 0;
  value = abs(value);
  unsigned size = 0;
  do {
    p[size++] = '0' + (value % 10);
    value /= 10;
  } while(value);
  if(negative) p[size++] = '-';
  buffer.resize(size);
  buffer.reverse();
  if(precision) buffer.size(precision, padchar);
  return buffer;
}

template<signed precision, char padchar> auto decimal(uintmax_t value) -> string {
  string buffer;
  buffer.resize(sizeof(uintmax_t) * 3);
  char* p = buffer.pointer();

  unsigned size = 0;
  do {
    p[size++] = '0' + (value % 10);
    value /= 10;
  } while(value);
  buffer.resize(size);
  buffer.reverse();
  if(precision) buffer.size(precision, padchar);
  return buffer;
}

template<signed precision, char padchar> auto hex(uintmax_t value) -> string {
  string buffer;
  buffer.resize(sizeof(uintmax_t) * 2);
  char* p = buffer.pointer();

  unsigned size = 0;
  do {
    unsigned n = value & 15;
    p[size++] = n < 10 ? '0' + n : 'a' + n - 10;
    value >>= 4;
  } while(value);
  buffer.resize(size);
  buffer.reverse();
  if(precision) buffer.size(precision, padchar);
  return buffer;
}

template<signed precision, char padchar> auto octal(uintmax_t value) -> string {
  string buffer;
  buffer.resize(sizeof(uintmax_t) * 3);
  char* p = buffer.pointer();

  unsigned size = 0;
  do {
    p[size++] = '0' + (value & 7);
    value >>= 3;
  } while(value);
  buffer.resize(size);
  buffer.reverse();
  if(precision) buffer.size(precision, padchar);
  return buffer;
}

template<signed precision, char padchar> auto binary(uintmax_t value) -> string {
  string buffer;
  buffer.resize(sizeof(uintmax_t) * 8);
  char* p = buffer.pointer();

  unsigned size = 0;
  do {
    p[size++] = '0' + (value & 1);
    value >>= 1;
  } while(value);
  buffer.resize(size);
  buffer.reverse();
  if(precision) buffer.size(precision, padchar);
  return buffer;
}

template<signed precision, typename T> auto pointer(const T* value) -> string {
  if(value == nullptr) return "(null)";
  return {"0x", hex<precision>((uintptr_t)value)};
}

template<signed precision> auto pointer(uintptr_t value) -> string {
  if(value == 0) return "(null)";
  return {"0x", hex<precision>(value)};
}

auto real(long double value) -> string {
  string temp;
  temp.resize(real(nullptr, value));
  real(temp.pointer(), value);
  return temp;
}

}

#endif
