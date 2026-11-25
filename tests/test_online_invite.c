#include "online_invite.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static size_t symbols(const char *s) {
    size_t n = 0;
    for (; *s; s++) n += *s != '-';
    return n;
}

int main(void) {
    char txt[64];
    int fam;
    uint8_t out[16];
    uint16_t port;

    const uint8_t v4[4] = {192, 168, 1, 77};
    CHECK(online_invite_encode(ONLINE_INVITE_V4, v4, 8123, txt, sizeof txt));
    CHECK(symbols(txt) <= 15);
    CHECK(strlen(txt) == 18 && txt[4] == '-' && txt[9] == '-' && txt[14] == '-');
    CHECK(online_invite_decode(txt, &fam, out, &port));
    CHECK(fam == ONLINE_INVITE_V4 && port == 8123 && !memcmp(out, v4, 4));

    // lower case with spaces instead of dashes
    {
        char alt[64];
        size_t k = 0;
        for (const char *s = txt; *s; s++) {
            char c = *s;
            if (c == '-') {
                alt[k++] = ' ';
                continue;
            }
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            alt[k++] = c;
        }
        alt[k] = 0;
        CHECK(online_invite_decode(alt, &fam, out, &port) && port == 8123);
    }
    // O for 0, I and l for 1
    {
        char t2[64];
        strcpy(t2, txt);
        for (char *s = t2; *s; s++) {
            if (*s == '0') *s = 'O';
            else if (*s == '1') *s = ((s - t2) % 2) ? 'I' : 'l';
        }
        CHECK(online_invite_decode(t2, &fam, out, &port) && port == 8123);
    }
    // a code with no 0/1 in it must also be exercised: many addresses round-trip
    for (int i = 0; i < 256; i++) {
        uint8_t a[4] = {(uint8_t)i, (uint8_t)(i * 3), (uint8_t)(i * 7), (uint8_t)(255 - i)};
        uint16_t p = (uint16_t)(1 + i * 251);
        CHECK(online_invite_encode(ONLINE_INVITE_V4, a, p, txt, sizeof txt));
        CHECK(online_invite_decode(txt, &fam, out, &port) && port == p && !memcmp(out, a, 4));
    }

    // IPv6
    uint8_t v6[16];
    for (int i = 0; i < 16; i++) v6[i] = (uint8_t)(0x20 + i * 7);
    CHECK(online_invite_encode(ONLINE_INVITE_V6, v6, 65535, txt, sizeof txt));
    CHECK(strlen(txt) < ONLINE_INVITE_TEXT_V6 && symbols(txt) == 34);
    CHECK(online_invite_decode(txt, &fam, out, &port));
    CHECK(fam == ONLINE_INVITE_V6 && port == 65535 && !memcmp(out, v6, 16));

    // every single changed symbol is rejected
    CHECK(online_invite_encode(ONLINE_INVITE_V4, v4, 8123, txt, sizeof txt));
    for (size_t i = 0; txt[i]; i++) {
        if (txt[i] == '-') continue;
        char saved = txt[i];
        txt[i] = saved == 'Z' ? 'Y' : 'Z';
        CHECK(!online_invite_decode(txt, &fam, out, &port));
        txt[i] = saved;
    }
    CHECK(online_invite_decode(txt, &fam, out, &port));

    // bad inputs
    CHECK(!online_invite_decode("", &fam, out, &port));
    CHECK(!online_invite_decode("----  ", &fam, out, &port));
    CHECK(!online_invite_decode(NULL, &fam, out, &port));
    CHECK(!online_invite_decode("ABCD-U123-4567-89A", &fam, out, &port));  // U is not in the alphabet
    CHECK(!online_invite_decode("ABCD-EFGH-JKMN-PQ!", &fam, out, &port));
    {
        static char big[1001];
        memset(big, 'A', 1000);
        big[1000] = 0;
        CHECK(!online_invite_decode(big, &fam, out, &port));
        memset(big, '-', 1000);  // only separators, long
        CHECK(!online_invite_decode(big, &fam, out, &port));
    }
    CHECK(!online_invite_encode(ONLINE_INVITE_V4, v4, 0, txt, sizeof txt));
    CHECK(!online_invite_encode(5, v4, 80, txt, sizeof txt));
    CHECK(!online_invite_encode(ONLINE_INVITE_V4, NULL, 80, txt, sizeof txt));

    // cap: the exact size works, one less fails and nothing is written past cap
    {
        char tight[ONLINE_INVITE_TEXT_V4 + 2];
        memset(tight, 'x', sizeof tight);
        CHECK(online_invite_encode(ONLINE_INVITE_V4, v4, 80, tight, ONLINE_INVITE_TEXT_V4));
        CHECK(tight[ONLINE_INVITE_TEXT_V4] == 'x' && tight[ONLINE_INVITE_TEXT_V4 + 1] == 'x');
        memset(tight, 'x', sizeof tight);
        CHECK(!online_invite_encode(ONLINE_INVITE_V4, v4, 80, tight, ONLINE_INVITE_TEXT_V4 - 1));
        CHECK(tight[0] == 'x');
        CHECK(!online_invite_encode(ONLINE_INVITE_V4, v4, 80, tight, 0));
        char t6[ONLINE_INVITE_TEXT_V6 + 2];
        memset(t6, 'x', sizeof t6);
        CHECK(online_invite_encode(ONLINE_INVITE_V6, v6, 80, t6, ONLINE_INVITE_TEXT_V6));
        CHECK(t6[ONLINE_INVITE_TEXT_V6] == 'x');
    }

    // distinct inputs give distinct codes
    {
        char a[64], b[64];
        const uint8_t v4b[4] = {192, 168, 1, 78};
        online_invite_encode(ONLINE_INVITE_V4, v4, 80, a, sizeof a);
        online_invite_encode(ONLINE_INVITE_V4, v4b, 80, b, sizeof b);
        CHECK(strcmp(a, b) != 0);
    }

    if (fails) {
        printf("%d failure(s)\n", fails);
        return 1;
    }
    printf("test_online_invite: ok\n");
    return 0;
}
