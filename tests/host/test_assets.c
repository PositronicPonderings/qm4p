/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/*
 * The asset library (qa4p), on a PC. Built with QA_HOST_TEST, so qa_open()
 * reads a pretend 4 MB flash chip (qa_host_flash, see qa4p.h) instead of
 * real flash. Run from tests/host/build, where run_tests.sh puts the packs:
 *   pack/assets.bin         the hardware test pack (tests/hardware/pack)
 *   packs/one.bin, two.bin  two small packs (tests/host/packs/one and two)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qa4p.h"
#include "qg_image.h"
#include "demo_images.h"
static uint8_t *load(const char *p, long *n){ FILE*f=fopen(p,"rb"); if(!f){printf("FAIL  can't open %s\n",p); exit(1);} fseek(f,0,SEEK_END); *n=ftell(f); fseek(f,0,SEEK_SET); uint8_t*b=malloc(*n); if(fread(b,1,*n,f)!=(size_t)*n) exit(1); fclose(f); return b; }
static int fails=0;
#define CHECK(c,msg) do{ if(c) printf("PASS  %s\n",msg); else {printf("FAIL  %s\n",msg); fails++;} }while(0)

/* Every function refuses a pack that isn't open: NOT_READY, or 0 for the counts. */
static int all_refuse(const qa_pack_t *p){
  qa_file_t a;
  return qa_find(p,"dice/d20.bmp",&a)==QA_ERR_NOT_READY && qa_get(p,0,&a)==QA_ERR_NOT_READY
      && qa_count(p)==0 && qa_size(p)==0 && qa_verify(p)==QA_ERR_NOT_READY; }

/* The text of a file, as a string (pack files have no '\0' of their own). */
static const char *text_of(const qa_pack_t *p, const char *name){
  static char buf[64]; qa_file_t a;
  if(qa_find(p,name,&a)!=QA_OK || a.size>=sizeof buf) return "";
  memcpy(buf,a.data,a.size); buf[a.size]='\0'; return buf; }

int main(void){
  long n; uint8_t *pk = load("pack/assets.bin",&n);
  qa_pack_t pack = {0};
  qa_file_t a;

  /* ---- not open ---- */
  CHECK(all_refuse(&pack), "unopened (zeroed) pack: every function refuses (NOT_READY or 0)");
  CHECK(all_refuse(NULL), "NULL pack: every function refuses (NOT_READY or 0)");
  CHECK(qa_open(NULL,QA_DEFAULT_OFFSET)==QA_ERR_NOT_READY && qa_open_at(NULL,pk)==QA_ERR_NOT_READY, "  ...and opening into NULL is refused");

  /* ---- one pack, from an array ---- */
  CHECK(qa_open_at(&pack,pk)==QA_OK, "pack opens");
  CHECK(qa_count(&pack)==6 && qa_size(&pack)==(uint32_t)n, "6 files, size matches the file");
  for(uint16_t i=0;i<qa_count(&pack);i++){ qa_get(&pack,i,&a); printf("      %-22s %6u bytes  type %u  flags %u\n",a.name,a.size,a.type,a.flags); }
  CHECK(qa_get(&pack,6,&a)==QA_ERR_NOT_FOUND, "file number past the end -> not found");
  CHECK(qa_verify(&pack)==QA_OK, "CRC-32 matches");
  struct { const char*name; const uint8_t*ref; uint32_t size; int transp; } imgs[]={
    {"dice/d20.bmp",img_d20,img_d20_size,1},{"items/potion.bmp",img_potion,img_potion_size,1},
    {"art/banner.bmp",img_banner,img_banner_size,0},{"art/landscape.bmp",img_landscape,img_landscape_size,0},
    {"ui/d20_prebuilt.bmp",img_d20,img_d20_size,1}};
  for(int i=0;i<5;i++){ char msg[96]; int ok = qa_find(&pack,imgs[i].name,&a)==QA_OK && a.size==imgs[i].size && memcmp(a.data,imgs[i].ref,a.size)==0
       && a.type==QA_TYPE_IMAGE && ((a.flags&QA_FLAG_TRANSPARENT)!=0)==imgs[i].transp;
    qg_image_t im; ok = ok && qg_image_open(&im,a.data,a.size,(a.flags&QA_FLAG_TRANSPARENT)?QG_IMAGE_TRANSPARENT:0)==QG_OK;
    snprintf(msg,sizeof msg,"%s: identical to the M6 array, right flag, opens as an image",imgs[i].name); CHECK(ok,msg);
    CHECK(((uintptr_t)a.data - (uintptr_t)pk) % 4 == 0, "  ...and starts on a 4-byte boundary"); }
  CHECK(qa_find(&pack,"text/welcome.txt",&a)==QA_OK && a.type==QA_TYPE_TEXT && memcmp(a.data,"Welcome",7)==0, "text file found, type text");
  CHECK(qa_find(&pack,"dice/d21.bmp",&a)==QA_ERR_NOT_FOUND, "missing name -> not found");
  CHECK(qa_find(&pack,"Dice/d20.bmp",&a)==QA_ERR_NOT_FOUND, "names are case-sensitive");
  CHECK(qa_find(&pack,"",&a)==QA_ERR_NOT_FOUND && qa_find(&pack,NULL,&a)==QA_ERR_NOT_FOUND, "empty and NULL names -> not found");

  /* ---- damage ---- */
  uint8_t *bad = malloc(n); memcpy(bad,pk,n); bad[n-100]^=0x01; qa_open_at(&pack,bad);
  CHECK(qa_verify(&pack)==QA_ERR_CORRUPT, "one flipped bit -> CRC mismatch detected");
  memcpy(bad,pk,n); bad[32+36+2]=0x7F; CHECK(qa_open_at(&pack,bad)==QA_ERR_CORRUPT, "entry pointing outside the pack -> refused when opened");
  CHECK(all_refuse(&pack), "  ...and the failed open leaves the pack closed: every function refuses");
  memset(bad,0xFF,n); CHECK(qa_open_at(&pack,bad)==QA_ERR_NO_PACK, "erased flash (all 0xFF) -> no pack");
  memcpy(bad,pk,n); bad[4]=2; CHECK(qa_open_at(&pack,bad)==QA_ERR_VERSION, "newer version -> version error");
  memcpy(bad,pk,n); bad[1]='G'; /* the identifying bytes QG4P 1.0 packs had */ CHECK(qa_open_at(&pack,bad)==QA_ERR_NO_PACK, "a QG4P 1.0 pack (old identifying bytes) -> no pack: rebuild it");

  /* ---- two packs open at once, from arrays ---- */
  long n1, n2; uint8_t *p1 = load("packs/one.bin",&n1), *p2 = load("packs/two.bin",&n2);
  qa_pack_t one = {0}, two = {0};
  CHECK(qa_open_at(&one,p1)==QA_OK && qa_open_at(&two,p2)==QA_OK, "two different packs open at the same time");
  CHECK(qa_count(&one)==3 && qa_count(&two)==2 && qa_size(&one)==(uint32_t)n1 && qa_size(&two)==(uint32_t)n2, "  ...each with its own count and size");
  CHECK(qa_find(&one,"hello.txt",&a)==QA_OK && a.data>=p1 && a.data<p1+n1, "  a file found in pack one, inside pack one");
  CHECK(qa_find(&two,"sound/beep.raw",&a)==QA_OK && a.type==QA_TYPE_SOUND && a.data>=p2 && a.data<p2+n2, "  a file found in pack two, inside pack two");
  CHECK(qa_find(&two,"hello.txt",&a)==QA_ERR_NOT_FOUND && qa_find(&one,"sound/beep.raw",&a)==QA_ERR_NOT_FOUND, "  a name only in one pack isn't found in the other");
  CHECK(strcmp(text_of(&one,"shared.txt"),"one\n")==0 && strcmp(text_of(&two,"shared.txt"),"two\n")==0, "  the same name in both: each pack gives its own file");
  CHECK(qa_verify(&one)==QA_OK && qa_verify(&two)==QA_OK, "  both pass their checksums");

  /* ---- qa_open: packs in the pretend flash ---- */
  memcpy(qa_host_flash+QA_DEFAULT_OFFSET,p1,n1); memcpy(qa_host_flash+0x280000,p2,n2);
  CHECK(qa_open(&one,QA_DEFAULT_OFFSET)==QA_OK && qa_open(&two,0x280000)==QA_OK, "qa_open: packs at 1 MB and 2.5 MB open side by side");
  CHECK(qa_find(&one,"only_one.txt",&a)==QA_OK && a.data>=qa_host_flash+QA_DEFAULT_OFFSET && a.data<qa_host_flash+QA_DEFAULT_OFFSET+n1
        && strcmp(text_of(&two,"shared.txt"),"two\n")==0, "  ...and read in place from flash");
  CHECK(qa_open(&pack,0x200000)==QA_ERR_NO_PACK && all_refuse(&pack), "empty flash -> no pack, and the pack stays closed");
  qa_host_firmware_size = QA_DEFAULT_OFFSET + 4096;
  CHECK(qa_open(&one,QA_DEFAULT_OFFSET)==QA_ERR_OVERLAP && all_refuse(&one), "firmware grown into the pack's area -> overlap");
  qa_host_firmware_size = 64 * 1024;

  /* ---- qa_open: running off the end of flash ---- */
  uint32_t end_ofs = QA_HOST_FLASH_SIZE - 4096;                  /* the last 4 KB sector */
  CHECK(n > 4096, "(the hardware test pack is bigger than one 4 KB sector)");
  memcpy(qa_host_flash+end_ofs,pk,4096);                          /* as much as fits      */
  CHECK(qa_open(&pack,end_ofs)==QA_ERR_TOO_BIG && all_refuse(&pack), "pack whose offset + size passes the end of flash -> too big");
  CHECK(qa_open(&pack,QA_HOST_FLASH_SIZE)==QA_ERR_TOO_BIG && qa_open(&pack,0xFFFFF000u)==QA_ERR_TOO_BIG, "offset at or past the end of flash -> too big");
  memcpy(qa_host_flash+end_ofs,p1,n1);
  CHECK(n1 <= 4096 && qa_open(&pack,end_ofs)==QA_OK && qa_verify(&pack)==QA_OK, "a pack that just fits in the last sector opens");
  CHECK(strcmp(qa_err_str(QA_ERR_TOO_BIG),"asset pack runs past the end of flash")==0 && strcmp(qa_err_str(QA_ERR_NOT_FOUND),"not found")==0, "error text for too big (and the old texts unchanged)");

  printf(fails? "\n%d FAILED\n":"\nALL PASS\n", fails); return fails; }
