#ifndef __UTIL_H__
#define __UTIL_H__

#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdint.h>

#include "aes.h"

using namespace std;

const uint32_t BLOCK_SIZE = sizeof(block);

struct AESContext {
  static const uint32_t AES_KEY_LEN = 16;

  AESContext();
  AESContext(uint8_t* keyBuf);
  ~AESContext();

  void Init(uint8_t* keyBuf);
  void GetBlock(block& output, uint64_t tag, uint64_t ind) const;
  void GetBlocks(block* buf, uint32_t nBlocks, uint64_t tag) const;
  void FillBuffer(uint8_t* buf, uint32_t len, uint64_t tag) const;

  AESKey key;
  uint8_t keyBytes[AES_KEY_LEN];
};

#endif
