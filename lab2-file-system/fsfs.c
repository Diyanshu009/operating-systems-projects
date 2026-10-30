#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>

#define BLKSZ 256
#define MAGIC 0x46534653u // 'FSFS'
#define NONE  0xFFFFFFFFu

static unsigned char *disk = NULL;
static uint32_t NBLOCKS = 0;
static int loaded = 0;

static void die(const char *msg) {
    printf("ERROR: %s\n", msg);
    exit(1);
}

static unsigned char *blk(uint32_t b) {
    return disk + (size_t)b * BLKSZ;
}

static uint32_t ceildiv(uint32_t a, uint32_t b) {
    return (a + b - 1) / b;
}

static void trim(char *s) {
    int n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = 0;
        n--;
    }
}

static int valid_name(const char *s) {
    if (!s || !*s) return 0;
    if (strlen(s) > 55) return 0; // 56 max incl null
    for (const char *p = s; *p; p++) {
        if (isspace((unsigned char)*p)) return 0;
    }
    return 1;
}

// On-disk structs
typedef struct {
    uint32_t magic;
    uint32_t nblocks;
    uint32_t formatted;
    uint32_t bitmap_start;
    uint32_t bitmap_blocks;
    uint32_t fnt_start;
    uint32_t fnt_blocks;
    uint32_t fnt_entries;
    uint32_t dabpt_start;
    uint32_t dabpt_blocks;
    uint32_t dabpt_entries;
    uint32_t bpt_start;
    uint32_t bpt_blocks;
    uint32_t bpt_entries;
    uint32_t data_start;
    uint32_t data_blocks;
    char user[40];
    unsigned char pad[BLKSZ - (4 * 15) - 40];
} Super;

typedef struct {
    char name[56];        // empty => free
    uint32_t dabpt_idx;   // inode pointer
    unsigned char pad[64 - 56 - 4];
} FNT; // 64 bytes => 4 per block

typedef struct {
    uint32_t size;        // bytes
    uint32_t links;       // link count
    uint32_t head_bpt;    // first BPT entry index
    uint64_t mtime;       // last write time
    char owner[40];       // username
    unsigned char pad[64 - 4 - 4 - 4 - 8 - 40];
} DABPT; // 64 bytes => 4 per block

typedef struct {
    uint32_t p[8];        // p[0..6] data blocks; p[7] = next BPT index or NONE
} BPT; // 32 bytes => 8 per block

static Super *SB(void) {
    return (Super *)blk(0);
}

// Bitmap (1 bit per block)
static unsigned char *bitmap_base(void) {
    return blk(SB()->bitmap_start);
}

static uint32_t bitmap_bytes(void) {
    return SB()->bitmap_blocks * BLKSZ;
}

static int bm_get(uint32_t b) {
    uint32_t byte = b / 8;
    uint32_t bit  = b % 8;
    if (byte >= bitmap_bytes()) return 1;
    return (bitmap_base()[byte] >> bit) & 1;
}

static void bm_set(uint32_t b, int used) {
    uint32_t byte = b / 8;
    uint32_t bit  = b % 8;
    if (byte >= bitmap_bytes()) die("bitmap overflow");
    if (used) bitmap_base()[byte] |= (1u << bit);
    else      bitmap_base()[byte] &= ~(1u << bit);
}

static uint32_t alloc_data_block(void) {
    uint32_t start = SB()->data_start;
    uint32_t end   = SB()->data_start + SB()->data_blocks;
    for (uint32_t b = start; b < end; b++) {
        if (!bm_get(b)) {
            bm_set(b, 1);
            memset(blk(b), 0, BLKSZ);
            return b;
        }
    }
    return NONE;
}

static void free_data_block(uint32_t b) {
    if (b == NONE) return;
    bm_set(b, 0);
}

// Table access
static FNT *fnt_at(uint32_t i) {
    uint32_t per = BLKSZ / sizeof(FNT); // 4
    uint32_t b   = SB()->fnt_start + i / per;
    uint32_t o   = i % per;
    return &((FNT *)blk(b))[o];
}

static DABPT *dabpt_at(uint32_t i) {
    uint32_t per = BLKSZ / sizeof(DABPT); // 4
    uint32_t b   = SB()->dabpt_start + i / per;
    uint32_t o   = i % per;
    return &((DABPT *)blk(b))[o];
}

static BPT *bpt_at(uint32_t i) {
    uint32_t per = BLKSZ / sizeof(BPT); // 8
    uint32_t b   = SB()->bpt_start + i / per;
    uint32_t o   = i % per;
    return &((BPT *)blk(b))[o];
}

// Find helpers
static int find_fnt(const char *name, uint32_t *out) {
    for (uint32_t i = 0; i < SB()->fnt_entries; i++) {
        FNT *e = fnt_at(i);
        if (e->name[0] && strcmp(e->name, name) == 0) {
            if (out) *out = i;
            return 1;
        }
    }
    return 0;
}

static int find_free_fnt(uint32_t *out) {
    for (uint32_t i = 0; i < SB()->fnt_entries; i++) {
        if (fnt_at(i)->name[0] == 0) {
            *out = i;
            return 1;
        }
    }
    return 0;
}

static int find_free_dabpt(uint32_t *out) {
    for (uint32_t i = 0; i < SB()->dabpt_entries; i++) {
        if (dabpt_at(i)->links == 0) {
            *out = i;
            return 1;
        }
    }
    return 0;
}

// Free BPT entry is all zeros (never in-use)
// In-use BPT always has p[7] set to NONE initially (end-of-chain sentinel)
static int find_free_bpt(uint32_t *out) {
    for (uint32_t i = 0; i < SB()->bpt_entries; i++) {
        BPT *b = bpt_at(i);
        int all0 = 1;
        for (int k = 0; k < 8; k++) {
            if (b->p[k] != 0) {
                all0 = 0;
                break;
            }
        }
        if (all0) {
            *out = i;
            return 1;
        }
    }
    return 0;
}

static void bpt_init_inuse(uint32_t idx) {
    BPT *b = bpt_at(idx);
    memset(b, 0, sizeof(BPT));
    b->p[7] = NONE; // important: end of chain sentinel
}

static void bpt_clear(uint32_t idx) {
    memset(bpt_at(idx), 0, sizeof(BPT));
}

// Load/format checks
static void need_loaded(void) {
    if (!loaded || !disk) die("No FS loaded. Use Createfs/Openfs first.");
}

static void need_formatted(void) {
    need_loaded();
    if (SB()->magic != MAGIC) die("Bad magic. Not a valid FS.");
    if (!SB()->formatted) die("Not formatted. Run Formatfs.");
}

// Commands
static void cmd_Createfs(uint32_t n) {
    if (n < 20) die("Use >= 20 blocks.");
    if (disk) free(disk);
    NBLOCKS = n;
    disk = (unsigned char *)malloc((size_t)NBLOCKS * BLKSZ);
    if (!disk) die("malloc failed");
    memset(disk, 0, (size_t)NBLOCKS * BLKSZ);
    loaded = 1;
    SB()->magic = MAGIC;
    SB()->nblocks = NBLOCKS;
    SB()->formatted = 0;
    strcpy(SB()->user, "default");
    printf("OK: Created FS with %u blocks\n", NBLOCKS);
}

static void cmd_Formatfs(uint32_t fnt_entries, uint32_t dabpt_entries) {
    need_loaded();
    if (fnt_entries == 0 || dabpt_entries == 0) die("Formatfs args must be > 0");

    uint32_t fnt_blocks   = ceildiv(fnt_entries, 4);
    uint32_t dabpt_blocks = ceildiv(dabpt_entries, 4);

    uint32_t want_bpt_entries = dabpt_entries * 2;
    if (want_bpt_entries < 64) want_bpt_entries = 64;

    uint32_t bpt_blocks  = ceildiv(want_bpt_entries, 8);
    uint32_t bpt_entries = bpt_blocks * 8;

    uint32_t bitmap_blocks = ceildiv(SB()->nblocks, BLKSZ * 8);

    uint32_t cur = 1;
    uint32_t bitmap_start = cur; cur += bitmap_blocks;
    uint32_t fnt_start    = cur; cur += fnt_blocks;
    uint32_t dabpt_start  = cur; cur += dabpt_blocks;
    uint32_t bpt_start    = cur; cur += bpt_blocks;
    uint32_t data_start   = cur;

    if (data_start >= SB()->nblocks) die("Not enough blocks for metadata");

    uint32_t nblocks = SB()->nblocks;

    char saved_user[40];
    strncpy(saved_user, SB()->user, 39);
    saved_user[39] = 0;

    memset(disk, 0, (size_t)nblocks * BLKSZ);

    SB()->magic = MAGIC;
    SB()->nblocks = nblocks;
    SB()->formatted = 1;

    SB()->bitmap_start = bitmap_start;
    SB()->bitmap_blocks = bitmap_blocks;

    SB()->fnt_start = fnt_start;
    SB()->fnt_blocks = fnt_blocks;
    SB()->fnt_entries = fnt_entries;

    SB()->dabpt_start = dabpt_start;
    SB()->dabpt_blocks = dabpt_blocks;
    SB()->dabpt_entries = dabpt_entries;

    SB()->bpt_start = bpt_start;
    SB()->bpt_blocks = bpt_blocks;
    SB()->bpt_entries = bpt_entries;

    SB()->data_start = data_start;
    SB()->data_blocks = SB()->nblocks - data_start;

    if (saved_user[0]) strcpy(SB()->user, saved_user);
    else strcpy(SB()->user, "default");

    // mark metadata blocks used in bitmap
    for (uint32_t b = 0; b < data_start; b++) bm_set(b, 1);

    printf("OK: Formatted FS\n");
}

static void cmd_Savefs(const char *name) {
    need_loaded();
    FILE *f = fopen(name, "wb");
    if (!f) die("Savefs fopen failed");
    fwrite(disk, BLKSZ, NBLOCKS, f);
    fclose(f);
    printf("OK: Saved '%s'\n", name);
}

static void cmd_Openfs(const char *name) {
    FILE *f = fopen(name, "rb");
    if (!f) die("Openfs fopen failed");

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || (sz % BLKSZ) != 0) die("Openfs invalid file size");

    uint32_t nblocks = (uint32_t)(sz / BLKSZ);

    if (disk) free(disk);
    NBLOCKS = nblocks;
    disk = (unsigned char *)malloc((size_t)NBLOCKS * BLKSZ);
    if (!disk) die("malloc failed");

    fread(disk, BLKSZ, NBLOCKS, f);
    fclose(f);

    loaded = 1;

    if (SB()->magic != MAGIC) die("Not a valid FS image (magic mismatch)");

    printf("OK: Opened '%s'\n", name);
}

static void cmd_User(const char *u) {
    need_formatted();
    if (!u || !*u) die("User empty");
    if (strlen(u) >= sizeof(SB()->user)) die("User too long");
    memset(SB()->user, 0, sizeof(SB()->user));
    strcpy(SB()->user, u);
    printf("OK: user='%s'\n", SB()->user);
}

static void cmd_List(void) {
    need_formatted();
    printf("NAME SIZE OWNER MTIME\n");
    printf("--------------------------------------------------------------------------\n");
    for (uint32_t i = 0; i < SB()->fnt_entries; i++) {
        FNT *e = fnt_at(i);
        if (!e->name[0]) continue;
        if (e->dabpt_idx >= SB()->dabpt_entries) continue;

        DABPT *d = dabpt_at(e->dabpt_idx);
        if (d->links == 0) continue;

        // print mtime as seconds (spec says secs); readable format optional
        printf("%-20s %6u %-30s %llu\n",
               e->name, d->size, d->owner, (unsigned long long)d->mtime);
    }
}

static void free_all_blocks_for_file(uint32_t dabpt_idx) {
    DABPT *d = dabpt_at(dabpt_idx);
    uint32_t cur = d->head_bpt;
    while (cur != NONE) {
        BPT *b = bpt_at(cur);
        for (int k = 0; k < 7; k++) {
            uint32_t db = b->p[k];
            if (db != 0 && db != NONE) {
                if (db >= SB()->data_start &&
                    db < SB()->data_start + SB()->data_blocks) {
                    free_data_block(db);
                }
            }
        }
        uint32_t next = b->p[7]; // this is NONE at end of chain
        bpt_clear(cur);
        cur = next;
    }
    d->head_bpt = NONE;
}

static void cmd_Remove(const char *name) {
    need_formatted();
    uint32_t idx;
    if (!find_fnt(name, &idx)) die("Remove: file not found");

    FNT *e = fnt_at(idx);
    uint32_t didx = e->dabpt_idx;
    if (didx >= SB()->dabpt_entries) die("Remove: corrupted dabpt index");

    DABPT *d = dabpt_at(didx);
    if (d->links == 0) die("Remove: corrupted file");

    memset(e, 0, sizeof(FNT)); // remove directory entry

    d->links--;
    if (d->links == 0) {
        free_all_blocks_for_file(didx);
        memset(d, 0, sizeof(DABPT));
    }
    printf("OK: Removed '%s'\n", name);
}

static void cmd_Unlink(const char *name) {
    cmd_Remove(name);
}

static void cmd_Rename(const char *oldn, const char *newn) {
    need_formatted();
    if (!valid_name(newn)) die("Rename: invalid new name");
    if (find_fnt(newn, NULL)) die("Rename: new name exists");

    uint32_t idx;
    if (!find_fnt(oldn, &idx)) die("Rename: old not found");

    FNT *e = fnt_at(idx);
    memset(e->name, 0, sizeof(e->name));
    strncpy(e->name, newn, sizeof(e->name) - 1);

    printf("OK: Renamed '%s' -> '%s'\n", oldn, newn);
}

static void cmd_Link(const char *exist, const char *newn) {
    need_formatted();
    if (!valid_name(newn)) die("Link: invalid new name");
    if (find_fnt(newn, NULL)) die("Link: new name exists");

    uint32_t src;
    if (!find_fnt(exist, &src)) die("Link: file not found");

    uint32_t freei;
    if (!find_free_fnt(&freei)) die("Link: no free FNT entries");

    FNT *s = fnt_at(src);
    if (s->dabpt_idx >= SB()->dabpt_entries) die("Link: corrupted inode");

    DABPT *d = dabpt_at(s->dabpt_idx);
    if (d->links == 0) die("Link: corrupted file");

    FNT *e = fnt_at(freei);
    memset(e, 0, sizeof(FNT));
    strncpy(e->name, newn, sizeof(e->name) - 1);
    e->dabpt_idx = s->dabpt_idx;

    d->links++;

    printf("OK: Linked '%s' -> '%s'\n", exist, newn);
}

static void cmd_Put(const char *hostfile) {
    need_formatted();
    if (!valid_name(hostfile)) die("Put: invalid filename (no spaces, <=55 chars)");
    if (find_fnt(hostfile, NULL)) die("Put: name already exists in FS");

    FILE *f = fopen(hostfile, "rb");
    if (!f) die("Put: cannot open host file");

    fseek(f, 0, SEEK_END);
    long szL = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (szL < 0) {
        fclose(f);
        die("Put: size error");
    }

    uint32_t sz = (uint32_t)szL;

    uint32_t fidx, didx, bhead;
    if (!find_free_fnt(&fidx)) {
        fclose(f);
        die("Put: no free FNT slots");
    }
    if (!find_free_dabpt(&didx)) {
        fclose(f);
        die("Put: no free DABPT slots");
    }

    // if file is empty, we can avoid BPT allocation entirely
    if (sz == 0) {
        DABPT *d = dabpt_at(didx);
        memset(d, 0, sizeof(DABPT));
        d->size = 0;
        d->links = 1;
        d->head_bpt = NONE;
        d->mtime = (uint64_t)time(NULL);
        strncpy(d->owner, SB()->user, sizeof(d->owner) - 1);

        FNT *e = fnt_at(fidx);
        memset(e, 0, sizeof(FNT));
        strncpy(e->name, hostfile, sizeof(e->name) - 1);
        e->dabpt_idx = didx;

        fclose(f);
        printf("OK: Put '%s' (0 bytes)\n", hostfile);
        return;
    }

    if (!find_free_bpt(&bhead)) {
        fclose(f);
        die("Put: no free BPT slots");
    }
    bpt_init_inuse(bhead);

    uint32_t remaining = sz;
    uint32_t cur_bpt = bhead;
    int pos = 0;

    while (remaining > 0) {
        if (pos == 7) {
            uint32_t nextb;
            if (!find_free_bpt(&nextb)) {
                fclose(f);
                die("Put: out of BPT entries");
            }
            bpt_at(cur_bpt)->p[7] = nextb; // chain
            cur_bpt = nextb;
            bpt_init_inuse(cur_bpt);
            pos = 0;
        }

        uint32_t db = alloc_data_block();
        if (db == NONE) {
            fclose(f);
            die("Put: out of data space");
        }
        bpt_at(cur_bpt)->p[pos++] = db;

        uint32_t chunk = (remaining > BLKSZ) ? BLKSZ : remaining;
        size_t r = fread(blk(db), 1, chunk, f);
        if (r != chunk) {
            fclose(f);
            die("Put: read error");
        }
        remaining -= chunk;
    }

    fclose(f);

    DABPT *d = dabpt_at(didx);
    memset(d, 0, sizeof(DABPT));
    d->size = sz;
    d->links = 1;
    d->head_bpt = bhead;
    d->mtime = (uint64_t)time(NULL);
    strncpy(d->owner, SB()->user, sizeof(d->owner) - 1);

    FNT *e = fnt_at(fidx);
    memset(e, 0, sizeof(FNT));
    strncpy(e->name, hostfile, sizeof(e->name) - 1);
    e->dabpt_idx = didx;

    printf("OK: Put '%s' (%u bytes)\n", hostfile, sz);
}

static void cmd_Get(const char *name) {
    need_formatted();
    uint32_t idx;
    if (!find_fnt(name, &idx)) die("Get: file not found");

    FNT *e = fnt_at(idx);
    if (e->dabpt_idx >= SB()->dabpt_entries) die("Get: corrupted inode");

    DABPT *d = dabpt_at(e->dabpt_idx);
    if (d->links == 0) die("Get: corrupted file");

    FILE *f = fopen(name, "wb");
    if (!f) die("Get: cannot create host output file");

    uint32_t remaining = d->size;
    uint32_t cur = d->head_bpt;

    // empty file
    if (remaining == 0) {
        fclose(f);
        printf("OK: Get '%s'\n", name);
        return;
    }

    while (remaining > 0 && cur != NONE) {
        BPT *b = bpt_at(cur);
        for (int k = 0; k < 7 && remaining > 0; k++) {
            uint32_t db = b->p[k];
            if (db == 0) continue;

            uint32_t chunk = (remaining > BLKSZ) ? BLKSZ : remaining;
            fwrite(blk(db), 1, chunk, f);
            remaining -= chunk;
        }
        cur = b->p[7]; // NONE ends chain
    }

    fclose(f);

    if (remaining != 0) die("Get: chain ended early (corrupt FS)");
    printf("OK: Get '%s'\n", name);
}

// Parsing
static int split(char *line, char *a[], int max) {
    int n = 0;
    char *p = line;
    while (*p && n < max) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        a[n++] = p;

        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) {
            *p = 0;
            p++;
        }
    }
    return n;
}

int main(void) {
    char line[512];
    printf("Tiny FS\n");
    while (1) {
        printf("FS> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        trim(line);
        if (!line[0]) continue;

        char *av[5] = {0};
        int ac = split(line, av, 5);
        if (ac == 0) continue;

        if (!strcasecmp(av[0], "Quit") || !strcasecmp(av[0], "Exit")) {
            break;
        } else if (!strcasecmp(av[0], "Createfs")) {
            if (ac != 2) die("Usage: Createfs <#blocks>");
            cmd_Createfs((uint32_t)strtoul(av[1], NULL, 10));
        } else if (!strcasecmp(av[0], "Formatfs")) {
            if (ac != 3) die("Usage: Formatfs <#filenames> <#DABPTentries>");
            cmd_Formatfs((uint32_t)strtoul(av[1], NULL, 10),
                         (uint32_t)strtoul(av[2], NULL, 10));
        } else if (!strcasecmp(av[0], "Savefs")) {
            if (ac != 2) die("Usage: Savefs <name>");
            cmd_Savefs(av[1]);
        } else if (!strcasecmp(av[0], "Openfs")) {
            if (ac != 2) die("Usage: Openfs <name>");
            cmd_Openfs(av[1]);
        } else if (!strcasecmp(av[0], "List")) {
            cmd_List();
        } else if (!strcasecmp(av[0], "User")) {
            if (ac != 2) die("Usage: User <name>");
            cmd_User(av[1]);
        } else if (!strcasecmp(av[0], "Put")) {
            if (ac != 2) die("Usage: Put <ExternalFile>");
            cmd_Put(av[1]);
        } else if (!strcasecmp(av[0], "Get")) {
            if (ac != 2) die("Usage: Get <ExternalFile>");
            cmd_Get(av[1]);
        } else if (!strcasecmp(av[0], "Remove")) {
            if (ac != 2) die("Usage: Remove <name>");
            cmd_Remove(av[1]);
        } else if (!strcasecmp(av[0], "Rename")) {
            if (ac != 3) die("Usage: Rename <oldname> <newname>");
            cmd_Rename(av[1], av[2]);
        } else if (!strcasecmp(av[0], "Link")) {
            if (ac != 3) die("Usage: Link <existingname> <newlinkname>");
            cmd_Link(av[1], av[2]);
        } else if (!strcasecmp(av[0], "Unlink")) {
            if (ac != 2) die("Usage: Unlink <name>");
            cmd_Unlink(av[1]);
        } else {
            printf("Unknown command.\n");
        }
    }

    if (disk) free(disk);
    return 0;
}