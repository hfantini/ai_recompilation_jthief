#include "sha256.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static void to_hex(const uint8_t digest[SHA256_DIGEST_SIZE], char out[SHA256_DIGEST_SIZE * 2 + 1])
{
    int i;
    for (i = 0; i < SHA256_DIGEST_SIZE; i++) {
        sprintf(out + i * 2, "%02x", digest[i]);
    }
}

static void check_digest(const char *input, const char *expected_hex)
{
    uint8_t digest[SHA256_DIGEST_SIZE];
    char hex[SHA256_DIGEST_SIZE * 2 + 1];

    sha256_digest(input, strlen(input), digest);
    to_hex(digest, hex);
    CHECK(strcmp(hex, expected_hex) == 0);
}

static void check_incremental_matches_one_shot(void)
{
    static const char input[] = "The quick brown fox jumps over the lazy dog, again and again and again.";
    uint8_t one_shot[SHA256_DIGEST_SIZE];
    uint8_t incremental[SHA256_DIGEST_SIZE];
    Sha256 ctx;
    size_t i;

    sha256_digest(input, sizeof(input) - 1, one_shot);
    sha256_init(&ctx);
    for (i = 0; i < sizeof(input) - 1; i++) {
        sha256_update(&ctx, input + i, 1);
    }
    sha256_final(&ctx, incremental);
    CHECK(memcmp(one_shot, incremental, sizeof(one_shot)) == 0);
}

int main(void)
{
    /* FIPS 180-2 test vectors. */
    check_digest("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    check_digest("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    check_digest("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                 "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    check_incremental_matches_one_shot();
    return TEST_RESULT();
}
