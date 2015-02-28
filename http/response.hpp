#ifndef NALL_HTTP_RESPONSE_HPP
#define NALL_HTTP_RESPONSE_HPP

#include <nall/http/message.hpp>

namespace nall {

struct httpResponse : httpMessage {
  using type = httpResponse;

  httpResponse() = default;
  httpResponse(const httpRequest& request) { setRequest(request); }

  explicit operator bool() const { return responseType() != 0; }
  auto operator()(unsigned responseType) -> type& { return setResponseType(responseType); }

  inline auto head(const function<bool (const uint8_t* data, unsigned size)>& callback) const -> bool;
  inline auto setHead() -> bool;

  inline auto body(const function<bool (const uint8_t* data, unsigned size)>& callback) const -> bool;
  inline auto setBody() -> bool;

  auto request() const -> const httpRequest* { return _request; }
  auto setRequest(const httpRequest& value) -> type& { _request = &value; return *this; }

  auto responseType() const -> unsigned { return _responseType; }
  auto setResponseType(unsigned value) -> type& { _responseType = value; return *this; }

  auto appendHeader(const string& name, const string& value = "") -> type& { return httpMessage::appendHeader(name, value), *this; }
  auto removeHeader(const string& name) -> type& { return httpMessage::removeHeader(name), *this; }
  auto setHeader(const string& name, const string& value = "") -> type& { return httpMessage::setHeader(name, value), *this; }

  auto hasData() const -> bool { return (bool)_data; }
  auto data() const -> const vector<uint8_t>& { return _data; }
  inline auto setData(const vector<uint8_t>& value) -> type&;

  auto hasFile() const -> bool { return (bool)_file; }
  auto file() const -> const string& { return _file; }
  inline auto setFile(const string& value) -> type&;

  auto hasText() const -> bool { return (bool)_text; }
  auto text() const -> const string& { return _text; }
  inline auto setText(const string& value) -> type&;

  inline auto hasBody() const -> bool;
  inline auto findContentLength() const -> unsigned;
  inline auto findContentType() const -> string;
  inline auto findContentType(const string& suffix) const -> string;
  inline auto findResponseType() const -> string;
  inline auto setFileETag() -> void;

  const httpRequest* _request = nullptr;
  unsigned _responseType = 0;
  vector<uint8_t> _data;
  string _file;
  string _text;
};

auto httpResponse::head(const function<bool (const uint8_t*, unsigned)>& callback) const -> bool {
  if(!callback) return false;
  string output;

  if(auto request = this->request()) {
    if(auto eTag = header("ETag")) {
      if(eTag == request->header("If-None-Match")) {
        output.append("HTTP/1.1 304 Not Modified\r\n");
        output.append("Connection: close\r\n");
        output.append("\r\n");
        return callback(output.binary(), output.size());
      }
    }
  }

  output.append("HTTP/1.1 ", findResponseType(), "\r\n");
  for(auto& header : _header) {
    output.append(header.name, ": ", header.value, "\r\n");
  }
  if(hasBody()) {
    if(!header("Content-Length") && !header("Transfer-Encoding").iequals("chunked")) {
      output.append("Content-Length: ", findContentLength(), "\r\n");
    }
    if(!header("Content-Type")) {
      output.append("Content-Type: ", findContentType(), "\r\n");
    }
  }
  if(!header("Connection")) {
    output.append("Connection: close\r\n");
  }
  output.append("\r\n");

  return callback(output.binary(), output.size());
}

auto httpResponse::setHead() -> bool {
  lstring headers = _head.split("\n");
  string response = headers.takeFirst().rtrim("\r");

       if(iltrim(response, "HTTP/1.0 "));
  else if(iltrim(response, "HTTP/1.1 "));
  else return false;

  setResponseType(decimal(response));

  for(auto& header : headers) {
    if(header.beginsWith(" ") || header.beginsWith("\t")) continue;
    lstring variable = header.split<1>(":").strip();
    if(variable.size() != 2) continue;
    appendHeader(variable[0], variable[1]);
  }

  return true;
}

auto httpResponse::body(const function<bool (const uint8_t*, unsigned)>& callback) const -> bool {
  if(!callback) return false;
  if(!hasBody()) return true;
  bool chunked = header("Transfer-Encoding") == "chunked";

  if(chunked) {
    string prefix = {hex(findContentLength()), "\r\n"};
    if(!callback(prefix.binary(), prefix.size())) return false;
  }

  if(_body) {
    if(!callback(_body.binary(), _body.size())) return false;
  } else if(hasData()) {
    if(!callback(data().data(), data().size())) return false;
  } else if(hasFile()) {
    filemap map(file(), filemap::mode::read);
    if(!callback(map.data(), map.size())) return false;
  } else if(hasText()) {
    if(!callback(text().binary(), text().size())) return false;
  } else {
    string response = findResponseType();
    if(!callback(response.binary(), response.size())) return false;
  }

  if(chunked) {
    string suffix = {"\r\n0\r\n\r\n"};
    if(!callback(suffix.binary(), suffix.size())) return false;
  }

  return true;
}

auto httpResponse::setBody() -> bool {
  return true;
}

auto httpResponse::hasBody() const -> bool {
  if(auto request = this->request()) {
    if(request->requestType() == httpRequest::RequestType::Head) return false;
  }
  if(responseType() == 301) return false;
  if(responseType() == 302) return false;
  if(responseType() == 303) return false;
  if(responseType() == 304) return false;
  if(responseType() == 307) return false;
  return true;
}

auto httpResponse::findContentLength() const -> unsigned {
  if(auto contentLength = header("Content-Length")) return decimal(contentLength);
  if(_body) return _body.size();
  if(hasData()) return data().size();
  if(hasFile()) return file::size(file());
  if(hasText()) return text().size();
  return findResponseType().size();
}

auto httpResponse::findContentType() const -> string {
  if(auto contentType = header("Content-Type")) return contentType;
  if(hasData()) return "application/octet-stream";
  if(hasFile()) return findContentType(file().suffixname());
  return "text/html; charset=utf-8";
}

auto httpResponse::findContentType(const string& s) const -> string {
  if(s == ".7z"  ) return "application/x-7z-compressed";
  if(s == ".avi" ) return "video/avi";
  if(s == ".bml" ) return "text/plain; charset=utf-8";
  if(s == ".bz2" ) return "application/x-bzip2";
  if(s == ".css" ) return "text/css; charset=utf-8";
  if(s == ".gif" ) return "image/gif";
  if(s == ".gz"  ) return "application/gzip";
  if(s == ".htm" ) return "text/html; charset=utf-8";
  if(s == ".html") return "text/html; charset=utf-8";
  if(s == ".jpg" ) return "image/jpeg";
  if(s == ".jpeg") return "image/jpeg";
  if(s == ".js"  ) return "application/javascript";
  if(s == ".mka" ) return "audio/x-matroska";
  if(s == ".mkv" ) return "video/x-matroska";
  if(s == ".mp3" ) return "audio/mpeg";
  if(s == ".mp4" ) return "video/mp4";
  if(s == ".mpeg") return "video/mpeg";
  if(s == ".mpg" ) return "video/mpeg";
  if(s == ".ogg" ) return "audio/ogg";
  if(s == ".pdf" ) return "application/pdf";
  if(s == ".png" ) return "image/png";
  if(s == ".rar" ) return "application/x-rar-compressed";
  if(s == ".svg" ) return "image/svg+xml";
  if(s == ".tar" ) return "application/x-tar";
  if(s == ".txt" ) return "text/plain; charset=utf-8";
  if(s == ".wav" ) return "audio/vnd.wave";
  if(s == ".webm") return "video/webm";
  if(s == ".xml" ) return "text/xml; charset=utf-8";
  if(s == ".xz"  ) return "application/x-xz";
  if(s == ".zip" ) return "application/zip";
  return "application/octet-stream";  //binary
}

auto httpResponse::findResponseType() const -> string {
  switch(responseType()) {
  case 200: return "200 OK";
  case 301: return "301 Moved Permanently";
  case 302: return "302 Found";
  case 303: return "303 See Other";
  case 304: return "304 Not Modified";
  case 307: return "307 Temporary Redirect";
  case 400: return "400 Bad Request";
  case 403: return "403 Forbidden";
  case 404: return "404 Not Found";
  case 500: return "500 Internal Server Error";
  case 501: return "501 Not Implemented";
  case 503: return "503 Service Unavailable";
  }
  return "501 Not Implemented";
}

auto httpResponse::setData(const vector<uint8_t>& value) -> type& {
  _data = value;
  setHeader("Content-Length", value.size());
  return *this;
}

auto httpResponse::setFile(const string& value) -> type& {
  _file = value;
  string eTag = {"\"", string::datetime(file::timestamp(value, file::time::modify)), "\""};
  setHeader("Content-Length", file::size(value));
  setHeader("Cache-Control", "public");
  setHeader("ETag", eTag);
  return *this;
}

auto httpResponse::setText(const string& value) -> type& {
  _text = value;
  setHeader("Content-Length", value.size());
  return *this;
}

}

#endif
