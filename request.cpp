#include <string.h>
#include <iostream>
#include <map>
#include <sstream>
#include <curl/curl.h>
#include "lib/request.hpp"

using namespace std;

/*
// Pure C implementation. New version below
// New version made this obsolete
// meta->response.data where data = char*
size_t curl_write_cb(void *data, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  CURLMeta *meta = (CURLMeta*)userp;
  char *ptr = (char*)realloc((void*)meta->response.data, meta->response.size + realsize + 1);

  meta->response.data = ptr;
  memcpy(&(meta->response.data[meta->response.size]), data, realsize);
  meta->response.size += realsize;
  meta->response.data[meta->response.size] = 0;

  // curl_on_write
  if (meta->events.on_curl_write != NULL) {
    meta->events.on_curl_write(data, realsize);
  }

  return realsize;
}
*/

size_t curl_write_cb(void *data, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  CURLMeta *meta = (CURLMeta*)userp;

  if (meta->save.data) {
    meta->response.data.append((const char*)data, realsize);
  }

  if (meta->events.on_curl_write != NULL) {
    if (meta->events.on_curl_write(data, realsize) < 0) 
      return -1;
  }

  return realsize;
}

size_t curl_write_headers_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
  size_t realsize = size * nitems;
  CURLMeta *meta = (CURLMeta*)userdata;

  if (meta->save.headers_raw) {
    meta->response.headers_raw.append(buffer, realsize);
  }

  if (meta->events.on_curl_write_headers != NULL) {
    meta->events.on_curl_write_headers(buffer, realsize);
  }

  return realsize;
}

CURLMeta* Request::CURLMetaInit(RequestOptions options) {
  CURLMeta *meta = new CURLMeta {
    .curl = curl_easy_init(),
    .response = { "", "" },
    .events = {
      .on_curl_write_headers = options.events.on_curl_write_headers,
      .on_curl_write = options.events.on_curl_write,
    },
    .save = {
      .headers_raw = options.save.headers_raw,
      .data = options.save.data,
    }
  };

  return meta;
}

void Request::PrepareCurl(CURLMeta *meta, string url,  RequestOptions options) {
  // preparing headers
  for (auto&& [key, value] : options.headers) {
    stringstream stream;
    stream << key << ": " << value;
    meta->headers = curl_slist_append(meta->headers, stream.str().c_str());
  }

  curl_easy_setopt(meta->curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(meta->curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
  curl_easy_setopt(meta->curl, CURLOPT_HEADERFUNCTION, curl_write_headers_cb);
  //curl_easy_setopt(meta->curl, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(meta->curl, CURLOPT_WRITEDATA, (void*)(meta));
  curl_easy_setopt(meta->curl, CURLOPT_HEADERDATA, (void*)(meta));
  curl_easy_setopt(meta->curl, CURLOPT_HTTPHEADER, meta->headers);
  curl_easy_setopt(meta->curl, CURLOPT_BUFFERSIZE, 1024 * 1024); // 1MB
}

void Request::Cleanup(CURLMeta *meta) {
  curl_easy_cleanup(meta->curl);
  curl_slist_free_all(meta->headers);
  delete meta;
}

Response Request::Get(string url, RequestOptions options) {
  CURLMeta *meta = Request::CURLMetaInit(options);
  Response response;

  Request::PrepareCurl(meta, url, options);
  curl_easy_perform(meta->curl);
  response = meta->response;
  Request::Cleanup(meta);

  return response;
}

map<string, string> Request::ParseHeaders(std::vector<std::string> raw) {
  // note: line[pos + 1] == ' ' ? pos + 2 : pos + 1 - will avoid space in the start of the value
  map<string, string> headers;
  int pos;
  string key;

  for (const string line : raw) {
    if ((pos = line.find(":")) == string::npos)
      continue;

    key = line.substr(0, pos);
    headers[key] = line.substr(line[pos + 1] == ' ' ? pos + 2 : pos + 1, line.size() - key.size());
  }

  return headers;
}