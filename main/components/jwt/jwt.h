#include "../crypto/sha256.h"
#include "../crypto/ecdsa.h"
#include "../crypto/nn.h"

#include <stdlib.h> // For malloc

size_t b64_encoded_size(size_t inlen);

// const char b64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char b64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"; // base64url

char *b64_encode(const unsigned char *in, size_t len);

// static const char *private_key =
//     "ab:05:1e:33:36:d9:b0:1e:b2:00:1a:b2:da:1c:21:"
//     "84:bf:ee:46:5e:3a:7d:3f:11:1f:73:a6:4b:bd:d7:"
//     "5d:f6";

// NN_DIGIT priv_key[9];

void fillPrivateKey(const char *private_key, NN_DIGIT *priv_key);

char *createJwt(const char *private_key, const char *project_id, uint32_t exp_sec, uint32_t cur_time);