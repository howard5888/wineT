/*
 * Unit test suite for file functions
 *
 * Copyright 2002 Bill Currie
 * Copyright 2005 Paul Rupe
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "wine/test.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <share.h>
#include <sys/stat.h>
#include <io.h>
#include <windef.h>
#include <winbase.h>
#include <winnls.h>
#include <process.h>
#include <errno.h>
#include "msvcrt.h"

static void test_fdopen( void )
{
    static const char buffer[] = {0,1,2,3,4,5,6,7,8,9};
    char ibuf[10];
    int fd;
    FILE *file;

    fd = open ("fdopen.tst", O_WRONLY | O_CREAT | O_BINARY, _S_IREAD |_S_IWRITE);
    write (fd, buffer, sizeof (buffer));
    close (fd);

    fd = open ("fdopen.tst", O_RDONLY | O_BINARY);
    lseek (fd, 5, SEEK_SET);
    file = fdopen (fd, "rb");
    ok (fread (ibuf, 1, sizeof (buffer), file) == 5, "read wrong byte count\n");
    ok (memcmp (ibuf, buffer + 5, 5) == 0, "read wrong bytes\n");
    fclose (file);
    unlink ("fdopen.tst");
}

static void test_fileops( void )
{
    static const char outbuffer[] = "0,1,2,3,4,5,6,7,8,9";
    char buffer[256];
    WCHAR wbuffer[256];
    int fd;
    FILE *file;
    fpos_t pos;
    int i, c;

    fd = open ("fdopen.tst", O_WRONLY | O_CREAT | O_BINARY, _S_IREAD |_S_IWRITE);
    write (fd, outbuffer, sizeof (outbuffer));
    close (fd);

    fd = open ("fdopen.tst", O_RDONLY | O_BINARY);
    file = fdopen (fd, "rb");
    ok(strlen(outbuffer) == (sizeof(outbuffer)-1),"strlen/sizeof error\n");
    ok(fgets(buffer,sizeof(buffer),file) !=0,"fgets failed unexpected\n");
    ok(fgets(buffer,sizeof(buffer),file) ==0,"fgets didn't signal EOF\n");
    ok(feof(file) !=0,"feof doesn't signal EOF\n");
    rewind(file);
    ok(fgets(buffer,strlen(outbuffer),file) !=0,"fgets failed unexpected\n");
    ok(lstrlenA(buffer) == lstrlenA(outbuffer) -1,"fgets didn't read right size\n");
    ok(fgets(buffer,sizeof(outbuffer),file) !=0,"fgets failed unexpected\n");
    ok(strlen(buffer) == 1,"fgets dropped chars\n");
    ok(buffer[0] == outbuffer[strlen(outbuffer)-1],"fgets exchanged chars\n");

    rewind(file);
    for (i = 0, c = EOF; i < sizeof(outbuffer); i++)
    {
        ok((c = fgetc(file)) == outbuffer[i], "fgetc returned wrong data\n");
    }
    ok((c = fgetc(file)) == EOF, "getc did not return EOF\n");
    ok(feof(file), "feof did not return EOF\n");
    ok(ungetc(c, file) == EOF, "ungetc(EOF) did not return EOF\n");
    ok(feof(file), "feof after ungetc(EOF) did not return EOF\n");
    ok((c = fgetc(file)) == EOF, "getc did not return EOF\n");
    c = outbuffer[sizeof(outbuffer) - 1];
    ok(ungetc(c, file) == c, "ungetc did not return its input\n");
    ok(!feof(file), "feof after ungetc returned EOF\n");
    ok((c = fgetc(file)) != EOF, "getc after ungetc returned EOF\n");
    ok(c == outbuffer[sizeof(outbuffer) - 1],
       "getc did not return ungetc'd data\n");
    ok(!feof(file), "feof after getc returned EOF prematurely\n");
    ok((c = fgetc(file)) == EOF, "getc did not return EOF\n");
    ok(feof(file), "feof after getc did not return EOF\n");

    rewind(file);
    ok(fgetpos(file,&pos) == 0, "fgetpos failed unexpected\n");
    ok(pos == 0, "Unexpected result of fgetpos 0x%Lx\n", pos);
    pos = (ULONGLONG)sizeof (outbuffer);
    ok(fsetpos(file, &pos) == 0, "fsetpos failed unexpected\n");
    ok(fgetpos(file,&pos) == 0, "fgetpos failed unexpected\n");
    ok(pos == (ULONGLONG)sizeof (outbuffer), "Unexpected result of fgetpos 0x%Lx\n", pos);

    fclose (file);
    fd = open ("fdopen.tst", O_RDONLY | O_TEXT);
    file = fdopen (fd, "rt"); /* open in TEXT mode */
    ok(fgetws(wbuffer,sizeof(wbuffer),file) !=0,"fgetws failed unexpected\n");
    ok(fgetws(wbuffer,sizeof(wbuffer),file) ==0,"fgetws didn't signal EOF\n");
    ok(feof(file) !=0,"feof doesn't signal EOF\n");
    rewind(file);
    ok(fgetws(wbuffer,strlen(outbuffer),file) !=0,"fgetws failed unexpected\n");
    ok(lstrlenW(wbuffer) == (lstrlenA(outbuffer) -1),"fgetws didn't read right size\n");
    ok(fgetws(wbuffer,sizeof(outbuffer),file) !=0,"fgets failed unexpected\n");
    ok(lstrlenW(wbuffer) == 1,"fgets dropped chars\n");
    fclose (file);

    file = fopen("fdopen.tst", "rb");
    ok( file != NULL, "fopen failed\n");
    /* sizeof(buffer) > content of file */
    ok(fread(buffer, sizeof(buffer), 1, file) == 0, "fread test failed\n");
    /* feof should be set now */
    ok(feof(file), "feof after fread failed\n");
    fclose (file);

    unlink ("fdopen.tst");
}

#define IOMODE (ao?"ascii mode":"binary mode")
static void test_readmode( BOOL ascii_mode )
{
    static const char outbuffer[] = "0,1,2,3,4,5,6,7,8,9\r\n\r\nA,B,C,D,E\r\nX,Y,Z";
    static const char padbuffer[] = "ghjghjghjghj";
    static const char nlbuffer[] = "\r\n";
    char buffer[MSVCRT_BUFSIZ+256];
    int fd;
    FILE *file;
    int i, j, fp, ao, *ip, pl;
    long l;

    fd = open ("fdopen.tst", O_WRONLY | O_CREAT | O_BINARY, _S_IREAD |_S_IWRITE);
    /* an internal buffer of MSVCRT_BUFSIZ is maintained, so make a file big
     * enough to test operations that cross the buffer boundary 
     */
    j = (MSVCRT_BUFSIZ-4)/strlen(padbuffer);
    for (i=0; i<j; i++)
        write (fd, padbuffer, strlen(padbuffer));
    j = (MSVCRT_BUFSIZ-4)%strlen(padbuffer);
    for (i=0; i<j; i++)
        write (fd, &padbuffer[i], 1);
    write (fd, nlbuffer, strlen(nlbuffer));
    write (fd, outbuffer, sizeof (outbuffer));
    close (fd);
    
    if (ascii_mode) {
        /* Open file in ascii mode */
        fd = open ("fdopen.tst", O_RDONLY);
        file = fdopen (fd, "r");
        ao = -1; /* on offset to account for carriage returns */
    }
    else {
        fd = open ("fdopen.tst", O_RDONLY | O_BINARY);
        file = fdopen (fd, "rb");
        ao = 0;
    }
    
    /* first is a test of fgets, ftell, fseek */
    ok(ftell(file) == 0,"Did not start at beginning of file in %s\n", IOMODE);
    ok(fgets(buffer,MSVCRT_BUFSIZ+256,file) !=0,"padding line fgets failed unexpected in %s\n", IOMODE);
    l = ftell(file);
    pl = MSVCRT_BUFSIZ-2;
    ok(l == pl,"padding line ftell got %ld should be %d in %s\n", l, pl, IOMODE);
    ok(lstrlenA(buffer) == pl+ao,"padding line fgets got size %d should be %d in %s\n",
     lstrlenA(buffer), pl+ao, IOMODE);
    for (fp=0; fp<strlen(outbuffer); fp++)
        if (outbuffer[fp] == '\n') break;
    fp++;
    ok(fgets(buffer,256,file) !=0,"line 1 fgets failed unexpected in %s\n", IOMODE);
    l = ftell(file);
    ok(l == pl+fp,"line 1 ftell got %ld should be %d in %s\n", l, pl+fp, IOMODE);
    ok(lstrlenA(buffer) == fp+ao,"line 1 fgets got size %d should be %d in %s\n",
     lstrlenA(buffer), fp+ao, IOMODE);
    /* test a seek back across the buffer boundary */
    l = pl;
    ok(fseek(file,l,SEEK_SET)==0,"seek failure in %s\n", IOMODE);
    l = ftell(file);
    ok(l == pl,"ftell after seek got %ld should be %d in %s\n", l, pl, IOMODE);
    ok(fgets(buffer,256,file) !=0,"second read of line 1 fgets failed unexpected in %s\n", IOMODE);
    l = ftell(file);
    ok(l == pl+fp,"second read of line 1 ftell got %ld should be %d in %s\n", l, pl+fp, IOMODE);
    ok(lstrlenA(buffer) == fp+ao,"second read of line 1 fgets got size %d should be %d in %s\n",
     lstrlenA(buffer), fp+ao, IOMODE);
    ok(fgets(buffer,256,file) !=0,"line 2 fgets failed unexpected in %s\n", IOMODE);
    fp += 2;
    l = ftell(file);
    ok(l == pl+fp,"line 2 ftell got %ld should be %d in %s\n", l, pl+fp, IOMODE);
    ok(lstrlenA(buffer) == 2+ao,"line 2 fgets got size %d should be %d in %s\n",
     lstrlenA(buffer), 2+ao, IOMODE);
    
    /* test fread across buffer boundary */
    rewind(file);
    ok(ftell(file) == 0,"Did not start at beginning of file in %s\n", IOMODE);
    ok(fgets(buffer,MSVCRT_BUFSIZ-6,file) !=0,"padding line fgets failed unexpected in %s\n", IOMODE);
    j=strlen(outbuffer);
    i=fread(buffer,1,256,file);
    ok(i==j+6+ao*4,"fread failed, expected %d got %d in %s\n", j+6+ao*4, i, IOMODE);
    l = ftell(file);
    ok(l == pl+j+1,"ftell after fread got %ld should be %d in %s\n", l, pl+j+1, IOMODE);
    /* fread should return the requested number of bytes if available */
    rewind(file);
    ok(ftell(file) == 0,"Did not start at beginning of file in %s\n", IOMODE);
    ok(fgets(buffer,MSVCRT_BUFSIZ-6,file) !=0,"padding line fgets failed unexpected in %s\n", IOMODE);
    j = fp+10;
    i=fread(buffer,1,j,file);
    ok(i==j,"fread failed, expected %d got %d in %s\n", j, i, IOMODE);
    
    /* test some additional functions */
    rewind(file);
    ok(ftell(file) == 0,"Did not start at beginning of file in %s\n", IOMODE);
    ok(fgets(buffer,MSVCRT_BUFSIZ+256,file) !=0,"padding line fgets failed unexpected in %s\n", IOMODE);
    i = _getw(file);
    ip = (int *)outbuffer;
    ok(i == *ip,"_getw failed, expected %08x got %08x in %s\n", *ip, i, IOMODE);
    for (fp=0; fp<strlen(outbuffer); fp++)
        if (outbuffer[fp] == '\n') break;
    fp++;
    /* this will cause the next _getw to cross carriage return characters */
    ok(fgets(buffer,fp-6,file) !=0,"line 1 fgets failed unexpected in %s\n", IOMODE);
    for (i=0, j=0; i<6; i++) {
        if (ao==0 || outbuffer[fp-3+i] != '\r')
            buffer[j++] = outbuffer[fp-3+i];
    }
    i = _getw(file);
    ip = (int *)buffer;
    ok(i == *ip,"_getw failed, expected %08x got %08x in %s\n", *ip, i, IOMODE);

    fclose (file);
    unlink ("fdopen.tst");
}


static WCHAR* AtoW( char* p )
{
    WCHAR* buffer;
    DWORD len = MultiByteToWideChar( CP_ACP, 0, p, -1, NULL, 0 );
    buffer = malloc( len * sizeof(WCHAR) );
    MultiByteToWideChar( CP_ACP, 0, p, -1, buffer, len );
    return buffer;
}

static void test_fgetwc( void )
{
#define LLEN 512

  char* tempf;
  FILE *tempfh;
  static const char mytext[]= "This is test_fgetwc\r\n";
  WCHAR wtextW[MSVCRT_BUFSIZ+LLEN+1];
  WCHAR *mytextW = NULL, *aptr, *wptr;
  BOOL diff_found = FALSE;
  int i, j;
  long l;

  tempf=_tempnam(".","wne");
  tempfh = fopen(tempf,"wb");
  j = 'a';
  /* pad to almost the length of the internal buffer */
  for (i=0; i<MSVCRT_BUFSIZ-4; i++)
    fputc(j,tempfh);
  j = '\r';
  fputc(j,tempfh);
  j = '\n';
  fputc(j,tempfh);
  fputs(mytext,tempfh);
  fclose(tempfh);
  /* in text mode, getws/c expects multibyte characters */
  /*currently Wine only supports plain ascii, and that is all that is tested here */
  tempfh = fopen(tempf,"rt"); /* open in TEXT mode */
  fgetws(wtextW,LLEN,tempfh);
  l=ftell(tempfh);
  ok(l==MSVCRT_BUFSIZ-2, "ftell expected %d got %ld\n", MSVCRT_BUFSIZ-2, l);
  fgetws(wtextW,LLEN,tempfh);
  l=ftell(tempfh);
  ok(l==MSVCRT_BUFSIZ-2+strlen(mytext), "ftell expected %d got %ld\n",
   MSVCRT_BUFSIZ-2+strlen(mytext), l);
  mytextW = AtoW ((char*)mytext);
  aptr = mytextW;
  wptr = wtextW;
  for (i=0; i<strlen(mytext)-2; i++, aptr++, wptr++)
    {
      diff_found |= (*aptr != *wptr);
    }
  ok(!(diff_found), "fgetwc difference found in TEXT mode\n");
  ok(*wptr == '\n', "Carriage return was not skipped\n");
  fclose(tempfh);
  unlink(tempf);
  
  tempfh = fopen(tempf,"wb");
  j = 'a';
  /* pad to almost the length of the internal buffer. Use an odd number of bytes
     to test that we can read wchars that are split across the internal buffer
     boundary */
  for (i=0; i<MSVCRT_BUFSIZ-3-strlen(mytext)*sizeof(WCHAR); i++)
    fputc(j,tempfh);
  j = '\r';
  fputwc(j,tempfh);
  j = '\n';
  fputwc(j,tempfh);
  fputws(wtextW,tempfh);
  fputws(wtextW,tempfh);
  fclose(tempfh);
  /* in binary mode, getws/c expects wide characters */
  tempfh = fopen(tempf,"rb"); /* open in BINARY mode */
  j=(MSVCRT_BUFSIZ-2)/sizeof(WCHAR)-strlen(mytext);
  fgetws(wtextW,j,tempfh);
  l=ftell(tempfh);
  j=(j-1)*sizeof(WCHAR);
  ok(l==j, "ftell expected %d got %ld\n", j, l);
  i=fgetc(tempfh);
  ok(i=='a', "fgetc expected %d got %d\n", 0x61, i);
  l=ftell(tempfh);
  j++;
  ok(l==j, "ftell expected %d got %ld\n", j, l);
  fgetws(wtextW,3,tempfh);
  ok(wtextW[0]=='\r',"expected carriage return got %04hx\n", wtextW[0]);
  ok(wtextW[1]=='\n',"expected newline got %04hx\n", wtextW[1]);
  l=ftell(tempfh);
  j += 4;
  ok(l==j, "ftell expected %d got %ld\n", j, l);
  for(i=0; i<strlen(mytext); i++)
    wtextW[i] = 0;
  /* the first time we get the string, it should be entirely within the local buffer */
  fgetws(wtextW,LLEN,tempfh);
  l=ftell(tempfh);
  j += (strlen(mytext)-1)*sizeof(WCHAR);
  ok(l==j, "ftell expected %d got %ld\n", j, l);
  diff_found = FALSE;
  aptr = mytextW;
  wptr = wtextW;
  for (i=0; i<strlen(mytext)-2; i++, aptr++, wptr++)
    {
      ok(*aptr == *wptr, "Char %d expected %04hx got %04hx\n", i, *aptr, *wptr);
      diff_found |= (*aptr != *wptr);
    }
  ok(!(diff_found), "fgetwc difference found in BINARY mode\n");
  ok(*wptr == '\n', "Should get newline\n");
  for(i=0; i<strlen(mytext); i++)
    wtextW[i] = 0;
  /* the second time we get the string, it should cross the local buffer boundary.
     One of the wchars should be split across the boundary */
  fgetws(wtextW,LLEN,tempfh);
  diff_found = FALSE;
  aptr = mytextW;
  wptr = wtextW;
  for (i=0; i<strlen(mytext)-2; i++, aptr++, wptr++)
    {
      ok(*aptr == *wptr, "Char %d expected %04hx got %04hx\n", i, *aptr, *wptr);
      diff_found |= (*aptr != *wptr);
    }
  ok(!(diff_found), "fgetwc difference found in BINARY mode\n");
  ok(*wptr == '\n', "Should get newline\n");
  
  if(mytextW) free (mytextW);
  fclose(tempfh);
  unlink(tempf);
}

static void test_ctrlz( void )
{
  char* tempf;
  FILE *tempfh;
  static const char mytext[]= "This is test_ctrlz";
  char buffer[256];
  int i, j;
  long l;

  tempf=_tempnam(".","wne");
  tempfh = fopen(tempf,"wb");
  fputs(mytext,tempfh);
  j = 0x1a; /* a ctrl-z character signals EOF in text mode */
  fputc(j,tempfh);
  j = '\r';
  fputc(j,tempfh);
  j = '\n';
  fputc(j,tempfh);
  j = 'a';
  fputc(j,tempfh);
  fclose(tempfh);
  tempfh = fopen(tempf,"rt"); /* open in TEXT mode */
  ok(fgets(buffer,256,tempfh) != 0,"fgets failed unexpected\n");
  i=strlen(buffer);
  j=strlen(mytext);
  ok(i==j, "returned string length expected %d got %d\n", j, i);
  j+=4; /* ftell should indicate the true end of file */
  l=ftell(tempfh);
  ok(l==j, "ftell expected %d got %ld\n", j, l);
  ok(feof(tempfh), "did not get EOF\n");
  fclose(tempfh);
  
  tempfh = fopen(tempf,"rb"); /* open in BINARY mode */
  ok(fgets(buffer,256,tempfh) != 0,"fgets failed unexpected\n");
  i=strlen(buffer);
  j=strlen(mytext)+3; /* should get through newline */
  ok(i==j, "returned string length expected %d got %d\n", j, i);
  l=ftell(tempfh);
  ok(l==j, "ftell expected %d got %ld\n", j, l);
  ok(fgets(buffer,256,tempfh) != 0,"fgets failed unexpected\n");
  i=strlen(buffer);
  ok(i==1, "returned string length expected %d got %d\n", 1, i);
  ok(feof(tempfh), "did not get EOF\n");
  fclose(tempfh);
  unlink(tempf);
}

static void test_file_put_get( void )
{
  char* tempf;
  FILE *tempfh;
  static const char mytext[]=  "This is a test_file_put_get\n";
  static const char dostext[]= "This is a test_file_put_get\r\n";
  char btext[LLEN];
  WCHAR wtextW[LLEN+1];
  WCHAR *mytextW = NULL, *aptr, *wptr;
  BOOL diff_found = FALSE;
  unsigned int i;

  tempf=_tempnam(".","wne");
  tempfh = fopen(tempf,"wt"); /* open in TEXT mode */
  fputs(mytext,tempfh);
  fclose(tempfh);
  tempfh = fopen(tempf,"rb"); /* open in TEXT mode */
  fgets(btext,LLEN,tempfh);
  ok( strlen(mytext) + 1 == strlen(btext),"TEXT/BINARY mode not handled for write\n");
  ok( btext[strlen(mytext)-1] == '\r', "CR not written\n");
  fclose(tempfh);
  tempfh = fopen(tempf,"wb"); /* open in BINARY mode */
  fputs(dostext,tempfh);
  fclose(tempfh);
  tempfh = fopen(tempf,"rt"); /* open in TEXT mode */
  fgets(btext,LLEN,tempfh);
  ok(strcmp(btext, mytext) == 0,"_O_TEXT read doesn't strip CR\n");
  fclose(tempfh);
  tempfh = fopen(tempf,"rb"); /* open in TEXT mode */
  fgets(btext,LLEN,tempfh);
  ok(strcmp(btext, dostext) == 0,"_O_BINARY read doesn't preserve CR\n");

  fclose(tempfh);
  tempfh = fopen(tempf,"rt"); /* open in TEXT mode */
  fgetws(wtextW,LLEN,tempfh);
  mytextW = AtoW ((char*)mytext);
  aptr = mytextW;
  wptr = wtextW;

  for (i=0; i<strlen(mytext); i++, aptr++, wptr++)
    {
      diff_found |= (*aptr != *wptr);
    }
  ok(!(diff_found), "fgetwc doesn't strip CR in TEXT mode\n");
  if(mytextW) free (mytextW);
  fclose(tempfh);
  unlink(tempf);
}

static void test_file_write_read( void )
{
  char* tempf;
  int tempfd;
  static const char mytext[]=  "This is test_file_write_read\nsecond line\n";
  static const char dostext[]= "This is test_file_write_read\r\nsecond line\r\n";
  char btext[LLEN];
  int ret, i;

  tempf=_tempnam(".","wne");
  tempfd = _open(tempf,_O_CREAT|_O_TRUNC|_O_BINARY|_O_RDWR,
                     _S_IREAD | _S_IWRITE);
  ok( tempfd != -1,
     "Can't open '%s': %d\n", tempf, errno); /* open in BINARY mode */
  ok(_write(tempfd,dostext,strlen(dostext)) == lstrlenA(dostext),
     "_write _O_BINARY bad return value\n");
  _close(tempfd);
  i = lstrlenA(mytext);
  tempfd = _open(tempf,_O_RDONLY|_O_BINARY,0); /* open in BINARY mode */
  ok(_read(tempfd,btext,i) == i,
     "_read _O_BINARY got bad length\n");
  ok( memcmp(dostext,btext,i) == 0,
      "problems with _O_BINARY  _write / _read\n");
  _close(tempfd);
  tempfd = _open(tempf,_O_RDONLY|_O_TEXT); /* open in TEXT mode */
  ok(_read(tempfd,btext,i) == i-1,
     "_read _O_TEXT got bad length\n");
  ok( memcmp(mytext,btext,i-1) == 0,
      "problems with _O_BINARY _write / _O_TEXT _read\n");
  _close(tempfd);
  tempfd = _open(tempf,_O_CREAT|_O_TRUNC|_O_TEXT|_O_RDWR,
                     _S_IREAD | _S_IWRITE);
  ok( tempfd != -1,
     "Can't open '%s': %d\n", tempf, errno); /* open in TEXT mode */
  ok(_write(tempfd,mytext,strlen(mytext)) == lstrlenA(mytext),
     "_write _O_TEXT bad return value\n");
  _close(tempfd);
  tempfd = _open(tempf,_O_RDONLY|_O_BINARY,0); /* open in BINARY mode */
  ok(_read(tempfd,btext,LLEN) == lstrlenA(dostext),
     "_read _O_BINARY got bad length\n");
  ok( memcmp(dostext,btext,strlen(dostext)) == 0,
      "problems with _O_TEXT _write / _O_BINARY _read\n");
  ok( btext[strlen(dostext)-2] == '\r', "CR not written or read\n");
  _close(tempfd);
  tempfd = _open(tempf,_O_RDONLY|_O_TEXT); /* open in TEXT mode */
  ok(_read(tempfd,btext,LLEN) == lstrlenA(mytext),
     "_read _O_TEXT got bad length\n");
  ok( memcmp(mytext,btext,strlen(mytext)) == 0,
      "problems with _O_TEXT _write / _read\n");
  _close(tempfd);

  memset(btext, 0, LLEN);
  tempfd = _open(tempf,_O_APPEND|_O_RDWR); /* open for APPEND in default mode */
  ok(tell(tempfd) == 0, "bad position %lu expecting 0\n", tell(tempfd));
  ok(_read(tempfd,btext,LLEN) == lstrlenA(mytext), "_read _O_APPEND got bad length\n");
  ok( memcmp(mytext,btext,strlen(mytext)) == 0, "problems with _O_APPEND _read\n");
  _close(tempfd);

  /* Test reading only \n or \r */
  tempfd = _open(tempf,_O_RDONLY|_O_TEXT); /* open in TEXT mode */
  _lseek(tempfd, -1, FILE_END);
  ret = _read(tempfd,btext,LLEN);
  ok(ret == 1, "_read expected 1 got bad length: %d\n", ret);
  _lseek(tempfd, -2, FILE_END);
  ret = _read(tempfd,btext,LLEN);
  ok(ret == 1 && *btext == '\n', "_read expected '\\n' got bad length: %d\n", ret);
  _lseek(tempfd, -3, FILE_END);
  ret = _read(tempfd,btext,2);
  ok(ret == 1 && *btext == 'e', "_read expected 'e' got \"%.*s\" bad length: %d\n", ret, btext, ret);
  ok(tell(tempfd) == 42, "bad position %lu expecting 42\n", tell(tempfd));
  _close(tempfd);

  ret = unlink(tempf);
  ok( ret == 0 ,"Can't unlink '%s': %d\n", tempf, errno);

  tempf=_tempnam(".","wne");
  tempfd = _open(tempf,_O_CREAT|_O_TRUNC|_O_BINARY|_O_RDWR,0);
  ok( tempfd != -1,
     "Can't open '%s': %d\n", tempf, errno); /* open in BINARY mode */
  ok(_write(tempfd,dostext,strlen(dostext)) == lstrlenA(dostext),
     "_write _O_BINARY bad return value\n");
  _close(tempfd);
  tempfd = _open(tempf,_O_RDONLY|_O_BINARY,0); /* open in BINARY mode */
  ok(_read(tempfd,btext,LLEN) == lstrlenA(dostext),
     "_read _O_BINARY got bad length\n");
  ok( memcmp(dostext,btext,strlen(dostext)) == 0,
      "problems with _O_BINARY _write / _read\n");
  ok( btext[strlen(dostext)-2] == '\r', "CR not written or read\n");
  _close(tempfd);
  tempfd = _open(tempf,_O_RDONLY|_O_TEXT); /* open in TEXT mode */
  ok(_read(tempfd,btext,LLEN) == lstrlenA(mytext),
     "_read _O_TEXT got bad length\n");
  ok( memcmp(mytext,btext,strlen(mytext)) == 0,
      "problems with _O_BINARY _write / _O_TEXT _read\n");
  _close(tempfd);

   ret =_chmod (tempf, _S_IREAD | _S_IWRITE);
  ok( ret == 0,
     "Can't chmod '%s' to read-write: %d\n", tempf, errno);
  ret = unlink(tempf);
  ok( ret == 0 ,"Can't unlink '%s': %d\n", tempf, errno);
}

static void test_file_inherit_child(const char* fd_s)
{
    int fd = atoi(fd_s);
    char buffer[32];
    int ret;

    ret =write(fd, "Success", 8);
    ok( ret == 8, "Couldn't write in child process on %d (%s)\n", fd, strerror(errno));
    lseek(fd, 0, SEEK_SET);
    ok(read(fd, buffer, sizeof (buffer)) == 8, "Couldn't read back the data\n");
    ok(memcmp(buffer, "Success", 8) == 0, "Couldn't read back the data\n");
}

static void test_file_inherit_child_no(const char* fd_s)
{
    int fd = atoi(fd_s);
    int ret;

    ret = write(fd, "Success", 8);
    ok( ret == -1 && errno == EBADF, 
       "Wrong write result in child process on %d (%s)\n", fd, strerror(errno));
}
 
static void test_file_inherit( const char* selfname )
{
    int			fd;
    const char*		arg_v[5];
    char 		buffer[16];

    fd = open ("fdopen.tst", O_CREAT | O_RDWR | O_BINARY, _S_IREAD |_S_IWRITE);
    ok(fd != -1, "Couldn't create test file\n");
    arg_v[0] = selfname;
    arg_v[1] = "tests/file.c";
    arg_v[2] = buffer; sprintf(buffer, "%d", fd);
    arg_v[3] = 0;
    _spawnvp(_P_WAIT, selfname, arg_v);
    ok(tell(fd) == 8, "bad position %lu expecting 8\n", tell(fd));
    lseek(fd, 0, SEEK_SET);
    ok(read(fd, buffer, sizeof (buffer)) == 8 && memcmp(buffer, "Success", 8) == 0, "Couldn't read back the data\n");
    close (fd);
    ok(unlink("fdopen.tst") == 0, "Couldn't unlink\n");
    
    fd = open ("fdopen.tst", O_CREAT | O_RDWR | O_BINARY | O_NOINHERIT, _S_IREAD |_S_IWRITE);
    ok(fd != -1, "Couldn't create test file\n");
    arg_v[0] = selfname;
    arg_v[1] = "tests/file.c";
    arg_v[2] = buffer; sprintf(buffer, "%d", fd);
    arg_v[3] = buffer;
    arg_v[4] = 0;
    _spawnvp(_P_WAIT, selfname, arg_v);
    ok(tell(fd) == 0, "bad position %lu expecting 0\n", tell(fd));
    ok(read(fd, buffer, sizeof (buffer)) == 0, "Found unexpected data (%s)\n", buffer);
    close (fd);
    ok(unlink("fdopen.tst") == 0, "Couldn't unlink\n");
}

static void test_tmpnam( void )
{
  char name[MAX_PATH] = "abc";
  char *res;

  res = tmpnam(NULL);
  ok(res != NULL, "tmpnam returned NULL\n");
  ok(res[0] == '\\', "first character is not a backslash\n");
  ok(strchr(res+1, '\\') == 0, "file not in the root directory\n");
  ok(res[strlen(res)-1] == '.', "first call - last character is not a dot\n");

  res = tmpnam(name);
  ok(res != NULL, "tmpnam returned NULL\n");
  ok(res == name, "supplied buffer was not used\n");
  ok(res[0] == '\\', "first character is not a backslash\n");
  ok(strchr(res+1, '\\') == 0, "file not in the root directory\n");
  ok(res[strlen(res)-1] != '.', "second call - last character is a dot\n");
}

static void test_chsize( void )
{
    int fd;
    long cur, pos, count;
    char temptext[] = "012345678";
    char *tempfile = _tempnam( ".", "tst" );
    
    ok( tempfile != NULL, "Couldn't create test file: %s\n", tempfile );

    fd = _open( tempfile, _O_CREAT|_O_TRUNC|_O_RDWR, _S_IREAD|_S_IWRITE );
    ok( fd > 0, "Couldn't open test file\n" );

    count = _write( fd, temptext, sizeof(temptext) );
    ok( count > 0, "Couldn't write to test file\n" );

    /* get current file pointer */
    cur = _lseek( fd, 0, SEEK_CUR );

    /* make the file smaller */
    ok( _chsize( fd, sizeof(temptext) / 2 ) == 0, "_chsize() failed\n" );

    pos = _lseek( fd, 0, SEEK_CUR );
    ok( cur == pos, "File pointer changed from: %ld to: %ld\n", cur, pos );
    ok( _filelength( fd ) == sizeof(temptext) / 2, "Wrong file size\n" );

    /* enlarge the file */
    ok( _chsize( fd, sizeof(temptext) * 2 ) == 0, "_chsize() failed\n" ); 

    pos = _lseek( fd, 0, SEEK_CUR );
    ok( cur == pos, "File pointer changed from: %ld to: %ld\n", cur, pos );
    ok( _filelength( fd ) == sizeof(temptext) * 2, "Wrong file size\n" );

    _close( fd );
    _unlink( tempfile );
}

static void test_fopen_fclose_fcloseall( void )
{
    char fname1[] = "empty1";
    char fname2[] = "empty2";
    char fname3[] = "empty3";
    FILE *stream1, *stream2, *stream3, *stream4;
    int ret, numclosed;

    /* testing fopen() */
    stream1 = fopen(fname1, "w+");
    ok(stream1 != NULL, "The file '%s' was not opened\n", fname1);
    stream2 = fopen(fname2, "w ");
    ok(stream2 != NULL, "The file '%s' was not opened\n", fname2 );
    _unlink(fname3);
    stream3 = fopen(fname3, "r");
    ok(stream3 == NULL, "The file '%s' shouldn't exist before\n", fname3 );
    stream3 = fopen(fname3, "w+");
    ok(stream3 != NULL, "The file '%s' should be opened now\n", fname3 );
    errno = 0xfaceabad;
    stream4 = fopen("", "w+");
    ok(stream4 == NULL && errno == ENOENT, 
       "filename is empty, errno = %d (expected 2)\n", errno);
    errno = 0xfaceabad;
    stream4 = fopen(NULL, "w+");
    ok(stream4 == NULL && (errno == EINVAL || errno == ENOENT), 
       "filename is NULL, errno = %d (expected 2 or 22)\n", errno);

    /* testing fclose() */
    ret = fclose(stream2);
    ok(ret == 0, "The file '%s' was not closed\n", fname2);
    ret = fclose(stream3);
    ok(ret == 0, "The file '%s' was not closed\n", fname3);
    ret = fclose(stream2);
    ok(ret == EOF, "Closing file '%s' returned %d\n", fname2, ret);
    ret = fclose(stream3);
    ok(ret == EOF, "Closing file '%s' returned %d\n", fname3, ret);

    /* testing fcloseall() */
    numclosed = _fcloseall();
    /* fname1 should be closed here */
    ok(numclosed == 1, "Number of files closed by fcloseall(): %u\n", numclosed);
    numclosed = _fcloseall();
    ok(numclosed == 0, "Number of files closed by fcloseall(): %u\n", numclosed);

    ok(_unlink(fname1) == 0, "Couldn't unlink file named '%s'\n", fname1);
    ok(_unlink(fname2) == 0, "Couldn't unlink file named '%s'\n", fname2);
    ok(_unlink(fname3) == 0, "Couldn't unlink file named '%s'\n", fname3);
}

static void test_get_osfhandle(void)
{
    int fd;
    char fname[] = "t_get_osfhanle";
    DWORD bytes_written;
    HANDLE handle;

    fd = _sopen(fname, _O_CREAT|_O_RDWR, _SH_DENYRW, _S_IREAD | _S_IWRITE);
    handle = (HANDLE)_get_osfhandle(fd);
    WriteFile(handle, "bar", 3, &bytes_written, NULL);
    _close(fd);
    fd = _open(fname, _O_RDONLY, 0);
    ok(fd != -1, "Coudn't open '%s' after _get_osfhanle()\n", fname);

    _close(fd);
    _unlink(fname);
}

START_TEST(file)
{
    int arg_c;
    char** arg_v;

    arg_c = winetest_get_mainargs( &arg_v );

    /* testing low-level I/O */
    if (arg_c >= 3)
    {
        if (arg_c == 3) test_file_inherit_child(arg_v[2]); 
        else test_file_inherit_child_no(arg_v[2]);
        return;
    }
    test_file_inherit(arg_v[0]);
    test_file_write_read();
    test_chsize();

    /* testing stream I/O */
    test_fdopen();
    test_fopen_fclose_fcloseall();
    test_fileops();
    test_readmode(FALSE); /* binary mode */
    test_readmode(TRUE);  /* ascii mode */
    test_fgetwc();
    test_ctrlz();
    test_file_put_get();
    test_tmpnam();
    test_get_osfhandle();
}
