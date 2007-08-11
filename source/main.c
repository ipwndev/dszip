/* main.c -- handle dsZip user interface.
 * Copyright (C) 2007 lanugo
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 *
 * The code in this file is derived from the file funzip.c written
 * and put in the public domain by Mark Adler.
 */
#include <fat.h>
#include <sys/dir.h>
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <nds/arm9/console.h> //basic print funcionality

#define MAX_FILE_PER_DIR	512		// maximum number of directory entries
#define MAX_DISPLAY			16		// maximum number of entries per screen (scroll if you want more)
#define MAX_FILENAME_LEN	256		// maximum filename length

struct stat st[MAX_FILE_PER_DIR];					// stat structures (to know where we are)
char filename[MAX_FILE_PER_DIR][MAX_FILENAME_LEN];	// to hold a full filename and string terminator
char dirname[MAX_FILENAME_LEN];						// name of the current directory
unsigned entry_no, current_no;						// number of directory entries, currently selected entry.
DIR_ITER* dir = NULL;

extern int do_decompression(const char *input, const char *output);
void list_dir(const char *name);
void print_dir(void);
void do_selected(void);

//---------------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------------

	// initialise the irq dispatcher
	irqInit();
	// a vblank interrupt is needed to use swiWaitForVBlank()
	// since the dispatcher handles the flags no handler is required
	irqEnable(IRQ_VBLANK);

	videoSetMode(0);
	videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE);
	vramSetBankC(VRAM_C_SUB_BG);

	SUB_BG0_CR = BG_MAP_BASE(31);

	BG_PALETTE_SUB[255] = RGB15(31,31,31);

	consoleInitDefault((u16*)SCREEN_BASE_BLOCK_SUB(31), (u16*)CHAR_BASE_BLOCK_SUB(0), 16);

	if(!fatInitDefault()) {
		iprintf("Unable to initialize libfat\n");
		return 0;
	}
	
	iprintf("dsZip 0.01 - PRE ALPHA\n");

	list_dir("/");
	print_dir();

	while(1) {		
		scanKeys();
		u32 keys = keysDown();

//		if ( keys & KEY_L ) ; 
//		if ( keys & KEY_R ) ;

//		if ( keys & KEY_LEFT );
//		if ( keys & KEY_RIGHT ) ;
		if ( keys & KEY_UP ) {
			if(current_no > 0) {
				--current_no;
				print_dir();
			}
		}
				
		if ( keys & KEY_DOWN ) {
			if(current_no + 1 < entry_no) {
				++current_no;
				print_dir();
			}
		}

		if ( keys & KEY_A )
			do_selected();

		if ( keys & KEY_B ) {
			list_dir("..");
			print_dir();
		}

//		if( keys & KEY_X ) ;
//		if( keys & KEY_Y ) ;
		
		swiWaitForVBlank();
	}
	
	return 0;
}

void list_dir(const char * name)
{
	strcpy(dirname, name);

	chdir(name);
	
	if(dir != NULL) 
		dirclose(dir);

	dir = diropen (".");

	if (dir == NULL) {
		iprintf ("Unable to open the directory.\n");
		exit(1);
	} else {
		entry_no = 0;
		current_no = 0;
		while (dirnext(dir, filename[entry_no], &st[entry_no]) == 0) {
			if(st[entry_no].st_mode & S_IFDIR || strstr(filename[entry_no], ".gz") ||
				strstr(filename[entry_no], ".GZ"))
				++entry_no;
		}
		++entry_no;
	}
}


void print_dir(void)
{
	unsigned i;
	
	// ansi escape sequence to clear screen and home cursor
	// /x1b[line;columnH
	iprintf("\x1b[2J");

	for(i = current_no; i < entry_no && i - current_no < MAX_DISPLAY; ++i) {
			if(i == current_no)
				iprintf("-> ");
			else
				iprintf("   ");

			// st.st_mode & S_IFDIR indicates a directory
			
			char *tag;
			if(st[i].st_mode & S_IFDIR)
				tag = " DIR";
			else if(strstr(filename[i], ".gz") || strstr(filename[i], ".GZ"))
				tag = "  GZ";
			else {
//				tag = "FILE";
				continue; // we do _not_ print file that are no compressed archives
			}
			
			iprintf ("%s: %.20s\n", tag, filename[i]);
	}
}

void do_selected(void)
{
	char output_name[MAX_FILENAME_LEN];
	char *ext;

	if(st[current_no].st_mode & S_IFDIR) {
		list_dir(filename[current_no]);
		print_dir();
	} else {
		iprintf("\x1b[2J");	// clear the screen
		iprintf("file selected: %s\n", filename[current_no]);
		strcpy(output_name, filename[current_no]);
		if(!(ext = strstr(output_name, ".gz")) && !(ext = strstr(output_name, ".GZ")))
			iprintf("ERROR: wrong filename/type. Only .gz file are supported\n");
		else {
			int result;
			*ext = '\0';	// erase the .gz extension
			iprintf("output name: %s\n", output_name);
			iprintf("beginning decompression...\n");
			result = do_decompression(filename[current_no], output_name);
			iprintf("decompression finished, result: %d\n", result);
		}
		
		iprintf("PLEASE REBOOT YOUR NINTENDO DS\n");
		
		for(;;)
			swiWaitForVBlank();
	}
	return;
}

void print_progress(void)
{
	iprintf(".");
	return;
}

