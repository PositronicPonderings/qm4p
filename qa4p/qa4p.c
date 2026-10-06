/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qa4p.c
 * @brief   Reading asset packs built by tools/mkpack.py.
 *
 * LAYER:   Assets
 * DEPENDS: qa4p.h, pico-sdk (XIP_BASE, PICO_FLASH_SIZE_BYTES,
 *          __flash_binary_end)
 *
 * The pack format is described at the top of tools/mkpack.py. In short: a
 * 32-byte header, then a table of 48-byte entries sorted by name, then the
 * files' data. Everything is read in place from flash.
 *
 * The library keeps no state of its own: everything it knows about a pack
 * is in the qa_pack_t the program passes in. That's what lets several packs
 * be open at once.
 */
#include <string.h>
#include "qa4p.h"

/*
 * Three facts about the chip, which qa_open() needs for its safety checks:
 *   FLASH_START   the address where flash appears in memory
 *   FLASH_SIZE    how many bytes of flash there are
 *   FIRMWARE_END  the address of the first byte after the firmware
 */
#ifdef QA_HOST_TEST
/* On a PC: the pretend flash chip described at the end of qa4p.h. */
uint8_t  qa_host_flash[QA_HOST_FLASH_SIZE];
uint32_t qa_host_firmware_size = 64u * 1024u;
#define FLASH_START  ((uintptr_t)qa_host_flash)
#define FLASH_SIZE   QA_HOST_FLASH_SIZE
#define FIRMWARE_END (FLASH_START + qa_host_firmware_size)
#else
#include "pico.h"                       /* PICO_FLASH_SIZE_BYTES, from the board's header */
#include "hardware/regs/addressmap.h"   /* XIP_BASE: where flash appears in memory        */
#ifndef PICO_FLASH_SIZE_BYTES
#error "qa4p: this board doesn't say how big its flash is: define PICO_FLASH_SIZE_BYTES"
#endif
/* Defined by the pico-sdk linker script: the first byte after the firmware. */
extern char __flash_binary_end;
#define FLASH_START  ((uintptr_t)XIP_BASE)
#define FLASH_SIZE   ((uint32_t)PICO_FLASH_SIZE_BYTES)
#define FIRMWARE_END ((uintptr_t)&__flash_binary_end)
#endif

#define HEADER_SIZE  32u
#define ENTRY_SIZE   48u
#define PACK_VERSION 1u

/* Numbers in the pack are little-endian; read them a byte at a time so the
 * code doesn't depend on alignment.                                        */
static inline uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }
static inline uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Is this pack open? A NULL pointer, a never-opened (zeroed) pack and one
 * whose qa_open() failed all have no base, so all count as "not open".     */
static inline bool is_open(const qa_pack_t *pack) { return pack != NULL && pack->base != NULL; }

static inline const uint8_t *entry(const qa_pack_t *pack, uint16_t i)
{
    return pack->base + HEADER_SIZE + (uint32_t)i * ENTRY_SIZE;
}

/*
 * The checks shared by both ways of opening a pack. `room` is how many bytes
 * there are from `addr` to the end of the memory the pack lives in: the end
 * of flash for qa_open(), "unlimited" for qa_open_at().
 */
static qa_err_t open_checked(qa_pack_t *pack, const uint8_t *addr, uint32_t room)
{
    /* Erased flash reads as 0xFF, so a missing pack simply fails the magic. */
    if (addr == NULL || memcmp(addr, "QAPK", 4) != 0) return QA_ERR_NO_PACK;
    if (rd16(addr + 4) != PACK_VERSION)               return QA_ERR_VERSION;

    uint32_t count = rd16(addr + 6);
    uint32_t total = rd32(addr + 8);
    if (total < HEADER_SIZE + count * ENTRY_SIZE || total > 0x1000000u) {
        return QA_ERR_CORRUPT;
    }

    /* Does all of it fit? Checked before the table is read, so a pack
     * running off the end of flash is never read past that end.           */
    if (total > room) return QA_ERR_TOO_BIG;

    /* Check every entry points inside the pack and has a terminated name.
     * This is quick (a few hundred bytes of table) and means qa_find()
     * can never hand back a pointer outside the pack.                      */
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *e = addr + HEADER_SIZE + i * ENTRY_SIZE;
        uint32_t ofs = rd32(e + 36), size = rd32(e + 40);
        if (memchr(e, '\0', QA_NAME_MAX + 1) == NULL) return QA_ERR_CORRUPT;
        if (ofs < HEADER_SIZE || ofs > total || size > total - ofs) return QA_ERR_CORRUPT;
    }

    pack->base  = addr;
    pack->count = (uint16_t)count;
    pack->size  = total;
    return QA_OK;
}

/* Start every open from "closed", so a failed open leaves nothing behind. */
static void close_pack(qa_pack_t *pack)
{
    pack->base  = NULL;
    pack->size  = 0;
    pack->count = 0;
}

qa_err_t qa_open_at(qa_pack_t *pack, const uint8_t *addr)
{
    if (pack == NULL) return QA_ERR_NOT_READY;
    close_pack(pack);
    return open_checked(pack, addr, UINT32_MAX);
}

qa_err_t qa_open(qa_pack_t *pack, uint32_t flash_offset)
{
    if (pack == NULL) return QA_ERR_NOT_READY;
    close_pack(pack);

    /* SAFETY CHECK 1: the pack must lie wholly inside the flash chip. Most
     * flash chips wrap round past their end, so reading beyond it quietly
     * reads the start of flash (the firmware) again. Here the offset itself
     * is checked; open_checked() checks the pack's size against what's left. */
    if (flash_offset >= FLASH_SIZE) return QA_ERR_TOO_BIG;

    /* SAFETY CHECK 2: if the firmware has grown past the pack's start,
     * loading the firmware overwrote the start of the pack (or will). Say
     * so plainly rather than reading garbage.                              */
    uintptr_t start = FLASH_START + flash_offset;
    if (FIRMWARE_END > start) return QA_ERR_OVERLAP;

    return open_checked(pack, (const uint8_t *)start, FLASH_SIZE - flash_offset);
}

static void fill(const qa_pack_t *pack, uint16_t i, qa_file_t *out)
{
    const uint8_t *e = entry(pack, i);
    out->name  = (const char *)e;
    out->data  = pack->base + rd32(e + 36);
    out->size  = rd32(e + 40);
    out->type  = (uint16_t)rd16(e + 44);
    out->flags = (uint16_t)rd16(e + 46);
}

/*
 * The table is sorted by name, so we can use binary search: compare with the
 * middle entry, then keep only the half that can contain the name. 100 files
 * take at most 7 comparisons.
 */
qa_err_t qa_find(const qa_pack_t *pack, const char *name, qa_file_t *out)
{
    if (!is_open(pack)) return QA_ERR_NOT_READY;
    if (name == NULL)   return QA_ERR_NOT_FOUND;

    uint32_t lo = 0, hi = pack->count;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        int cmp = strcmp(name, (const char *)entry(pack, (uint16_t)mid));
        if (cmp == 0) {
            if (out) fill(pack, (uint16_t)mid, out);
            return QA_OK;
        }
        if (cmp < 0) hi = mid;
        else         lo = mid + 1;
    }
    return QA_ERR_NOT_FOUND;
}

uint16_t qa_count(const qa_pack_t *pack) { return is_open(pack) ? pack->count : 0; }
uint32_t qa_size(const qa_pack_t *pack)  { return is_open(pack) ? pack->size : 0; }

qa_err_t qa_get(const qa_pack_t *pack, uint16_t i, qa_file_t *out)
{
    if (!is_open(pack))  return QA_ERR_NOT_READY;
    if (i >= pack->count) return QA_ERR_NOT_FOUND;
    if (out) fill(pack, i, out);
    return QA_OK;
}

/*
 * CRC-32: a checksum that changes if any bit of the data changes. mkpack.py
 * stores the CRC of everything after the header; recomputing it here and
 * comparing tells us whether the pack in flash is exactly what was built.
 *
 * This is the standard CRC-32 (the one zip files use). The 256-entry table
 * speeds it up from 8 steps per byte to 1; it's built on first use, and
 * shared by every pack.
 */
qa_err_t qa_verify(const qa_pack_t *pack)
{
    static uint32_t table[256];
    static bool     table_ready = false;

    if (!is_open(pack)) return QA_ERR_NOT_READY;

    if (!table_ready) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[n] = c;
        }
        table_ready = true;
    }

    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = HEADER_SIZE; i < pack->size; i++) {
        crc = table[(crc ^ pack->base[i]) & 0xFFu] ^ (crc >> 8);
    }
    crc ^= 0xFFFFFFFFu;
    return (crc == rd32(pack->base + 12)) ? QA_OK : QA_ERR_CORRUPT;
}

const char *qa_err_str(qa_err_t err)
{
    switch (err) {
    case QA_OK:            return "OK";
    case QA_ERR_NO_PACK:   return "no asset pack in flash";
    case QA_ERR_VERSION:   return "asset pack from a newer mkpack.py";
    case QA_ERR_CORRUPT:   return "asset pack is damaged";
    case QA_ERR_NOT_FOUND: return "not found";
    case QA_ERR_OVERLAP:   return "firmware has grown into the asset pack area";
    case QA_ERR_NOT_READY: return "asset pack not initialised";
    case QA_ERR_TOO_BIG:   return "asset pack runs past the end of flash";
    default:               return "unknown error";
    }
}
