/*
 * Copyright 2022, 2024 S. V. Nickolas.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the Software), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * This is a recreation of the MS-DOS EXE2BIN utility designed to be usable on
 * systems that have C99 compilers but might not necessarily be little-endian.
 * The purpose is to assist in cross-compilation.  I felt having such a tool
 * with a university style license (here, UIUC with UCB warranty disclaimer)
 * would be useful even though Watcom C does come with their own equivalent.
 *
 * To avoid the issue of having to prompt for a relocation segment, as in the
 * actual exe2bin, a switch was provided.  Also, general MS-DOS semantics and
 * conventions have been replaced by Unix equivalents.
 *
 * Testing was done with tcc, rather than GCC.  It was just easier that way.
 *
 * Usage:  exereloc [-rXXXX] filename.exe filename.bin
 *         -r  relocation segment (in hex).
 *
 * Returns: 0 = no error was noted
 *          1 = invalid switch
 *          2 = invalid number of parameters
 *          3 = file open error
 *          4 = read error
 *          5 = not an MZ file
 *          6 = file too big
 *          7 = cannot convert
 *          8 = write error
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Note: If you only have a C89 compiler, define uint8_t, uint16_t and
 *       uint32_t appropriately and remove <stdint.h>.  I don't think any
 *       other C99isms are used here.
 *
 * Note: unistd.h is used for getopt(3), if your compiler doesn't have one
 *       there's a few free options (like egetopt in BSD pr(1)).
 */
#include <stdint.h>
#include <unistd.h>

/*
 * Note: __attribute__((packed)) is used to pack the MZ header and relocation
 *       table structs; if your compiler doesn't like this, replace it with
 *       whatever the compiler prefers.
 */
#ifndef DEALIGN
#define DEALIGN __attribute__((packed))
#endif

static char *argv0;

/* Must be longer than 16 bits and signed. */
long relseg;

/*
 * Portable ways of converting between native uint16_t and x86 uint16_t.
 *
 * Note: endianize() and unendianize() are important for ENDIAN NEUTRALITY.
 *       They are omitted when the only use of a variable is to test for zero
 *       or nonzero (optimization).
 */
uint16_t endianize (uint16_t x)
{
 uint8_t *y;
 uint16_t z;

 y=(void *)(&x);
 z=y[1];
 z<<=8;
 z|=y[0];
 return z;
}

uint16_t unendianize (uint16_t x)
{
 uint16_t y;
 uint8_t *z;

 z=(void *)(&y);
 z[0]=x&0x00FF;
 z[1]=(x>>8)&0xFF;
 return y;
}

/* EXE header format. */
struct MZ {
 uint16_t sig         DEALIGN;
 uint16_t xbytes      DEALIGN;
 uint16_t fullpages   DEALIGN;
 uint16_t relocs      DEALIGN;
 uint16_t hdrlen      DEALIGN;
 uint16_t memrequire  DEALIGN;
 uint16_t memrequest  DEALIGN;
 uint16_t ss          DEALIGN;
 uint16_t sp          DEALIGN;
 uint16_t checksum    DEALIGN;
 uint16_t ip          DEALIGN;
 uint16_t cs          DEALIGN;
 uint16_t relocptr    DEALIGN;
 uint16_t ovl         DEALIGN;
 uint16_t ovldat      DEALIGN;
};

/* EXE relocation table format. */
struct RL {
 uint16_t off  DEALIGN;
 uint16_t seg  DEALIGN;
};

/* perror(3) with name of program */
void xperror (char *filename)
{
 fprintf (stderr, "%s: %s: %s\n", argv0, filename, strerror(errno));
}

/* The actual meat and bones of the program. */
int doit (char *src, char *tgt)
{
 int e;
 FILE *file;
 char *buf;
 struct MZ mz;
 struct RL *rl;
 long r, rs;
 uint32_t s;
 int comovr;

 /* Open the first file, then examine the header. */
 file=fopen(src, "rb");
 if (!file)
 {
  xperror(src);
  return 3;
 }
 if (!fread(&mz, sizeof(struct MZ), 1, file))
 {
  fprintf(stderr, "%s: %s: could not read EXE header\n", argv0, src);
  return 4;
 }

 s=endianize(mz.fullpages);
 s<<=9;
 if (mz.xbytes) s-=512;
 s+=endianize(mz.xbytes);
 s-=((endianize(mz.hdrlen))<<4);

 /* Supposedly "ZM" EXEs exist but I have NEVER seen any in the wild. */
 if (endianize(mz.sig)!=0x5A4D)
 {
  fprintf (stderr, "%s: %s: source not a DOS EXE file\n", argv0, src);
  fclose(file);
  return 5;
 }

 if (s>0xFFFF)
 {
  fprintf (stderr, "%s: %s: file too large to convert\n", argv0, src);
  fclose(file);
  return 6;
 }

 buf=malloc(s);
 if (!buf)
 {
  fprintf (stderr, "%s: %s: file too large for memory\n", argv0, src);
  fclose(file);
  return 6;
 }

 /* Only start addresses of 0000:0000 and 0000:0100 accepted. */
 if (mz.cs||((mz.ip)&&(endianize(mz.ip)!=0x0100)))
 {
  fprintf (stderr, "%s: %s: invalid initial start address\n", argv0, src);
  fclose(file);
  return 7;
 }

 if (mz.ss||mz.sp)
 {
  fprintf (stderr, "%s: %s: "
           "cannot convert EXE with explicitly located stack\n", argv0, src);
  fclose(file);
  return 7;
 }

 rl=0;
 if (mz.relocs)
 {
  /* Only do relocations if start IP is 0. */
  if (mz.ip)
  {
   fprintf (stderr, "%s: %s: "
            "cannot convert EXE with relocation table and IP=0x0100\n",
            argv0, src);
   fclose(file);
   return 7;
  }
  if (relseg<0)
  {
   fprintf (stderr, "%s: %s: "
            "relocations needed and possible; use -r switch\n", argv0, src);
   fclose(file);
   return 7;
  }

  /* Get ready to slurp the relocation table into memory. */
  rl=malloc(endianize(mz.relocs)<<2);
  if (!rl)
  {
   fclose(file);
   fprintf (stderr, "%s: %s: insufficient memory for relocation table\n",
            argv0, src);
   return 6;
  }
  fseek(file,endianize(mz.relocptr),SEEK_SET);
  r=fread(rl, 4, endianize(mz.relocs), file);
  if (r<endianize(mz.relocs))
  {
   fclose(file);
   free(rl);
   fprintf (stderr, "%s: %s: could not read relocation table\n", argv0, src);
   return 4;
  }
 }

 comovr=(endianize(mz.ip)==0x0100);

 /* Slurp the run image into memory. */
 fseek(file,(endianize(mz.hdrlen))<<4,SEEK_SET);
 e=0;
 r=fread(buf, 1, s, file);
 if (r<s)
 {
  e=4; /* "Read Error" */
  fprintf (stderr, "%s: %s: short read\n", argv0, src);
 }

 /* We're done with you. */
 fclose(file);

 /* Apply relocations. */
 if (rl)
 {
  int t;

  for (t=0; t<endianize(mz.relocs); t++)
  {
   uint16_t *p;
   uint32_t ro;
   ro=endianize(rl[t].seg);
   ro<<=4; /* LOL 8086 */
   ro+=endianize(rl[t].off);
   p=(uint16_t *)(&(buf[ro]));
   *p=unendianize(endianize(*p)+relseg);
  }

  free(rl);
 }

 /* Get ready to spit it out. */
 file=fopen(tgt, "wb");
 if (!file)
 {
  free(buf);
  xperror(tgt);
  return 3;
 }

 /* Note - hold onto the return value until after our file buffer is freed! */
 r=fwrite(comovr?(buf+0x0100):buf, 1, comovr?(s-0x0100):s, file);

 fclose(file);

 free(buf);
 if (r<(comovr?(s-0x0100):s))
 {
  fprintf (stderr, "%s: %s: short write\n", argv0, tgt);
  return 8;
 }
 return e;
}

void usage (void)
{
 fprintf (stderr, "%s: usage: %s [-r relocseg] filename.exe filename.bin\n",
          argv0, argv0);
}

int main (int argc, char **argv)
{
 int e;

 argv0=argv[0];
 relseg=-1;

 while (-1!=(e=getopt(argc, argv, "r:")))
 {
  switch (e)
  {
   case 'r':
    relseg=strtol(optarg,0,16);
    if (relseg<0)
    {
     fprintf (stderr, "%s: invalid relocation: %s\n", argv0, optarg);
     return 1;
    }
    break;
   default:
    usage();
    return 1;
  }
 }

 if (argc!=(optind+2))
 {
  usage();
  return 2;
 }

 return doit(argv[optind], argv[optind+1]);
}
