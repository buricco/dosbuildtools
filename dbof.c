/*
 * Copyright 2024 S. V. Nickolas.
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char **argv)
{
 FILE *in, *out;
 size_t off, len, cur;
 
 if (argc!=5)
 {
  fprintf (stderr, "usage: %s source target offset length\n"
                   "       all values in hexadecimal\n", argv[0]);
  return 1;
 }
 
 off=len=cur=0;
 off=strtoul(argv[3], NULL, 16);
 len=strtoul(argv[4], NULL, 16);
 if (!off)
 {
  fprintf (stderr, "invalid offset %s\n", argv[3]);
  return 1;
 }
 if (!len)
 {
  fprintf (stderr, "invalid length %s\n", argv[4]);
  return 1;
 }
 in=fopen(argv[1],"rb");
 if (!in)
 {
  perror(argv[1]);
  return 1;
 }
 out=fopen(argv[2],"wt");
 if (!out)
 {
  fclose(in);
  perror(argv[2]);
  return 1;
 }
 fseek(in,off,SEEK_SET);
 for (cur=0; cur<len; cur++)
 {
  int c;
  
  c=fgetc(in);
  if (c<0)
  {
   fclose(in);
   fclose(out);
   perror(argv[1]);
   return 1;
  }
  if (cur&&(!(cur&7))) fprintf(out,"\n");
  if (!(cur&7))
   fprintf(out, "        DB      "); 
  else 
   fprintf(out, ", ");
  fprintf (out, "0%02XH",c);
 }
 fprintf (out, "\n");
 fclose(out);
 return 0;
}
