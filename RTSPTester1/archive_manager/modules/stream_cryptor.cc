#include "stream_cryptor.h"

#include <iostream>
#include <fstream>

StreamCryptor::StreamCryptor() {
  enc_ctx = EVP_CIPHER_CTX_new();
  dec_ctx = EVP_CIPHER_CTX_new();
  buffer_cipher_size = 0;
  buffer_plain_size = 0;
}

StreamCryptor::~StreamCryptor() {
  EVP_CIPHER_CTX_free(enc_ctx);
  EVP_CIPHER_CTX_free(dec_ctx);
  if (cipher_output != nullptr)
    delete[] cipher_output;

  if (plain_output != nullptr)
    delete[] plain_output;
}

Pnx::Media::MediaSourceFramePtr StreamCryptor::Encrypt(Pnx::Media::MediaSourceFramePtr pnx_media_data, EncryptionType encrypt_type) {

  std::lock_guard<std::mutex> lock(worker_mtx_);
  if (encrypt_type == EncryptionType::kEncryptionAES128) {
    EVP_EncryptInit_ex(enc_ctx, EVP_aes_128_cbc(), NULL, key, iv);
  } else if (encrypt_type == EncryptionType::kEncryptionAES192) {
    EVP_EncryptInit_ex(enc_ctx, EVP_aes_192_cbc(), NULL, key, iv);
  } else if (encrypt_type == EncryptionType::kEncryptionAES256) {
    EVP_EncryptInit_ex(enc_ctx, EVP_aes_256_cbc(), NULL, key, iv);
  } else {
    return nullptr;
  }
  int plain_len = pnx_media_data->dataSize();
  if (cipher_output == nullptr || buffer_cipher_size < plain_len + AES_BLOCK_SIZE * 2) {
    if (cipher_output != nullptr) {
      delete[] cipher_output;
      cipher_output = nullptr;
    }

    buffer_cipher_size = plain_len + AES_BLOCK_SIZE * 2;
    cipher_output = new unsigned char[buffer_cipher_size];
  }
  int cipher_output_len = 0;
  if (1 != EVP_EncryptUpdate(enc_ctx, cipher_output, &cipher_output_len, pnx_media_data->data(), plain_len)) {
    ERR_print_errors_fp(stderr);
    return nullptr;
  }
  int final_len = 0;
  if (1 != EVP_EncryptFinal_ex(enc_ctx, cipher_output + cipher_output_len, &final_len)) {
    ERR_print_errors_fp(stderr);
    return nullptr;
  }
  cipher_output_len += final_len;
  Pnx::Media::MediaSourceFramePtr enc_media_data = std::make_shared<Pnx::Media::MediaSourceFrame>(cipher_output, cipher_output_len);
  return enc_media_data;
}

Pnx::Media::MediaSourceFramePtr StreamCryptor::Decrypt(const unsigned char* in, unsigned int size, ArchiveType archive_type, EncryptionType encrypt_type) {
  std::lock_guard<std::mutex> lock(worker_mtx_);
  if (encrypt_type == EncryptionType::kEncryptionAES128) {
    EVP_DecryptInit_ex(dec_ctx, EVP_aes_128_cbc(), NULL, key, iv);
  } else if (encrypt_type == EncryptionType::kEncryptionAES192) {
    EVP_DecryptInit_ex(dec_ctx, EVP_aes_192_cbc(), NULL, key, iv);
  } else if (encrypt_type == EncryptionType::kEncryptionAES256) {
    EVP_DecryptInit_ex(dec_ctx, EVP_aes_256_cbc(), NULL, key, iv);
  } else {
    return nullptr;
  }

  if (plain_output == nullptr || buffer_plain_size < size + block_size_ * 2) {
    if (plain_output != nullptr) {
      delete[] plain_output;
      plain_output = nullptr;
    }
    buffer_plain_size = size + block_size_ * 2;
    plain_output = new unsigned char[buffer_plain_size];
  }

  int plain_output_len = 0;
  if (1 != EVP_DecryptUpdate(dec_ctx, plain_output, &plain_output_len, in, size)) {
    ERR_print_errors_fp(stderr);
    return nullptr;
  }
  int final_len = 0;
  if (1 != EVP_DecryptFinal_ex(dec_ctx, plain_output + plain_output_len, &final_len)) {
    ERR_print_errors_fp(stderr);
    return nullptr;
  }

  Pnx::Media::MediaSourceFramePtr frame = nullptr;
  if (archive_type == kArchiveTypeFrameVideo)
    frame = std::make_shared<Pnx::Media::VideoSourceFrame>(plain_output, plain_output_len);
  else if (archive_type == kArchiveTypeAudio)
    frame = std::make_shared<Pnx::Media::AudioSourceFrame>(plain_output, plain_output_len);
  else if (archive_type == kArchiveTypeMeta)
    frame = std::make_shared<Pnx::Media::MetaDataSourceFrame>(plain_output, plain_output_len);

  return frame;
}