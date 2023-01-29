#pragma once
#include <iostream>
#include <map>
#include <vector>
#include <functional>
#include <curl/curl.h>

// if on_curl_write exists, then its value will be important to tell wheter or not curl must stop its request
// it's like routing the original on_write callback, but without the boilerplate
// keep in mind that it will be called upon the arrival of the first chunk, already processed and buffered

struct RequestEvents {
  std::function<void(void*, size_t)> on_curl_write_headers = NULL;
  std::function<int(void*, size_t)> on_curl_write = NULL; // note above
};

struct RequestSave {
  // Should requests save data to buffers (headers_raw, data) for late usage?
  bool headers_raw = true;
  bool data = true;
};

struct RequestOptions {
  std::map<std::string, std::string> headers;
  RequestEvents events;
  RequestSave save;
};

struct Response {
  std::string headers_raw;
  std::string data;
  /*char *data;*/ // Check curl_write_cb_old in request.cpp
  size_t size = 0;
};

struct CURLMeta {
  CURL *curl;
  curl_slist *headers = NULL;
  Response response;
  RequestEvents events;
  RequestSave save;
};

class Request {
private:
  static CURLMeta* CURLMetaInit(RequestOptions options);
  static void PrepareCurl(CURLMeta *meta, std::string url, RequestOptions options);
  static void Cleanup(CURLMeta *meta);
public:
  static Response Get(std::string url, RequestOptions options);
  static std::map<std::string, std::string> ParseHeaders(std::vector<std::string> raw);
};