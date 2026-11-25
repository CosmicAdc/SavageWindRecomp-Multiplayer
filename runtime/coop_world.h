// World identity for co-op (runtime/coop_world.c): a 16-byte id that names the world of the player who created it,
// kept in <dir>/coop-world.txt, and the name of the guest's co-op save profile that goes with it (another host is
// another world: ADR-011, decision 29, so no profile is ever carried from one host to another). Pure logic: no game
// memory, no network and no random numbers of its own (the caller gives the random bytes), so that
// tests/test_coop_world.c can test it alone. The id and the name are file and folder names: whatever comes from the
// network goes through coop_world_valid_name before it forms a path.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COOP_WORLD_ID_SIZE 16    // bytes of an id
#define COOP_WORLD_NAME_SIZE 32  // room for a profile name, with its terminator
#define COOP_WORLD_NAME_MAX 31   // the longest valid profile name

// Reads <dir>/coop-world.txt: exactly 32 hexadecimal digits (either case), then at most whitespace. id is written
// only on success. false: no dir, no file, or a file that is not that (corrupt, short, long, other characters).
bool coop_world_load(const char *dir, uint8_t id[COOP_WORLD_ID_SIZE]);

// Writes rnd as the world id into <dir>/coop-world.txt (32 lowercase hexadecimal digits and a newline), through a
// temporary file that is then renamed, so a reader never sees half a file; an existing file is replaced. id (if not
// NULL) gets the id. The directory must exist. false: it could not be written (nothing is left behind).
bool coop_world_create(const char *dir, const uint8_t rnd[COOP_WORLD_ID_SIZE], uint8_t id[COOP_WORLD_ID_SIZE]);

// The profile name of a world: "coop-" and the first 8 bytes of the id in lowercase hexadecimal (21 characters).
// Always valid for coop_world_valid_name. out is "" if id is NULL.
void coop_world_profile_name(const uint8_t id[COOP_WORLD_ID_SIZE], char out[COOP_WORLD_NAME_SIZE]);

// Whether s may be a profile name: 1 to 31 characters, each one of a-z, 0-9 and '-'. NULL: false.
bool coop_world_valid_name(const char *s);

#ifdef __cplusplus
}
#endif
