#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <openssl/bio.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/hmac.h>

/**
 * BASE64编码
 *
 * LUA示例:
 * local codec = require('codec')
 * local bs = [[...]] --源内容
 * local result = codec.base64_encode(bs)
 */
int codec_base64_encode(lua_State *L)
{
  size_t len;
  const char *bs = luaL_checklstring(L, 1, &len);
  BIO *b64 = BIO_new(BIO_f_base64());
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  BIO *bio = BIO_new(BIO_s_mem());
  bio = BIO_push(b64, bio);
  BIO_write(bio, bs, len);
  BIO_flush(bio);
  BUF_MEM *p;
  BIO_get_mem_ptr(bio, &p);
  int n = p->length;
  char dst[n];
  memcpy(dst, p->data, n);
  BIO_free_all(bio);

  lua_pushlstring(L, dst, n);
  return 1;
}

/**
 * BASE64解码
 *
 * LUA示例:
 * local codec = require('codec')
 * local base64str = [[...]] --BASE64字符串，无换行
 * local result = codec.base64_decode(base64str)
 */
int codec_base64_decode(lua_State *L)
{
  size_t len;
  const char *cs = luaL_checklstring(L, 1, &len);
  BIO *b64 = BIO_new(BIO_f_base64());
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  BIO *bio = BIO_new_mem_buf((void *)cs, len);
  bio = BIO_push(b64, bio);
  char dst[len];
  int n = BIO_read(bio, dst, len);
  BIO_free_all(bio);

  lua_pushlstring(L, dst, n);
  return 1;
}

/**
 * MD5编码
 *
 * LUA示例:
 * local codec = require('codec')
 * local src = [[...]]
 * local result = codec.md5_encode(src)
 */
int codec_md5_encode(lua_State *L)
{
  size_t len;
  const char *cs = luaL_checklstring(L, 1, &len);
  unsigned char bs[16];
  char dst[32];

  MD5((unsigned char *)cs, len, bs);

  int i;
  for(i = 0; i < 16; i++)
    sprintf(dst + i * 2, "%02x", bs[i]);

  lua_pushlstring(L, dst, 32);
  return 1;
}

/**
 * HMAC-SHA1编码
 *
 * LUA示例:
 * local codec = require('codec')
 * local src = [[...]]
 * local key = [[...]]
 * local result = codec.hmac_sha1_encode(src, key)
 */
int codec_hmac_sha1_encode(lua_State *L)
{
  size_t len, klen;
  const char *cs = luaL_checklstring(L, 1, &len);
  const char *key = luaL_checklstring(L, 2, &klen);
  unsigned char bs[EVP_MAX_MD_SIZE];

  unsigned int n;
  const EVP_MD *evp = EVP_sha1();
  HMAC(evp, key, klen, (unsigned char *)cs, len, bs, &n);

  int hexn = n * 2, i;
  char dst[hexn];
  for(i = 0; i < n; i++)
    sprintf(dst + i * 2, "%02x", bs[i]);

  lua_pushlstring(L, dst, hexn);
  return 1;
}

/**
 * AES-ECB-PKCS5Padding加密
 *
 * LUA示例:
 * local codec = require('codec')
 * local src = 'something'
 * local key = [[...]] --16位数字串
 * local bs = codec.aes_encrypt(src, key)
 * local dst = codec.base64_encode(bs) --BASE64密文
 */
int codec_aes_encrypt(lua_State *L)
{
  size_t len;
  const char *src = luaL_checklstring(L, 1, &len);
  const char *key = luaL_checkstring(L, 2);

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

  int ret = EVP_EncryptInit(ctx, EVP_aes_128_ecb(), (unsigned char *)key, NULL);
  if(ret != 1)
  {
    EVP_CIPHER_CTX_free(ctx);
    return luaL_error(L, "EVP encrypt init error");
  }

  int dstn = len + 128, n, wn;
  char dst[dstn];
  memset(dst, 0, dstn);

  ret = EVP_EncryptUpdate(ctx, (unsigned char *)dst, &wn, (unsigned char *)src, len);
  if(ret != 1)
  {
    EVP_CIPHER_CTX_free(ctx);
    return luaL_error(L, "EVP encrypt update error");
  }
  n = wn;

  ret = EVP_EncryptFinal(ctx, (unsigned char *)(dst + n), &wn);
  if(ret != 1)
  {
    EVP_CIPHER_CTX_free(ctx);
    return luaL_error(L, "EVP encrypt final error");
  }
  EVP_CIPHER_CTX_free(ctx);
  n += wn;

  lua_pushlstring(L, dst, n);
  return 1;
}

/**
 * AES-ECB-PKCS5Padding解密
 *
 * LUA示例:
 * local codec = require('codec')
 * local src = [[...]] --BASE64密文
 * local key = [[...]] --16位数字串
 * local bs = codec.base64_decode(src)
 * local dst = codec.aes_decrypt(bs, key)
 */
int codec_aes_decrypt(lua_State *L)
{
  size_t len;
  const char *src = luaL_checklstring(L, 1, &len);
  const char *key = luaL_checkstring(L, 2);

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

  int ret = EVP_DecryptInit(ctx, EVP_aes_128_ecb(), (unsigned char *)key, NULL);
  if(ret != 1)
  {
    EVP_CIPHER_CTX_free(ctx);
    return luaL_error(L, "EVP decrypt init error");
  }

  int n, wn;
  char dst[len];
  memset(dst, 0, len);

  ret = EVP_DecryptUpdate(ctx, (unsigned char *)dst, &wn, (unsigned char *)src, len);
  if(ret != 1)
  {
    EVP_CIPHER_CTX_free(ctx);
    return luaL_error(L, "EVP decrypt update error");
  }
  n = wn;

  ret = EVP_DecryptFinal(ctx, (unsigned char *)(dst + n), &wn);
  if(ret != 1)
  {
    EVP_CIPHER_CTX_free(ctx);
    return luaL_error(L, "EVP decrypt final error");
  }
  EVP_CIPHER_CTX_free(ctx);
  n += wn;

  lua_pushlstring(L, dst, n);
  return 1;
}

/**
 * SHA1WithRSA私钥签名
 *
 * LUA示例:
 * local codec = require('codec')
 * local src = 'something'
 * local pem = [[...]] --私钥PEM字符串
 * local bs = codec.rsa_sign(src, pem)
 * local dst = codec.base64_encode(bs) --BASE64签名
 */
int codec_rsa_sign(lua_State *L)
{
  size_t len;
  const char *src = luaL_checklstring(L, 1, &len);
  const char *pem = luaL_checkstring(L, 2);

  SHA_CTX c;
  unsigned char sha[SHA_DIGEST_LENGTH];
  memset(sha, 0, SHA_DIGEST_LENGTH);
  if(SHA1_Init(&c) != 1)
  {
    OPENSSL_cleanse(&c, sizeof(c));
    return luaL_error(L, "SHA init error");
  }
  if(SHA1_Update(&c, src, len) != 1)
  {
    OPENSSL_cleanse(&c, sizeof(c));
    return luaL_error(L, "SHA update error");
  }
  if(SHA1_Final(sha, &c) != 1)
  {
    OPENSSL_cleanse(&c, sizeof(c));
    return luaL_error(L, "SHA update error");
  }
  OPENSSL_cleanse(&c, sizeof(c));

  BIO *bio = BIO_new_mem_buf((void *)pem, -1);
  if(bio == NULL)
  {
    BIO_free_all(bio);
    return luaL_error(L, "PEM error");
  }
  RSA *rsa = PEM_read_bio_RSAPrivateKey(bio, NULL, NULL, NULL);
  if(rsa == NULL)
  {
    BIO_free_all(bio);
    return luaL_error(L, "RSA read private key error");
  }
  BIO_free_all(bio);

  int n = RSA_size(rsa), wn;
  char dst[n];
  memset(dst, 0, n);

  int ret = RSA_sign(NID_sha1, (unsigned char *)sha, SHA_DIGEST_LENGTH, (unsigned char *)dst, (unsigned int *)&wn,
          rsa);
  if(ret != 1)
  {
    RSA_free(rsa);
    BIO_free_all(bio);
    return luaL_error(L, "RSA sign error");
  }
  RSA_free(rsa);

  lua_pushlstring(L, dst, wn);
  return 1;
}

/**
 * SHA1WithRSA公钥验签
 *
 * LUA示例:
 * local codec = require('codec')
 * local src = 'something'
 * local sign = [[...]] --BASE64签名
 * local bs = codec.base64_decode(sign)
 * local pem = [[...]] --公钥PEM字符串
 * local type = 1
 * local ok = codec.rsa_verify(src, bs, pem, type) --true/false
 */
int codec_rsa_verify(lua_State *L)
{
  size_t srclen, signlen;
  const char *src = luaL_checklstring(L, 1, &srclen);
  const char *sign = luaL_checklstring(L, 2, &signlen);
  const char *pem = luaL_checkstring(L, 3);
  int type = luaL_checkinteger(L, 4);

  SHA_CTX ctx;
  int ctxlen = sizeof(ctx);
  unsigned char sha[SHA_DIGEST_LENGTH];
  memset(sha, 0, SHA_DIGEST_LENGTH);
  if(SHA1_Init(&ctx) != 1)
  {
    OPENSSL_cleanse(&ctx, ctxlen);
    return luaL_error(L, "SHA init error");
  }
  if(SHA1_Update(&ctx, src, srclen) != 1)
  {
    OPENSSL_cleanse(&ctx, ctxlen);
    return luaL_error(L, "SHA update error");
  }
  if(SHA1_Final(sha, &ctx) != 1)
  {
    OPENSSL_cleanse(&ctx, ctxlen);
    return luaL_error(L, "SHA update error");
  }
  OPENSSL_cleanse(&ctx, ctxlen);

  BIO *bio = BIO_new_mem_buf((void *)pem, -1);
  if(bio == NULL)
  {
    BIO_free_all(bio);
    return luaL_error(L, "PEM error");
  }
  RSA *rsa = type == 1 ? PEM_read_bio_RSAPublicKey(bio, NULL, NULL, NULL) : PEM_read_bio_RSA_PUBKEY(bio, NULL, NULL, NULL);
  if(rsa == NULL)
  {
    BIO_free_all(bio);
    return luaL_error(L, "RSA read public key error");
  }
  BIO_free_all(bio);

  int ret = RSA_verify(NID_sha1, sha, SHA_DIGEST_LENGTH, (unsigned char *)sign, signlen, rsa);
  RSA_free(rsa);

  lua_pushboolean(L, ret);
  return 1;
}

/**
 * SHA1WithRSA私钥签名
 *
 * LUA示例:
 * local codec = require('codec')
 * local src = 'something'
 * local pem = [[...]] --私钥PEM字符串
 * local bs = codec.rsa_sign(src, pem)
 * local dst = codec.base64_encode(bs) --BASE64签名
 */
int codec_rsa_sha256_sign(lua_State *L)
{
  size_t len;
  const char *src = luaL_checklstring(L, 1, &len);
  const char *pem = luaL_checkstring(L, 2);

  SHA256_CTX c;
  unsigned char sha[SHA256_DIGEST_LENGTH];
  memset(sha, 0, SHA256_DIGEST_LENGTH);
  if(SHA256_Init(&c) != 1)
  {
    OPENSSL_cleanse(&c, sizeof(c));
    return luaL_error(L, "SHA init error");
  }
  if(SHA256_Update(&c, src, len) != 1)
  {
    OPENSSL_cleanse(&c, sizeof(c));
    return luaL_error(L, "SHA update error");
  }
  if(SHA256_Final(sha, &c) != 1)
  {
    OPENSSL_cleanse(&c, sizeof(c));
    return luaL_error(L, "SHA update error");
  }
  OPENSSL_cleanse(&c, sizeof(c));

  BIO *bio = BIO_new_mem_buf((void *)pem, -1);
  if(bio == NULL)
  {
    BIO_free_all(bio);
    return luaL_error(L, "PEM error");
  }
  RSA *rsa = PEM_read_bio_RSAPrivateKey(bio, NULL, NULL, NULL);
  if(rsa == NULL)
  {
    BIO_free_all(bio);
    return luaL_error(L, "RSA read private key error");
  }
  BIO_free_all(bio);

  int n = RSA_size(rsa), wn;
  char dst[n];
  memset(dst, 0, n);

  int ret = RSA_sign(NID_sha256, (unsigned char *)sha, SHA256_DIGEST_LENGTH, (unsigned char *)dst, (unsigned int *)&wn,
          rsa);
  if(ret != 1)
  {
    RSA_free(rsa);
    BIO_free_all(bio);
    return luaL_error(L, "RSA sign error");
  }
  RSA_free(rsa);

  lua_pushlstring(L, dst, wn);
  return 1;
}

/**
 * SHA1WithRSA公钥验签
 *
 * LUA示例:
 * local codec = require('codec')
 * local src = 'something'
 * local sign = [[...]] --BASE64签名
 * local bs = codec.base64_decode(sign)
 * local pem = [[...]] --公钥PEM字符串
 * local type = 1
 * local ok = codec.rsa_verify(src, bs, pem, type) --true/false
 */
int codec_rsa_sha256_verify(lua_State *L)
{
  size_t srclen, signlen;
  const char *src = luaL_checklstring(L, 1, &srclen);
  const char *sign = luaL_checklstring(L, 2, &signlen);
  const char *pem = luaL_checkstring(L, 3);
  int type = luaL_checkinteger(L, 4);

  SHA256_CTX ctx;
  int ctxlen = sizeof(ctx);
  unsigned char sha[SHA256_DIGEST_LENGTH];
  memset(sha, 0, SHA256_DIGEST_LENGTH);
  if(SHA256_Init(&ctx) != 1)
  {
    OPENSSL_cleanse(&ctx, ctxlen);
    return luaL_error(L, "SHA init error");
  }
  if(SHA256_Update(&ctx, src, srclen) != 1)
  {
    OPENSSL_cleanse(&ctx, ctxlen);
    return luaL_error(L, "SHA update error");
  }
  if(SHA256_Final(sha, &ctx) != 1)
  {
    OPENSSL_cleanse(&ctx, ctxlen);
    return luaL_error(L, "SHA update error");
  }
  OPENSSL_cleanse(&ctx, ctxlen);

  BIO *bio = BIO_new_mem_buf((void *)pem, -1);
  if(bio == NULL)
  {
    BIO_free_all(bio);
    return luaL_error(L, "PEM error");
  }
  RSA *rsa = type == 1 ? PEM_read_bio_RSAPublicKey(bio, NULL, NULL, NULL) : PEM_read_bio_RSA_PUBKEY(bio, NULL, NULL, NULL);
  if(rsa == NULL)
  {
    BIO_free_all(bio);
    return luaL_error(L, "RSA read public key error");
  }
  BIO_free_all(bio);

  int ret = RSA_verify(NID_sha256, sha, SHA256_DIGEST_LENGTH, (unsigned char *)sign, signlen, rsa);
  RSA_free(rsa);

  lua_pushboolean(L, ret);
  return 1;
}

/**
 * RSA公钥加密
 *
 * LUA示例:
 * local codec = require('codec')
 * local src = 'something'
 * local pem = [[...]] --公钥PEM字符串
 * local type = 1
 * local bs = codec.rsa_public_encrypt(src, pem, type)
 * local dst = codec.base64_encode(bs) --BASE64密文
 */
int codec_rsa_public_encrypt(lua_State *L)
{
  size_t len;
  const char *src = luaL_checklstring(L, 1, &len);
  const char *pem = luaL_checkstring(L, 2);
  int type = luaL_checkinteger(L, 3);

  BIO *bio = BIO_new_mem_buf((void *)pem, -1);
  if(bio == NULL)
  {
    BIO_free_all(bio);
    return luaL_error(L, "PEM error");
  }
  RSA *rsa = type == 1 ? PEM_read_bio_RSAPublicKey(bio, NULL, NULL, NULL) : PEM_read_bio_RSA_PUBKEY(bio, NULL, NULL, NULL);
  if(rsa == NULL)
  {
    BIO_free_all(bio);
    return luaL_error(L, "RSA read public key error");
  }
  BIO_free_all(bio);

  int n = RSA_size(rsa);
  char dst[n];
  memset(dst, 0, n);

  int ret = RSA_public_encrypt(len, (unsigned char *)src, (unsigned char *)dst, rsa, RSA_PKCS1_PADDING);
  if(ret != n)
  {
    RSA_free(rsa);
    BIO_free_all(bio);
    return luaL_error(L, "RSA public encrypt error");
  }
  RSA_free(rsa);

  lua_pushlstring(L, dst, n);
  return 1;
}

/**
 * RSA私钥解密
 *
 * LUA示例:
 * local codec = require('codec')
 * local src = [[...]] --BASE64密文
 * local bs = codec.base64_decode(src)
 * local pem = [[...]] --私钥PEM字符串
 * local dst = codec.rsa_private_decrypt(bs, pem)
 */
int codec_rsa_private_decrypt(lua_State *L)
{
  const char *src = luaL_checkstring(L, 1);
  const char *pem = luaL_checkstring(L, 2);

  BIO *bio = BIO_new_mem_buf((void *)pem, -1);
  if(bio == NULL)
  {
    BIO_free_all(bio);
    return luaL_error(L, "PEM error");
  }
  RSA *rsa = PEM_read_bio_RSAPrivateKey(bio, NULL, NULL, NULL);
  if(rsa == NULL)
  {
    BIO_free_all(bio);
    return luaL_error(L, "RSA read private key error");
  }
  BIO_free_all(bio);

  int n = RSA_size(rsa);
  char dst[n];
  memset(dst, 0, n);

  int ret = RSA_private_decrypt(n, (unsigned char *)src, (unsigned char *)dst, rsa, RSA_PKCS1_PADDING);
  if(ret <= 0)
  {
    RSA_free(rsa);
    BIO_free_all(bio);
    return luaL_error(L, "RSA private decrypt error");
  }
  RSA_free(rsa);

  lua_pushlstring(L, dst, ret);
  return 1;
}
