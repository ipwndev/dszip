/* gzip (GNU zip) -- compress files with zip algorithm and 'compress' interface
 * Copyright (C) 1992-1993 Jean-loup Gailly
 * The unzip code was written and put in the public domain by Mark Adler.
 * Portions of the lzw code are derived from the public domain 'compress'
 * written by Spencer Thomas, Joe Orost, James Woods, Jim McKie, Steve Davies,
 * Ken Turkowski, Dave Mack and Peter Jannesen.
 *
 * See the license_msg below and the file COPYING for the software license.
 * See the file algorithm.doc for the compression algorithms and file formats.
 */

/* Compress files with zip algorithm and 'compress' interface.
 * See usage() and help() functions below for all options.
 * Outputs:
 *        file.gz:   compressed file with same mode, owner, and utimes
 *     or stdout with -c option or if stdin used as input.
 * If the output file name had to be truncated, the original name is kept
 * in the compressed file.
 * On MSDOS, file.tmp -> file.tmz. On VMS, file.tmp -> file.tmp-gz.
 *
 * Using gz on MSDOS would create too many file name conflicts. For
 * example, foo.txt -> foo.tgz (.tgz must be reserved as shorthand for
 * tar.gz). Similarly, foo.dir and foo.doc would both be mapped to foo.dgz.
 * I also considered 12345678.txt -> 12345txt.gz but this truncates the name
 * too heavily. There is no ideal solution given the MSDOS 8+3 limitation. 
 *
 * For the meaning of all compilation flags, see comments in Makefile.in.
 */

#ifdef RCSID
static char rcsid[] = "$Id: gzip.c,v 0.24 1993/06/24 10:52:07 jloup Exp $";
#endif

#ifdef ARM9
#include <fat.h>
#include <sys/dir.h>
#include <nds.h>

#include <nds/arm9/console.h> //basic print funcionality
#endif


#include <ctype.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>

#include "tailor.h"
#include "gzip.h"
#include "lzw.h"
#include "revision.h"
#include "getopt.h"

long header_bytes;
		/* configuration */

#ifdef NO_TIME_H
#  include <sys/time.h>
#else
#  include <time.h>
#endif

#ifndef NO_FCNTL_H
#  include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#if defined(STDC_HEADERS) || !defined(NO_STDLIB_H)
#  include <stdlib.h>
#else
   extern int errno;
#endif

#if defined(DIRENT)
#  include <dirent.h>
   typedef struct dirent dir_type;
#  define NLENGTH(dirent) ((int)strlen((dirent)->d_name))
#  define DIR_OPT "DIRENT"
#else
#  define NLENGTH(dirent) ((dirent)->d_namlen)
#  ifdef SYSDIR
#    include <sys/dir.h>
     typedef struct direct dir_type;
#    define DIR_OPT "SYSDIR"
#  else
#    ifdef SYSNDIR
#      include <sys/ndir.h>
       typedef struct direct dir_type;
#      define DIR_OPT "SYSNDIR"
#    else
#      ifdef NDIR
#        include <ndir.h>
         typedef struct direct dir_type;
#        define DIR_OPT "NDIR"
#      else
#        define NO_DIR
#        define DIR_OPT "NO_DIR"
#      endif
#    endif
#  endif
#endif

#ifndef NO_UTIME
#  ifndef NO_UTIME_H
#    include <utime.h>
#    define TIME_OPT "UTIME"
#  else
#    ifdef HAVE_SYS_UTIME_H
#      include <sys/utime.h>
#      define TIME_OPT "SYS_UTIME"
#    else
       struct utimbuf {
         time_t actime;
         time_t modtime;
       };
#      define TIME_OPT ""
#    endif
#  endif
#else
#  define TIME_OPT "NO_UTIME"
#endif

#if !defined(S_ISDIR) && defined(S_IFDIR)
#  define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#if !defined(S_ISREG) && defined(S_IFREG)
#  define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

typedef RETSIGTYPE (*sig_type) OF((int));

#ifndef	O_BINARY
#  define  O_BINARY  0  /* creation mode for open() */
#endif

#ifndef O_CREAT
   /* Pure BSD system? */
#  include <sys/file.h>
#  ifndef O_CREAT
#    define O_CREAT FCREAT
#  endif
#  ifndef O_EXCL
#    define O_EXCL FEXCL
#  endif
#endif

#ifndef S_IRUSR
#  define S_IRUSR 0400
#endif
#ifndef S_IWUSR
#  define S_IWUSR 0200
#endif
#define RW_USER (S_IRUSR | S_IWUSR)  /* creation mode for open() */

#ifndef MAX_PATH_LEN
#  define MAX_PATH_LEN   1024 /* max pathname length */
#endif

#ifndef SEEK_END
#  define SEEK_END 2
#endif

#ifdef NO_OFF_T
  typedef long off_t;
  off_t lseek OF((int fd, off_t offset, int whence));
#endif

/* Separator for file name parts (see shorten_name()) */
#ifdef NO_MULTIPLE_DOTS
#  define PART_SEP "-"
#else
#  define PART_SEP "."
#endif

#ifndef false
#	define false (0)
#endif

		/* global buffers */

DECLARE(uch, inbuf,  INBUFSIZ +INBUF_EXTRA);
DECLARE(uch, outbuf, OUTBUFSIZ+OUTBUF_EXTRA);
DECLARE(ush, d_buf,  DIST_BUFSIZE);
DECLARE(uch, window, 2L*WSIZE);
#ifndef MAXSEG_64K
    DECLARE(ush, tab_prefix, 1L<<BITS);
#else
    DECLARE(ush, tab_prefix0, 1L<<(BITS-1));
    DECLARE(ush, tab_prefix1, 1L<<(BITS-1));
#endif

		/* local variables */

int ascii = 0;        /* convert end-of-lines to local OS conventions */
int to_stdout = 0;    /* output to stdout (-c) */
int decompress = 0;   /* decompress (-d) */
int force = 0;        /* don't ask questions, compress links (-f) */
int no_name = -1;     /* don't save or restore the original file name */
int no_time = -1;     /* don't save or restore the original file time */
int recursive = 0;    /* recurse through directories (-r) */
int list = 0;         /* list the file contents (-l) */
int verbose = 0;      /* be verbose (-v) */
int quiet = 0;        /* be very quiet (-q) */
int do_lzw = 0;       /* generate output compatible with old compress (-Z) */
int test = 0;         /* test .gz file integrity */
int foreground;       /* set if program run in foreground */
char *progname;       /* program name */
int maxbits = BITS;   /* max bits per code for LZW */
int method = DEFLATED;/* compression method */
int level = 6;        /* compression level */
int exit_code = OK;   /* program exit code */
int save_orig_name;   /* set if original name must be saved */
int last_member;      /* set for .zip and .Z files */
int part_nb;          /* number of parts in .gz file */
long time_stamp;      /* original time stamp (modification time) */
long ifile_size;      /* input file size, -1 for devices (debug only) */
char *env;            /* contents of GZIP env variable */
char **args = NULL;   /* argv pointer if GZIP env variable defined */
char z_suffix[MAX_SUFFIX+1]; /* default suffix (can be set with --suffix) */
int  z_len;           /* strlen(z_suffix) */

long bytes_in;             /* number of input bytes */
long bytes_out;            /* number of output bytes */
long total_in = 0;         /* input bytes for all files */
long total_out = 0;        /* output bytes for all files */
char ifname[MAX_PATH_LEN]; /* input file name */
char ofname[MAX_PATH_LEN]; /* output file name */
int  remove_ofname = 0;	   /* remove output file on error */
struct stat istat;         /* status for input file */
FILE  *ifd;                  /* input file descriptor */
FILE   *ofd;                  /* output file descriptor */
unsigned insize;           /* valid bytes in inbuf */
unsigned inptr;            /* index of next byte to be processed in inbuf */
unsigned outcnt;           /* bytes in output buffer */

struct option longopts[] =
{
 /* { name  has_arg  *flag  val } */
    {"ascii",      0, 0, 'a'}, /* ascii text mode */
    {"to-stdout",  0, 0, 'c'}, /* write output on standard output */
    {"stdout",     0, 0, 'c'}, /* write output on standard output */
    {"decompress", 0, 0, 'd'}, /* decompress */
    {"uncompress", 0, 0, 'd'}, /* decompress */
 /* {"encrypt",    0, 0, 'e'},    encrypt */
    {"force",      0, 0, 'f'}, /* force overwrite of output file */
    {"help",       0, 0, 'h'}, /* give help */
 /* {"pkzip",      0, 0, 'k'},    force output in pkzip format */
    {"list",       0, 0, 'l'}, /* list .gz file contents */
    {"license",    0, 0, 'L'}, /* display software license */
    {"no-name",    0, 0, 'n'}, /* don't save or restore original name & time */
    {"name",       0, 0, 'N'}, /* save or restore original name & time */
    {"quiet",      0, 0, 'q'}, /* quiet mode */
    {"silent",     0, 0, 'q'}, /* quiet mode */
    {"recursive",  0, 0, 'r'}, /* recurse through directories */
    {"suffix",     1, 0, 'S'}, /* use given suffix instead of .gz */
    {"test",       0, 0, 't'}, /* test compressed file integrity */
    {"no-time",    0, 0, 'T'}, /* don't save or restore the time stamp */
    {"verbose",    0, 0, 'v'}, /* verbose mode */
    {"version",    0, 0, 'V'}, /* display version number */
    {"fast",       0, 0, '1'}, /* compress faster */
    {"best",       0, 0, '9'}, /* compress better */
    {"lzw",        0, 0, 'Z'}, /* make output compatible with old compress */
    {"bits",       1, 0, 'b'}, /* max number of bits per code (implies -Z) */
    { 0, 0, 0, 0 }
};

/* local functions */

local void treat_file   OF((const char *iname));
local int create_outfile OF((void));
local int  get_method   OF((FILE * in));
local void do_exit      OF((int exitcode));
      int main          OF((int argc, char **argv));
int (*work) OF((FILE * infile, FILE * outfile)) = zip; /* function to call */

#ifndef NO_DIR
local void treat_dir    OF((char *dir));
#endif
#ifndef NO_UTIME
//local void reset_times  OF((char *name, struct stat *statb));
#endif

#define strequ(s1, s2) (strcmp((s1),(s2)) == 0)


/* ======================================================================== */
int do_decompression (const char *file_name, const char *output_file_name)
{
    int file_count;     /* number of files to precess */
	
	progname = "dsZip";
	
	strcpy(ifname, file_name);
	strcpy(ofname, output_file_name);

	// Force decompression of one file only
    decompress = 1;
	file_count = 1;

    /* Allocate all global buffers (for DYN_ALLOC option) */
    ALLOC(uch, inbuf,  INBUFSIZ +INBUF_EXTRA);
    ALLOC(uch, outbuf, OUTBUFSIZ+OUTBUF_EXTRA);
    ALLOC(ush, d_buf,  DIST_BUFSIZE);
    ALLOC(uch, window, 2L*WSIZE);
#ifndef MAXSEG_64K
    ALLOC(ush, tab_prefix, 1L<<BITS);
#else
    ALLOC(ush, tab_prefix0, 1L<<(BITS-1));
    ALLOC(ush, tab_prefix1, 1L<<(BITS-1));
#endif

    treat_file(file_name); // parameter is not used
    return exit_code; /* just to avoid lint warning */
}


/* ========================================================================
 * Compress or decompress the given file
 */
local void treat_file(const char *iname)
{

#if 0
// FIXME ->
    /* Generate output file name. For -r and (-t or -l), skip files
     * without a valid gzip suffix (check done in make_ofname).
     */
    if (to_stdout && !list && !test) {
		strcpy(ofname, "stdout");
    } else if (make_ofname() != OK) {
		return;
    }
// <- FIXME
#else
//	strcpy(ofname, "prova.txt");
//	strcpy(ifname, "prova.txt.gz");
#endif /* #if 0 */

//    ifd = OPEN(ifname, ascii && !decompress ? O_RDONLY : O_RDONLY | O_BINARY,
//	       RW_USER);
	ifd = fopen(ifname, "r");
	
    if (!ifd) {
		iprintf("%s: error while opening input file", progname);
		exit_code = ERROR;
		return;
    }
    clear_bufs(); /* clear input and output buffers */
    part_nb = 0;

    if (decompress) {
		method = get_method(ifd); /* updates ofname if original given */
		if (method < 0) {
			fclose(ifd);
			return;               /* error message already emitted */
		}
    }

    /* If compressing to a file, check if ofname is not ambiguous
     * because the operating system truncates names. Otherwise, generate
     * a new ofname and save the original name in the compressed file.
     */

	if (create_outfile() != OK) return;

	if (!decompress && save_orig_name && !verbose && !quiet) {
	    iprintf("%s: %s compressed to %s\n",
		    progname, ifname, ofname);
	}

    /* Keep the name even if not truncated except with --no-name: */
    if (!save_orig_name)
		save_orig_name = !no_name;

//	iprintf("%s:\t%s", ifname, (int)strlen(ifname) >= 15 ? 
//		"" : ((int)strlen(ifname) >= 7 ? "\t" : "\t\t"));
    

    /* Actually do the compression/decompression. Loop over zipped members.
     */
    for (;;) {
		if ((*work)(ifd, ofd) != OK) {
			method = -1; /* force cleanup */
			break;
		}

		if (!decompress || last_member || inptr == insize)
			break;
		/* end of file */

		method = get_method(ifd);
		if (method < 0)
			break;				/* error message already emitted */
		bytes_out = 0;          /* required for length check */
    }

    fclose(ifd);
    if (!to_stdout && fclose(ofd)) {
		write_error();
    }

    if (method == -1) {
//		if (!to_stdout) unlink (ofname);
		return;
    }
}

local int create_outfile()
{
	/* Create the output file */
	remove_ofname = 1;
	ofd = fopen(ofname, "w");
	if (!ofd) {
		iprintf("errore apertura output\n");
		fclose(ifd);
		exit_code = ERROR;
		return ERROR;
	}
	return OK;
}




local int get_method(FILE *in)
{
    uch flags;     /* compression flags */
    char magic[2]; /* magic header */
    ulg stamp;     /* time stamp */
	
    /* If --force and --stdout, zcat == cat, so do not complain about
		* premature end of file: use try_byte instead of get_byte.
		*/
	magic[0] = (char)get_byte();
	magic[1] = (char)get_byte();
	
    method = -1;                 /* unknown yet */
    part_nb++;                   /* number of parts in gzip file */
    header_bytes = 0;
    last_member = RECORD_IO;
    /* assume multiple members in gzip file except for record oriented I/O */
	
    if (memcmp(magic, GZIP_MAGIC, 2) == 0
        || memcmp(magic, OLD_GZIP_MAGIC, 2) == 0) {
		
		method = (int)get_byte();
		if (method != DEFLATED) {
			iprintf("%s: %s: unknown method %d -- get newer version of gzip\n",
					progname, ifname, method);
			exit_code = ERROR;
			return -1;
		}
		work = unzip;
		flags  = (uch)get_byte();
		
		if ((flags & ENCRYPTED) != 0) {
			iprintf("%s: %s is encrypted -- get newer version of gzip\n",
					progname, ifname);
			exit_code = ERROR;
			return -1;
		}
		
		if ((flags & CONTINUATION) != 0) {
			iprintf("%s: %s is a a multi-part gzip file -- get newer version of gzip\n",
					progname, ifname);
			exit_code = ERROR;
			if (force <= 1) return -1;
		}
		
		
		if ((flags & RESERVED) != 0) {
			iprintf("%s: %s has flags 0x%x -- get newer version of gzip\n",
					progname, ifname, flags);
			exit_code = ERROR;
			if (force <= 1) return -1;
		}
		stamp  = (ulg)get_byte();
		stamp |= ((ulg)get_byte()) << 8;
		stamp |= ((ulg)get_byte()) << 16;
		stamp |= ((ulg)get_byte()) << 24;
		if (stamp != 0 && !no_time) time_stamp = stamp;
		
		(void)get_byte();  /* Ignore extra flags for the moment */
		(void)get_byte();  /* Ignore OS type for the moment */
		
		if ((flags & CONTINUATION) != 0) {
			unsigned part = (unsigned)get_byte();
			part |= ((unsigned)get_byte())<<8;
			if (verbose) {
				iprintf("%s: %s: part number %u\n",
						progname, ifname, part);
			}
		}
		if ((flags & EXTRA_FIELD) != 0) {
			unsigned len = (unsigned)get_byte();
			len |= ((unsigned)get_byte())<<8;
			if (verbose) {
				iprintf("%s: %s: extra field of %u bytes ignored\n",
						progname, ifname, len);
			}
			while (len--) (void)get_byte();
		}


	/* Get original file name if it was truncated */
	if ((flags & ORIG_NAME) != 0) {
	    if (no_name || (to_stdout && !list) || part_nb > 1) {
		/* Discard the old name */
		char c; /* dummy used for NeXTstep 3.0 cc optimizer bug */
		do {c=get_byte();} while (c != 0);
	    } else {
		/* Copy the base name. Keep a directory prefix intact. */
//                char *p = basename(ofname);
 //               char *base = p;
		char p;
		for (;;) {
		    p = (char)get_char();
		    if (p == '\0') break;
//		    if (p >= ofname+sizeof(ofname)) {
//			error("corrupted input -- file name too large");
//		    }
		}
  //              /* If necessary, adapt the name to local OS conventions: */
  //              if (!list) {
  //                 MAKE_LEGAL_NAME(base);
//		   if (base) list=0; /* avoid warning about unused variable */
 //               }
	    } /* no_name || to_stdout */
	} /* ORIG_NAME */



		
		/* Discard file comment if any */
		if ((flags & COMMENT) != 0) {
			while (get_char() != 0) /* null */ ;
		}
		if (part_nb == 1) {
			header_bytes = inptr + 2*sizeof(long); /* include crc and size */
		}
		
    } else if (memcmp(magic, PKZIP_MAGIC, 2) == 0 && inptr == 2
			   && memcmp((char*)inbuf, PKZIP_MAGIC, 4) == 0) {
		/* To simplify the code, we support a zip file when alone only.
		* We are thus guaranteed that the entire local header fits in inbuf.
		*/
        inptr = 0;
		work = unzip;
		
		if (check_zipfile(in) != OK) return -1;
		/* check_zipfile may get ofname from the local header */
		last_member = 1;
		
    } else if (memcmp(magic, PACK_MAGIC, 2) == 0) {
		work = unpack;
		method = PPACKED;
		
    } else if (memcmp(magic, LZW_MAGIC, 2) == 0) {
		work = unlzw;
		method = COMPRESSED;
		last_member = 1;
		
    } else if (memcmp(magic, LZH_MAGIC, 2) == 0) {
		work = unlzh;
		method = LZHED;
		last_member = 1;
		
    } else if (force && to_stdout && !list) { /* pass input unchanged */
		method = STORED;
		work = copy;
        inptr = 0;
		last_member = 1;
    }
	
    if (method >= 0) return method;
	
    if (part_nb == 1) {
		iprintf("\n%s: %s: not in gzip format\n", progname, ifname);
		exit_code = ERROR;
		return -1;
    } else {
		iprintf("\n%s: %s: decompression OK, trailing garbage ignored\n",
				progname, ifname);
		return -2;
    }
}

#ifndef NO_DIR
local void treat_dir(dir)
    char *dir;
{
}
#endif /* ? NO_DIR */

local void do_exit(exitcode)
    int exitcode;
{
    static int in_exit = 0;

    if (in_exit) exit(exitcode);
    in_exit = 1;
    if (env != NULL)  free(env),  env  = NULL;
    if (args != NULL) free((char*)args), args = NULL;
    FREE(inbuf);
    FREE(outbuf);
    FREE(d_buf);
    FREE(window);
#ifndef MAXSEG_64K
    FREE(tab_prefix);
#else
    FREE(tab_prefix0);
    FREE(tab_prefix1);
#endif
    exit(exitcode);
}

/* ========================================================================
 * Signal and error handler.
 */
RETSIGTYPE abort_gzip()
{
   if (remove_ofname) {
       fclose(ofd);
//       unlink (ofname);
   }
   do_exit(ERROR);
}
