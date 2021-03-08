#include "util.h"

#include <iomanip>
#include <iostream>
#include <string.h>

using namespace std;

AESContext::AESContext() {
  FILE* f = fopen("/dev/urandom", "r");
  if (f == NULL) {
    throw "unable to open /dev/urandom";
  }

  uint32_t bytesRead = fread(keyBytes, 1, AES_KEY_LEN, f);
  if (bytesRead != AES_KEY_LEN) {
    throw "failed to initialize AES key";
  }

  fclose(f);

  AES_128_Key_Expansion(keyBytes, &key);
}

AESContext::AESContext(uint8_t* keyBuf) {
  Init(keyBuf);
}

AESContext::~AESContext() {
  memset(&key, 0, sizeof(key));
  memset(keyBytes, 0, sizeof(keyBytes));
}

void AESContext::Init(uint8_t* keyBuf) {
  memcpy(keyBytes, keyBuf, AES_KEY_LEN);
  AES_128_Key_Expansion(keyBytes, &key);
}

void AESContext::GetBlock(block& output, uint64_t tag, uint64_t ind) const {
  output = MAKE_BLOCK(tag, ind);
  AES_ecb_encrypt_blk(&output, &key);
}

void AESContext::GetBlocks(block* buf, uint32_t nBlocks, uint64_t tag) const {
  for (uint32_t i = 0; i < nBlocks; i++) {
    buf[i] = MAKE_BLOCK(tag, i);
  }
  AES_ecb_encrypt_blks(buf, nBlocks, &key);
}

void AESContext::FillBuffer(uint8_t* buf, uint32_t len, uint64_t tag) const {
  uint32_t nBlocks = len / sizeof(block);
  uint32_t bytesRemaining = len % sizeof(block);

  GetBlocks((block*) buf, nBlocks, tag);
  if (bytesRemaining != 0) {
    block lastBlock;
    GetBlock(lastBlock, tag, nBlocks);

    memcpy(buf + nBlocks * sizeof(block), &lastBlock, bytesRemaining);
  }
}
