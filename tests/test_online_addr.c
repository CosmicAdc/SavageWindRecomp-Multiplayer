#include "online_addr.h"
#include "online_invite.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

// parse text with default port 7777; true if it parsed to exactly host:port
static bool is(const char *text, const char *want_host, unsigned want_port) {
    char host[ONLINE_ADDR_HOST_MAX];
    uint16_t port = 0;
    if (!online_addr_parse(text, host, sizeof host, &port, 7777)) return false;
    return !strcmp(host, want_host) && port == want_port;
}

static bool bad(const char *text) {
    char host[ONLINE_ADDR_HOST_MAX];
    uint16_t port = 0;
    return !online_addr_parse(text, host, sizeof host, &port, 7777);
}

int main(void) {
    // the eight formats
    CHECK(is("mipc", "mipc", 7777));
    CHECK(is("mipc:5000", "mipc", 5000));
    CHECK(is("example.com:80", "example.com", 80));
    CHECK(is("1.2.3.4", "1.2.3.4", 7777));
    CHECK(is("1.2.3.4:5000", "1.2.3.4", 5000));
    CHECK(is("[2001:db8::1]", "2001:db8::1", 7777));
    CHECK(is("[2001:db8::1]:5000", "2001:db8::1", 5000));
    CHECK(is("2001:db8::1", "2001:db8::1", 7777));
    CHECK(is("::1", "::1", 7777));
    CHECK(is("fe80::1%eth0", "fe80::1%eth0", 7777));
    CHECK(is("[fe80::1%12]:6000", "fe80::1%12", 6000));
    CHECK(is("[::ffff:1.2.3.4]:9", "::ffff:1.2.3.4", 9));
    CHECK(is("  1.2.3.4:5000\t", "1.2.3.4", 5000));
    CHECK(is("1.2.3.4:65535", "1.2.3.4", 65535));
    CHECK(is("1.2.3.4:1", "1.2.3.4", 1));

    // invitation codes (IPv4 and IPv6), also typed in lower case and with spaces
    uint8_t v4[4] = {192, 168, 1, 20};
    uint8_t v6[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    char code[ONLINE_INVITE_TEXT_V6];
    CHECK(online_invite_encode(ONLINE_INVITE_V4, v4, 5000, code, sizeof code));
    CHECK(is(code, "192.168.1.20", 5000));
    char lower[ONLINE_INVITE_TEXT_V6];
    for (size_t i = 0; i <= strlen(code); i++) lower[i] = (code[i] >= 'A' && code[i] <= 'Z') ? (char)(code[i] + 32) : code[i];
    CHECK(is(lower, "192.168.1.20", 5000));
    CHECK(online_invite_encode(ONLINE_INVITE_V6, v6, 6000, code, sizeof code));
    CHECK(is(code, "2001:db8::1", 6000));
    // broken checksum: it is not read as a code; letters, digits and '-' make it a valid name, so it stays a name
    code[strlen(code) - 1] = code[strlen(code) - 1] == '0' ? '1' : '0';
    CHECK(is(code, code, 7777));

    // errors
    CHECK(bad(""));
    CHECK(bad("   "));
    CHECK(bad("mipc:0"));
    CHECK(bad("mipc:65536"));
    CHECK(bad("mipc:99999"));
    CHECK(bad("mipc:-1"));
    CHECK(bad("mipc:"));
    CHECK(bad("mipc:12a"));
    CHECK(bad(":5000"));
    CHECK(bad("1.2.3.4:0"));
    CHECK(bad("[2001:db8::1]:70000"));
    CHECK(bad("[2001:db8::1]:"));
    CHECK(bad("[2001:db8::1]x"));
    CHECK(bad("[2001:db8::1"));      // unclosed bracket
    CHECK(bad("[2001:db8::1:5000"));
    CHECK(bad("[]"));
    CHECK(bad("[1.2.3.4]"));         // not IPv6
    CHECK(bad("2001:db8::1]"));
    CHECK(bad("1.2.3"));
    CHECK(bad("1.2.3.4.5"));
    CHECK(bad("256.1.1.1"));
    CHECK(bad("01.2.3.4"));
    CHECK(bad("1..2.3"));
    CHECK(bad("2001:db8::1::2"));
    CHECK(bad("2001:db8:1"));
    CHECK(bad("1:2:3:4:5:6:7:8:9"));
    CHECK(bad("12345::1"));
    CHECK(bad("1:2:3:4:5:6:7:"));
    CHECK(bad("fe80::1%"));
    CHECK(bad("fe80::1%a b"));
    CHECK(bad("my pc"));
    CHECK(bad("mipc\n"));
    CHECK(bad("m\x01pc"));
    CHECK(bad("mi/pc"));
    CHECK(bad(".mipc"));
    CHECK(bad("a..b"));

    // text of 500 characters
    char big[501];
    memset(big, 'a', 500);
    big[500] = 0;
    CHECK(bad(big));
    char edge[300];
    memset(edge, 'a', 255);
    edge[255] = 0;
    CHECK(bad(edge));  // 255 characters but longer than a DNS name (253)
    memset(edge, 'a', 100);
    edge[100] = 0;
    CHECK(is(edge, edge, 7777));

    // buffer too small for the host (needs room for the NUL), and null arguments
    char small[5];
    uint16_t port = 0;
    CHECK(online_addr_parse("abcd", small, 5, &port, 1));
    CHECK(!online_addr_parse("abcde", small, 5, &port, 1));
    CHECK(!online_addr_parse("abcd", small, 4, &port, 1));
    CHECK(!online_addr_parse("abcd", small, 0, &port, 1));
    CHECK(!online_addr_parse(NULL, small, 5, &port, 1));
    CHECK(!online_addr_parse("abcd", NULL, 5, &port, 1));
    CHECK(!online_addr_parse("abcd", small, 5, NULL, 1));

    // classify
    uint8_t a[16] = {0};
    a[0] = 10;
    CHECK(online_addr_classify(ONLINE_INVITE_V4, a) == ONLINE_ADDR_V4_PRIVATE);
    a[0] = 172; a[1] = 16;
    CHECK(online_addr_classify(ONLINE_INVITE_V4, a) == ONLINE_ADDR_V4_PRIVATE);
    a[1] = 32;
    CHECK(online_addr_classify(ONLINE_INVITE_V4, a) == ONLINE_ADDR_V4_PUBLIC);
    a[0] = 192; a[1] = 168;
    CHECK(online_addr_classify(ONLINE_INVITE_V4, a) == ONLINE_ADDR_V4_PRIVATE);
    a[0] = 8; a[1] = 8;
    CHECK(online_addr_classify(ONLINE_INVITE_V4, a) == ONLINE_ADDR_V4_PUBLIC);
    a[0] = 127; a[1] = 0;
    CHECK(online_addr_classify(ONLINE_INVITE_V4, a) == -1);
    a[0] = 169; a[1] = 254;
    CHECK(online_addr_classify(ONLINE_INVITE_V4, a) == -1);
    a[0] = 0; a[1] = 0;
    CHECK(online_addr_classify(ONLINE_INVITE_V4, a) == -1);
    a[0] = 224;
    CHECK(online_addr_classify(ONLINE_INVITE_V4, a) == -1);
    CHECK(online_addr_classify(ONLINE_INVITE_V6, v6) == ONLINE_ADDR_V6_GLOBAL);
    uint8_t ll[16] = {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    CHECK(online_addr_classify(ONLINE_INVITE_V6, ll) == ONLINE_ADDR_V6_LINK_LOCAL);
    uint8_t lo[16] = {0};
    lo[15] = 1;
    CHECK(online_addr_classify(ONLINE_INVITE_V6, lo) == -1);
    CHECK(online_addr_classify(ONLINE_INVITE_V6, (uint8_t[16]){0}) == -1);
    uint8_t ula[16] = {0xfd, 0x12};
    CHECK(online_addr_classify(ONLINE_INVITE_V6, ula) == -1);
    CHECK(online_addr_classify(5, a) == -1);
    CHECK(online_addr_classify(ONLINE_INVITE_V4, NULL) == -1);

    // format
    char txt[ONLINE_ADDR_TEXT_MAX];
    CHECK(online_addr_format(ONLINE_INVITE_V4, v4, txt, sizeof txt) && !strcmp(txt, "192.168.1.20"));
    CHECK(online_addr_format(ONLINE_INVITE_V6, v6, txt, sizeof txt) && !strcmp(txt, "2001:db8::1"));
    CHECK(online_addr_format(ONLINE_INVITE_V6, lo, txt, sizeof txt) && !strcmp(txt, "::1"));
    CHECK(online_addr_format(ONLINE_INVITE_V6, (uint8_t[16]){0}, txt, sizeof txt) && !strcmp(txt, "::"));
    uint8_t one_zero[16] = {0, 1, 0, 2, 0, 0, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7};  // a single zero group is not shortened
    CHECK(online_addr_format(ONLINE_INVITE_V6, one_zero, txt, sizeof txt) && !strcmp(txt, "1:2:0:3:4:5:6:7"));
    CHECK(!online_addr_format(ONLINE_INVITE_V6, v6, txt, 5));
    CHECK(!online_addr_format(9, v6, txt, sizeof txt));
    // parse keeps an IPv6 as typed; format picks the longest zero run
    CHECK(is("2001:0:0:1:0:0:0:1", "2001:0:0:1:0:0:0:1", 7777));
    uint8_t two_runs[16] = {0x20, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1};
    CHECK(online_addr_format(ONLINE_INVITE_V6, two_runs, txt, sizeof txt) && !strcmp(txt, "2001:0:0:1::1"));

    // local addresses: whatever this machine has, every entry must be well formed and none a loopback
    OnlineLocalAddr list[16];
    int n = online_addr_local(list, 16);
    CHECK(n >= 0 && n <= 16);
    for (int i = 0; i < n; i++) {
        CHECK(list[i].family == ONLINE_INVITE_V4 || list[i].family == ONLINE_INVITE_V6);
        CHECK(online_addr_classify(list[i].family, list[i].addr) == (int)list[i].kind);
        CHECK(list[i].text[0] != 0);
        char again[ONLINE_ADDR_TEXT_MAX];
        CHECK(online_addr_format(list[i].family, list[i].addr, again, sizeof again) && !strcmp(again, list[i].text));
        CHECK(list[i].kind == ONLINE_ADDR_V6_LINK_LOCAL || list[i].scope == 0);
    }
    CHECK(online_addr_local(list, 0) == 0);
    CHECK(online_addr_local(NULL, 4) == 0);
    CHECK(online_addr_local(list, 1) <= 1);

    if (fails) {
        printf("%d failure(s)\n", fails);
        return 1;
    }
    printf("online_addr: ok\n");
    return 0;
}
