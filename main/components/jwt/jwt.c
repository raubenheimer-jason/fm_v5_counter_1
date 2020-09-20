#include "jwt.h"

static const char *TAG = "JWT";

size_t b64_encoded_size(size_t inlen)
{
    size_t ret;

    ret = inlen;
    if (inlen % 3 != 0)
        ret += 3 - (inlen % 3);
    ret /= 3;
    ret *= 4;

    return ret;
}

// TODO: if there are meant to be padding characters, dont allocate extra memory
char *b64_encode(const unsigned char *in, size_t len)
{
    char *out;
    size_t elen;
    size_t i;
    size_t j;
    size_t v;

    if (in == NULL || len == 0)
        return NULL;

    elen = b64_encoded_size(len);
    out = malloc(elen + 1);

    if (out == NULL)
    {
        ESP_LOGE(TAG, "[b64_encode] out == NULL");
        return NULL;
    }

    out[elen] = '\0';

    for (i = 0, j = 0; i < len; i += 3, j += 4)
    {
        v = in[i];
        v = i + 1 < len ? v << 8 | in[i + 1] : v << 8;
        v = i + 2 < len ? v << 8 | in[i + 2] : v << 8;

        out[j] = b64chars[(v >> 18) & 0x3F];
        out[j + 1] = b64chars[(v >> 12) & 0x3F];
        if (i + 1 < len)
        {
            out[j + 2] = b64chars[(v >> 6) & 0x3F];
        }
        else
        {
            out[j + 2] = '\0';
        }
        if (i + 2 < len)
        {
            out[j + 3] = b64chars[v & 0x3F];
        }
        else
        {
            out[j + 3] = '\0';
        }
    }

    return out;
}

void fillPrivateKey(const char *private_key, NN_DIGIT *priv_key)
{
    if (strlen(private_key) != (95))
    {
        printf("Warning: expected private key to be 95, was: %d", strlen(private_key));
    }

    priv_key[8] = 0;
    for (int i = 7; i >= 0; i--)
    {
        priv_key[i] = 0;
        for (int byte_num = 0; byte_num < 4; byte_num++)
        {
            priv_key[i] = (priv_key[i] << 8) + strtoul(private_key, NULL, 16);
            private_key += 3;
        }
    }
}

/*
-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEIk+73DCFlHAiRJbgxGqt8loMTMD8
woXrlldwibH6K5gRdqn5CpdKahEtcHqgi7z7pr7ioaq4a714bzKNeT3Yug==
-----END PUBLIC KEY-----

-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIKsFHjM22bAesgAastocIYS/7kZeOn0/ER9zpku91132oAoGCCqGSM49
AwEHoUQDQgAEIk+73DCFlHAiRJbgxGqt8loMTMD8woXrlldwibH6K5gRdqn5CpdK
ahEtcHqgi7z7pr7ioaq4a714bzKNeT3Yug==
-----END EC PRIVATE KEY-----
*/

char *createJwt(const char *private_key, const char *project_id, uint32_t exp_sec, uint32_t cur_time)
{
    // Header
    /*
        { "alg": "ES256", "typ": "JWT" }
    */

    const char *jwt_header = "{\"alg\":\"ES256\",\"typ\":\"JWT\"}";

    // Claim set
    /*
        {
        "aud": "my-project",
        "iat": 1509650801,
        "exp": 1509654401
        }
    */

    char iat[33];
    itoa(cur_time, iat, 10);
    char exp[33];
    itoa((cur_time + exp_sec), exp, 10);

    size_t jwt_claim_len = strlen("{\"aud\":\"\",\"iat\":,\"exp\":}");
    jwt_claim_len += strlen(project_id);
    jwt_claim_len += strlen(iat);
    jwt_claim_len += strlen(exp);

    char *jwt_claim = (char *)malloc(jwt_claim_len + 1); // + 1 for \0 // MUST FREE THIS

    if (jwt_claim == NULL)
    {
        ESP_LOGE(TAG, "[createJwt] jwt_claim == NULL");
        return NULL;
    }

    strcpy(jwt_claim, "{\"aud\":\"");
    strcat(jwt_claim, project_id);
    strcat(jwt_claim, "\",\"iat\":");
    strcat(jwt_claim, iat);
    strcat(jwt_claim, ",\"exp\":");
    strcat(jwt_claim, exp);
    strcat(jwt_claim, "}");

    // Combine --> this is the imput for the signature
    /*
        {Base64url encoded header}.{Base64url encoded claim set}
    */

    char *enc_header = b64_encode((const unsigned char *)jwt_header, strlen(jwt_header));
    char *enc_claim = b64_encode((const unsigned char *)jwt_claim, strlen(jwt_claim));

    // printf("%s  %d\n", jwt_header, strlen(jwt_header));
    // printf("%s  %d\n\n", enc_header, strlen(enc_header));
    // printf("%s  %d\n", jwt_claim, strlen(jwt_claim));
    // printf("%s  %d\n", enc_claim, strlen(enc_claim));

    size_t jwt_len = strlen(enc_header) + strlen(enc_claim) + 2;

    char *header_payload_base64 = (char *)malloc(jwt_len); // "header_length" + "." + "claims_length" + "\0"

    if (header_payload_base64 == NULL)
    {
        ESP_LOGE(TAG, "[createJwt] header_payload_base64 == NULL");
        return NULL;
    }

    strcpy(header_payload_base64, enc_header);
    strcat(header_payload_base64, ".");
    strcat(header_payload_base64, enc_claim);
    free(jwt_claim);
    free(enc_header);
    free(enc_claim);

    // printf("\n%s  %d\n", header_payload_base64, strlen(header_payload_base64));

    // Finally:
    /*
        {"alg": "ES256", "typ": "JWT"}.{"aud": "my-project", "iat": 1509650801, "exp": 1509654401}.[signature bytes]
        
        eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJhdWQiOiJteS1wcm9qZWN0IiwiZXhwIjoxNTA5NjUwODAxLCJpYXQiOjE1MDk2NTQ0MDF9.F4iKO0R0wvHkpCcQoyrYttdGxE5FLAgDhbTJQLEHIBPsbL2WkLxXB9IGbDESn9rE7oxn89PJFRtcLn7kJwvdQkQcsPxn2RQorvDAnvAi1w3k8gpxYWo2DYJlnsi7mxXDqSUCNm1UCLRCW68ssYJxYLSg7B1xGMgDADGyYPaIx1EdN4dDbh-WeDyLLa7a8iWVBXdbmy1H3fEuiAyxiZpk2ll7DcQ6ryyMrU2XadwEr9PDqbLe5SrlaJsQbFi8RIdlQJSo_DZGOoAlA5bYTDYXb-skm7qvoaH5uMtOUb0rjijYuuxhNZvZDaBerEaxgmmlO0nQgtn12KVKjmKlisG79Q

    */

    ecc_init();

    Sha256();

    update((const unsigned char *)header_payload_base64, strlen(header_payload_base64));

    unsigned char sha256[32];

    final(sha256); // this is the sha256 --> sha256

    NN_DIGIT priv_key[9];

    fillPrivateKey(private_key, priv_key);

    point_t pub_key;

    ecc_gen_pub_key(priv_key, &pub_key);

    ecdsa_init(&pub_key);

    NN_DIGIT signature_r[NUMWORDS], signature_s[NUMWORDS];

    ecdsa_sign((uint8_t *)sha256, signature_r, signature_s, priv_key);

    unsigned char signature[64];
    NN_Encode(signature, (NUMWORDS - 1) * NN_DIGIT_LEN, signature_r, (NN_UINT)(NUMWORDS - 1));
    NN_Encode(signature + (NUMWORDS - 1) * NN_DIGIT_LEN, (NUMWORDS - 1) * NN_DIGIT_LEN, signature_s, (NN_UINT)(NUMWORDS - 1));

    // printf("\n");

    char *enc_signature = b64_encode(signature, 64);

    // size_t enc_sig_len = strlen(enc_signature);
    // printf("%s  %d\n\n", enc_signature, enc_sig_len);

    size_t comp_len = strlen(header_payload_base64);
    comp_len += 1; // for the .
    comp_len += strlen(enc_signature);
    comp_len += 1; // for the \0

    char *comp_jwt = (char *)malloc(comp_len + 5);

    if (comp_jwt == NULL)
    {
        ESP_LOGE(TAG, "comp_jwt == NULL");
        return NULL;
    }

    strcpy(comp_jwt, header_payload_base64);
    free(header_payload_base64);
    strcat(comp_jwt, ".");
    strcat(comp_jwt, enc_signature);

    return comp_jwt;
}