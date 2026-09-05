#pragma once

#include <stddef.h>

class HAKVPairList {
protected:
  const char* key;
  const char* value;
  HAKVPairList* next;

public:
  HAKVPairList(const char* key, const char* value);
  void append(const char* key, const char* value = NULL);
  inline const char* getKey() { return key; }
  inline const char* getValue() { return value; }
  inline HAKVPairList* getNext() { return next; }
};
