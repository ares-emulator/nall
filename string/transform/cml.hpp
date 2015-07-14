#ifdef NALL_STRING_INTERNAL_HPP

/* CSS Markup Language (CML) v1.0 parser
 * revision 0.02
 */

namespace nall { namespace {

struct CML {
  auto& setPath(const string& pathname) { settings.path = pathname; return *this; }
  auto& setReader(const function<string (string)>& reader) { settings.reader = reader; return *this; }

  auto parse(const string& filename) -> string;
  auto parse(const string& filedata, const string& pathname) -> string;

private:
  struct Settings {
    string path;
    function<string (string)> reader;
  } settings;

  struct State {
    string output;
  } state;

  struct Variable {
    string name;
    string value;
  };
  vector<Variable> variables;

  auto parseDocument(const string& filedata, const string& pathname, unsigned depth) -> bool;
};

auto CML::parse(const string& filename) -> string {
  if(!settings.path) settings.path = filename.pathname();
  string document = settings.reader ? settings.reader(filename) : string::read(filename);
  parseDocument(document, settings.path, 0);
  return state.output;
}

auto CML::parse(const string& filedata, const string& pathname) -> string {
  settings.path = pathname;
  parseDocument(filedata, settings.path, 0);
  return state.output;
}

auto CML::parseDocument(const string& filedata, const string& pathname, unsigned depth) -> bool {
  if(depth >= 100) return false;  //prevent infinite recursion

  auto vendorAppend = [&](const string& name, const string& value) {
    state.output.append("  -moz-", name, ": ", value, ";\n");
    state.output.append("  -webkit-", name, ": ", value, ";\n");
  };

  for(auto& block : filedata.split("\n\n")) {
    lstring lines = block.rstrip().split("\n");
    string name = lines.takeFirst();

    if(name.beginsWith("include ")) {
      name.ltrim("include ", 1L);
      string filename{pathname, name};
      string document = settings.reader ? settings.reader(filename) : string::read(filename);
      parseDocument(document, filename.pathname(), depth + 1);
      continue;
    }

    if(name == "variables") {
      for(auto& line : lines) {
        auto data = line.split(":", 1L).strip();
        variables.append({data(0), data(1)});
      }
      continue;
    }

    state.output.append(name, " {\n");
    for(auto& line : lines) {
      auto data = line.split(":", 1L).strip();
      auto name = data(0), value = data(1);
      while(auto offset = value.find("var(")) {
        bool found = false;
        if(auto length = value.findFrom(*offset, ")")) {
          string name = value.slice(*offset + 4, *length - 4);
          for(auto& variable : variables) {
            if(variable.name == name) {
              value = {value.slice(0, *offset), variable.value, value.slice(*offset + *length + 1)};
              found = true;
              break;
            }
          }
        }
        if(!found) break;
      }
      state.output.append("  ", name, ": ", value, ";\n");
      if(name == "box-sizing") vendorAppend(name, value);
    }
    state.output.append("}\n\n");
  }

  return true;
}

}}

#endif
