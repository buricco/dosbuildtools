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

#include <stdio.h>

/*
 * Method:
 * 
 * 1. Patch the word at head[1] and head[2] to the filesize plus 21.
 * 2. Concatenate head, the meat of the file, and tail.
 * 
 * Obviously this will require that the source EXE will be no larger than
 * 65141 / 0xFE75 (I believe) bytes.
 * 
 * Note that while Microsoft's own tool created the .com filename from the
 * .exe, this version does not and you must provide BOTH.
 */

unsigned char head[16]={
 0xE9, 0x00, 0x00, 0x43, 0x6F, 0x6E, 0x76, 0x65,
 0x72, 0x74, 0x65, 0x64, 0x00, 0x00, 0x00, 0x00
};

unsigned char tail[123]={
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x5B, 0x50, 
 0x8C, 0xC0, 0x05, 0x10, 0x00, 0x8B, 0x0E, 0x1E, 0x01, 0x03, 0xC8, 0x89, 0x4F, 
 0xFB, 0x8B, 0x0E, 0x26, 0x01, 0x03, 0xC8, 0x89, 0x4F, 0xF7, 0x8B, 0x0E, 0x20, 
 0x01, 0x89, 0x4F, 0xF9, 0x8B, 0x0E, 0x24, 0x01, 0x89, 0x4F, 0xF5, 0x8B, 0x3E, 
 0x28, 0x01, 0x8B, 0x16, 0x18, 0x01, 0xB1, 0x04, 0xD3, 0xE2, 0x8B, 0x0E, 0x16, 
 0x01, 0xE3, 0x1A, 0x26, 0xC5, 0xB5, 0x10, 0x01, 0x83, 0xC7, 0x04, 0x8C, 0xDD, 
 0x26, 0x03, 0x2E, 0x18, 0x01, 0x83, 0xC5, 0x01, 0x03, 0xE8, 0x8E, 0xDD, 0x01, 
 0x04, 0xE2, 0xE6, 0x0E, 0x1F, 0xBF, 0x00, 0x01, 0x8B, 0xF2, 0x81, 0xC6, 0x10, 
 0x01, 0x8B, 0xCB, 0x2B, 0xCE, 0xF3, 0xA4, 0x58, 0xFA, 0x8E, 0x57, 0xFB, 0x8B, 
 0x67, 0xF9, 0xFB, 0xFF, 0x6F, 0xF5
};

#ifndef __MSDOS__
#define far
#endif

int main (int argc, char **argv)
{
 FILE *file;
 
 char fnout[128];
 unsigned char b1, b2;
 
 unsigned char far buf[65141];
 unsigned long l, e;
 
 if (argc!=3)
 {
  fprintf (stderr, "usage: uconvert filename.exe filename.com\n");
  return 1;
 }

 file=fopen(argv[1],"rb");
 if (!file)
 {
  perror(argv[1]);
  return 2;
 }
 fseek(file,0,SEEK_END);
 l=ftell(file);
 fseek(file,0,SEEK_SET);
 if (l>65141)
 {
  fclose(file);
  fprintf (stderr, "File %s is too large to convert\n", argv[1]);
  return 3;
 }
 e=fread(buf,1,l,file);
 fclose(file);
 if (e<l)
 {
  fprintf (stderr, "Short read\n");
  perror(argv[1]);
 }
 
 file=fopen(argv[2],"wb");
 if (!file)
 {
  perror(argv[2]);
  return 4;
 }
 
 b1=(l+21)&0xFF;
 b2=((l+21)>>8)&0xFF;
 head[1]=b1;
 head[2]=b2;
 e=fwrite(head,1,16,file);
 if (e<16)
 {
  fclose(file);
  fprintf (stderr, "Short write when writing header\n");
  perror(argv[2]);
  return 5;
 }
 e=fwrite(buf,1,l,file);
 if (e<l)
 {
  fclose(file);
  fprintf (stderr, "Short write when copying program\n");
  perror(argv[2]);
  return 6;
 }
 e=fwrite(tail,1,123,file);
 fclose(file);
 if (e<123)
 {
  fprintf (stderr, "Short write when writing footer\n");
  perror(argv[2]);
  return 7;
 }
 return 0;
}
