// World identity for co-op: see runtime/coop_world.h. Pure logic, tested by tests/test_coop_world.c.
#include "coop_world.h"

#include <stdio.h>
#include <string.h>

#define FILE_NAME "coop-world.txt"
#define TEMP_NAME "coop-world.txt.tmp"
#define PATH_MAX_LEN 1024

static const char k_hex[] = "0123456789abcdef";

static bool path_of(const char *dir, const char *name, char *out, size_t n) {
    if (!dir || !*dir) return false;
    int w = snprintf(out, n, "%s/%s", dir, name);
    return w > 0 && (size_t)w < n;
}

static int hex_val(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool coop_world_load(const char *dir, uint8_t id[COOP_WORLD_ID_SIZE]) {
    char path[PATH_MAX_LEN];
    if (!id || !path_of(dir, FILE_NAME, path, sizeof path)) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char buf[2 * COOP_WORLD_ID_SIZE + 8];
    size_t n = fread(buf, 1, sizeof buf, f);
    bool err = ferror(f) != 0;
    fclose(f);
    if (err || n < 2 * COOP_WORLD_ID_SIZE || n >= sizeof buf) return false;  // (short, or more than digits and a newline)
    uint8_t out[COOP_WORLD_ID_SIZE];
    for (int i = 0; i < COOP_WORLD_ID_SIZE; i++) {
        int hi = hex_val((unsigned char)buf[2 * i]), lo = hex_val((unsigned char)buf[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)(hi << 4 | lo);
    }
    for (size_t i = 2 * COOP_WORLD_ID_SIZE; i < n; i++)
        if (buf[i] != '\n' && buf[i] != '\r' && buf[i] != ' ' && buf[i] != '\t') return false;
    memcpy(id, out, sizeof out);
    return true;
}

bool coop_world_create(const char *dir, const uint8_t rnd[COOP_WORLD_ID_SIZE], uint8_t id[COOP_WORLD_ID_SIZE]) {
    char path[PATH_MAX_LEN], tmp[PATH_MAX_LEN];
    if (!rnd || !path_of(dir, FILE_NAME, path, sizeof path) || !path_of(dir, TEMP_NAME, tmp, sizeof tmp)) return false;
    char text[2 * COOP_WORLD_ID_SIZE + 1];
    for (int i = 0; i < COOP_WORLD_ID_SIZE; i++) text[2 * i] = k_hex[rnd[i] >> 4], text[2 * i + 1] = k_hex[rnd[i] & 15];
    text[2 * COOP_WORLD_ID_SIZE] = '\n';
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fwrite(text, 1, sizeof text, f) == sizeof text;
    ok = fflush(f) == 0 && ok;
    ok = fclose(f) == 0 && ok;
    // (rename does not replace a file on Windows: remove the old one and try again)
    if (ok && rename(tmp, path) != 0) ok = remove(path) == 0 && rename(tmp, path) == 0;
    if (!ok) {
        remove(tmp);
        return false;
    }
    if (id) memcpy(id, rnd, COOP_WORLD_ID_SIZE);
    return true;
}

void coop_world_profile_name(const uint8_t id[COOP_WORLD_ID_SIZE], char out[COOP_WORLD_NAME_SIZE]) {
    if (!id) {
        out[0] = 0;
        return;
    }
    memcpy(out, "coop-", 5);
    for (int i = 0; i < 8; i++) out[5 + 2 * i] = k_hex[id[i] >> 4], out[6 + 2 * i] = k_hex[id[i] & 15];
    out[5 + 16] = 0;
}

bool coop_world_valid_name(const char *s) {
    if (!s) return false;
    size_t n = 0;
    for (; s[n]; n++) {
        char c = s[n];
        if (n >= COOP_WORLD_NAME_MAX) return false;
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    }
    return n > 0;
}
