#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#include "archive_define.h"

class StreamCryptor {

 public:
  StreamCryptor();
  ~StreamCryptor();
  Pnx::Media::MediaSourceFramePtr Encrypt(Pnx::Media::MediaSourceFramePtr pnx_media_data, EncryptionType encrypt_type);
  Pnx::Media::MediaSourceFramePtr Decrypt(const unsigned char* in, unsigned int size, ArchiveType archive_type, EncryptionType encrypt_type);

 private:
  EVP_CIPHER_CTX* enc_ctx = nullptr;
  EVP_CIPHER_CTX* dec_ctx = nullptr;
  std::mutex worker_mtx_;
  unsigned char* cipher_output = nullptr;
  unsigned int buffer_cipher_size = 0;

  unsigned char* plain_output = nullptr;
  unsigned int buffer_plain_size = 0;
  int block_size_ = 0;
  unsigned char key[16] = {0xA5, 0xB3, 0x7C, 0x12, 0x84, 0xD9, 0x65, 0x2F, 0x39, 0x18, 0xE1, 0x4B, 0x7F, 0xC6, 0x92, 0xD1};
  unsigned char iv[16] = {0x8E, 0x27, 0x41, 0xF0, 0x6C, 0xB5, 0xD3, 0x78, 0x1E, 0xA9, 0x45, 0x3C, 0xB8, 0x62, 0xF4, 0x10};
};
