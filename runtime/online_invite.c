#include "online_invite.h"

#include <string.h>

#define VERSION 1
#define MAX_SCAN 256  // characters of input looked at; longer text is not a code

static const char ALPHABET[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

static uint16_t crc16(const uint8_t *p, size_t n) {  // CRC-16/CCITT-FALSE
    uint16_t crc = 0xffff;
    for (size_t i = 0; i < n; i++) {
        crc ^= (uint16_t)(p[i] << 8);
        for (int k = 0; k < 8; k++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

bool online_invite_encode(int family, const uint8_t *addr, uint16_t port, char *out, size_t cap) {
    if ((family != ONLINE_INVITE_V4 && family != ONLINE_INVITE_V6) || !addr || !out || port == 0)
        return false;
    size_t alen = family == ONLINE_INVITE_V4 ? 4 : 16;
    uint8_t raw[21];
    size_t n = 0;
    raw[n++] = (uint8_t)((VERSION << 4) | family);
    memcpy(raw + n, addr, alen);
    n += alen;
    raw[n++] = (uint8_t)(port >> 8);
    raw[n++] = (uint8_t)port;
    uint16_t crc = crc16(raw, n);
    raw[n++] = (uint8_t)(crc >> 8);
    raw[n++] = (uint8_t)crc;

    size_t symbols = (n * 8 + 4) / 5;
    size_t need = symbols + (symbols - 1) / 4 + 1;
    if (cap < need)
        return false;
    size_t o = 0;
    uint32_t acc = 0;
    int bits = 0;
    size_t emitted = 0, i = 0;
    while (emitted < symbols) {
        if (bits < 5) {
            acc = ((acc << 8) | (i < n ? raw[i] : 0)) & 0xfffffu;  // past the end: zero padding bits
            i++;
            bits += 8;
        }
        bits -= 5;
        if (emitted && emitted % 4 == 0)
            out[o++] = '-';
        out[o++] = ALPHABET[(acc >> bits) & 31];
        emitted++;
    }
    out[o] = 0;
    return true;
}

static int symbol_value(char c) {
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');
    if (c == 'O')
        c = '0';
    else if (c == 'I' || c == 'L')
        c = '1';
    for (int i = 0; i < 32; i++)
        if (ALPHABET[i] == c)
            return i;
    return -1;
}

bool online_invite_decode(const char *text, int *family, uint8_t *addr, uint16_t *port) {
    if (!text || !family || !addr || !port)
        return false;
    uint8_t raw[24];
    size_t symbols = 0, n = 0;
    uint32_t acc = 0;
    int bits = 0;
    for (size_t i = 0; text[i]; i++) {
        if (i >= MAX_SCAN)
            return false;
        char c = text[i];
        if (c == '-' || c == ' ' || c == '\t')
            continue;
        int v = symbol_value(c);
        if (v < 0 || symbols >= 34)
            return false;
        symbols++;
        acc = (acc << 5) | (uint32_t)v;
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            raw[n++] = (uint8_t)(acc >> bits);
        }
        acc &= (1u << bits) - 1;
    }
    if (acc != 0)  // leftover bits are the zero padding the encoder wrote
        return false;
    if (n < 1 || (raw[0] >> 4) != VERSION)
        return false;
    int fam = raw[0] & 15;
    size_t alen = fam == ONLINE_INVITE_V4 ? 4 : fam == ONLINE_INVITE_V6 ? 16 : 0;
    if (!alen || n != 1 + alen + 2 + 2)
        return false;
    size_t body = n - 2;
    if (crc16(raw, body) != (uint16_t)((raw[body] << 8) | raw[body + 1]))
        return false;
    uint16_t p = (uint16_t)((raw[1 + alen] << 8) | raw[2 + alen]);
    if (p == 0)
        return false;
    *family = fam;
    memset(addr, 0, 16);
    memcpy(addr, raw + 1, alen);
    *port = p;
    return true;
}
