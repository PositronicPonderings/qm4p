/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qa4p.h
 * @brief   QA4P - QuickAssets 4 Pico: read-only asset packs in flash; find
 *          files by name.
 *
 * LAYER:   Assets (a library of its own; it knows nothing about graphics
 *          or sound, and they know nothing about it)
 * DEPENDS: pico-sdk (for the flash address and size); nothing else
 *
 * ---------------------------------------------------------------------------
 *  THE IDEA
 * ---------------------------------------------------------------------------
 *  Images, sounds and game data live in "packs", built on the PC by
 *  tools/mkpack.py and loaded onto the Pico separately from the firmware.
 *  Each pack sits in its own area of flash, at an offset you choose:
 *
 *      0x10000000  +------------------------+  offset 0
 *                  |  firmware              |   the .uf2 from your build
 *                  |  (here, up to 1 MB)    |
 *      0x10100000  +------------------------+  offset 0x100000 = QA_DEFAULT_OFFSET
 *                  |  first pack: graphics  |   e.g. art.uf2 from mkpack.py
 *                  |  (here, up to 1.5 MB)  |
 *      0x10280000  +------------------------+  offset 0x280000
 *                  |  second pack: sound    |   e.g. sound.uf2 from mkpack.py
 *                  |  (here, up to 1.5 MB)  |
 *      0x10400000  +------------------------+  end of a 4 MB flash (Pico 2)
 *
 *  The offsets are only examples: put packs wherever suits your program,
 *  and tell mkpack.py the same offset with --offset. docs/RESOURCES.md has
 *  the plan the QM4P libraries follow.
 *
 *  Flash is "memory-mapped": its contents can be read through a pointer like
 *  any array. So qa_find() just hands back a pointer into flash, and a file
 *  is used in place, with no copying and no RAM.
 *
 *  Updating the firmware leaves the packs alone, and vice versa: changing the
 *  artwork means rebuilding and reloading only that pack.
 *
 * ---------------------------------------------------------------------------
 *  PACKS ARE HANDLES
 * ---------------------------------------------------------------------------
 *  The library keeps nothing of its own. Each open pack is a small
 *  qa_pack_t (12 bytes) that YOUR program owns, so several packs can be
 *  open at once: one for graphics, one for sound, one per game level...
 *  Every function takes the pack to look in.
 *
 *  A qa_pack_t that hasn't been opened must start as all zeros. Declaring it
 *  static or global does that for you (as below); for a local variable,
 *  write  qa_pack_t pack = {0};
 *
 * ---------------------------------------------------------------------------
 *  USE
 * ---------------------------------------------------------------------------
 *      static qa_pack_t art;
 *      if (qa_open(&art, QA_DEFAULT_OFFSET) != QA_OK) { ...no pack: tell the user... }
 *
 *      qa_file_t f;
 *      if (qa_find(&art, "icons/star.bmp", &f) == QA_OK) {
 *          ...f.data and f.size are the file's bytes, in flash...
 *      }
 *
 *  With QG4P, an image file opens straight from flash:
 *      qg_image_open(&img, f.data, f.size,
 *                    (f.flags & QA_FLAG_TRANSPARENT) ? QG_IMAGE_TRANSPARENT : 0);
 */
#ifndef QA4P_H
#define QA4P_H

#include <stdint.h>
#include <stdbool.h>

/**
 * A convenient place for a first pack: 1 MB into flash (0x10100000), which
 * leaves the firmware the first megabyte. Nothing in the library assumes
 * it: pass any offset to qa_open(), matching mkpack.py's --offset.
 */
#ifndef QA_DEFAULT_OFFSET
#define QA_DEFAULT_OFFSET 0x100000u
#endif

/** Longest name, in characters (the table stores 36 bytes with the NUL). */
#define QA_NAME_MAX 35

/** File types, set by mkpack.py from the file extension. */
enum {
    QA_TYPE_OTHER = 0,
    QA_TYPE_IMAGE = 1,     /* .bmp (or an image converted to .bmp) */
    QA_TYPE_SOUND = 2,     /* .wav, .raw                           */
    QA_TYPE_TEXT  = 3,     /* .txt, .json, .csv, .ini              */
};

/** Flags, set by mkpack.py. */
#define QA_FLAG_TRANSPARENT 0x0001   /* image with see-through pixels (QG4P: open with QG_IMAGE_TRANSPARENT) */

/**
 * One open pack. Fill it with qa_open(); after that, just pass it to the
 * other functions. The fields are for the library: read them if you're
 * curious, but don't change them.
 */
typedef struct {
    const uint8_t *base;     /**< the pack's first byte; NULL until a successful open */
    uint32_t       size;     /**< total size in bytes                                 */
    uint16_t       count;    /**< number of files                                     */
} qa_pack_t;

/** One file in a pack. `data` points straight into flash. */
typedef struct {
    const char    *name;     /**< e.g. "icons/star.bmp" (in flash)        */
    const uint8_t *data;     /**< the file's bytes (in flash)             */
    uint32_t       size;     /**< in bytes                                */
    uint16_t       type;     /**< QA_TYPE_*                               */
    uint16_t       flags;    /**< QA_FLAG_*                               */
} qa_file_t;

typedef enum {
    QA_OK            =  0,
    QA_ERR_NO_PACK   = -1,  /**< no pack there (never loaded, or erased)        */
    QA_ERR_VERSION   = -2,  /**< a pack from a newer mkpack.py                  */
    QA_ERR_CORRUPT   = -3,  /**< damaged pack (bad sizes, or CRC mismatch)      */
    QA_ERR_NOT_FOUND = -4,  /**< no file with that name                         */
    QA_ERR_OVERLAP   = -5,  /**< the firmware has grown into the pack's area    */
    QA_ERR_NOT_READY = -6,  /**< the pack isn't open (qa_open() hasn't succeeded) */
    QA_ERR_TOO_BIG   = -7,  /**< the pack runs past the end of flash            */
} qa_err_t;

/**
 * Open the pack at `flash_offset` bytes into flash (QA_DEFAULT_OFFSET for
 * the usual first pack). Quick: it reads only the header and the table of
 * contents. Call once per pack at start-up.
 *
 * Besides checking the pack itself, it checks the pack's place in flash:
 * QA_ERR_OVERLAP if the firmware reaches into it, QA_ERR_TOO_BIG if it runs
 * past the end of the flash chip. On any error, `pack` is left closed.
 */
qa_err_t qa_open(qa_pack_t *pack, uint32_t flash_offset);

/**
 * Like qa_open(), for a pack at any address: in RAM, or compiled into the
 * program as an array. It makes no flash checks, since the pack may not be
 * in flash at all.
 */
qa_err_t qa_open_at(qa_pack_t *pack, const uint8_t *addr);

/** Look up a file by its exact name (case matters). */
qa_err_t qa_find(const qa_pack_t *pack, const char *name, qa_file_t *out);

/** Number of files in the pack (0 if it isn't open). */
uint16_t qa_count(const qa_pack_t *pack);

/** File number i (0 .. qa_count()-1), in name order: for listings. */
qa_err_t qa_get(const qa_pack_t *pack, uint16_t i, qa_file_t *out);

/** Total pack size in bytes (0 if it isn't open). */
uint32_t qa_size(const qa_pack_t *pack);

/**
 * Check the whole pack against its CRC-32 checksum, to detect a damaged or
 * half-written pack. It reads every byte of the pack from flash, which is
 * the slow part: measured at about 4.7 KB per millisecond on a Pico 2
 * (16.8 KB in 3.6 ms), so roughly 0.2 s per MB, or 0.65 s for a 3 MB
 * pack. Call it once at start-up if you want it, never before lookups.
 * It's the only function that needs the 1 KB checksum table, so programs
 * that never call it don't carry the table.
 */
qa_err_t qa_verify(const qa_pack_t *pack);

/** A short English description of an error, for messages. */
const char *qa_err_str(qa_err_t err);

/* ------------------------------------------------------------------------- */
/*  Host tests only                                                          */
/* ------------------------------------------------------------------------- */
#ifdef QA_HOST_TEST
/*
 * On a PC there is no flash, so the tests in tests/host build qa4p.c with
 * QA_HOST_TEST defined and get a pretend flash chip instead: an array of
 * QA_HOST_FLASH_SIZE bytes (all zeros, so it holds no pack until a test
 * copies one in), whose first qa_host_firmware_size bytes count as the
 * firmware. qa_open() reads and checks it exactly as it would real flash.
 */
#define QA_HOST_FLASH_SIZE (4u * 1024u * 1024u)    /* a stub: a Pico 2's 4 MB */
extern uint8_t  qa_host_flash[QA_HOST_FLASH_SIZE];
extern uint32_t qa_host_firmware_size;
#endif

#endif /* QA4P_H */
