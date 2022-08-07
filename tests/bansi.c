/*
# Some basic ANSI terminal codes.
# Copyright 1932, jaggz.h at gmail
# Todo:
#  * The var/routine naming scheme probably sucks. I tried though.
#  * Re-add "disable" routines -- to wipe/reassign variables.
#  * Fix and advance the hackish higher color routines
#    (Only the 256 color set is even basically touched, so far).
# Notes:
#  No value bounds are placed on anything.  Up to the caller for now.
package bansi;

# perl version
# Gist url: https://gist.github.com/7b22252263719757e17ca710ac091110
*/
#define _IN_BANSI_C
#include "bansi.h"

#include <stdio.h>

// BG
char *bgbla=BGBLA; char *bgred=BGRED; char *bggre=BGGRE;
char *bgbro=BGBRO; char *bgblu=BGBLU; char *bgmag=BGMAG;
char *bgcya=BGCYA; char *bggra=BGGRA;
// FG
char *bla=BLA; char *red=RED; char *gre=GRE;
char *bro=BRO; char *blu=BLU; char *mag=MAG;
char *cya=CYA; char *gra=GRA;
// Bright FG (FG with intensity)
char *bbla=BBLA; char *bred=BRED; char *bgre=BGRE;
char *yel=YEL; char *bblu=BBLU; char *bmag=BMAG;
char *bcya=BCYA; char *whi=WHI;
// Other...
char *rst=RST; char *inv=INV;
char *cll=CLL; char *cllr=CLLR;
char *cls=CLS; char *clsb=CLSB;
char *ahome=AHOME;

void uncolor() {
	bgbla=bgred=bggre="";
	bgbro=bgblu=bgmag="";
	bgcya=bggra="";
	bla=red=gre="";
	bro=blu=mag="";
	cya=gra="";
	bbla=bred=bgre="";
	yel=bblu=bmag="";
	bcya=whi="";
	rst=inv="";
	cll=cllr=cls=clsb="";
	ahome="";
}

char *rotbuffull(void) { // color term sequence buffers
	static char bufs[COLOR_BUFS][COLOR_BUF_LEN];
	static int i=-1;
	if (++i >= COLOR_BUFS) i=0;
	return (char *)bufs[i];
}
char *rotbufi(void) {    // number-buffers only (6 digit max. no protection!)
	static char bufs[COLOR_BUFS][COLOR_BUF_I_LEN];
	static int i=-1;
	if (++i >= COLOR_BUFS) i=0;
	return (char *)bufs[i];
}

#if 0 // wip
// 0-5 color components for 256 color mode
char *fg_rgb5(float r, float g, float b) {
	/* warn "0-5 float value high " . join(',',@_) if ($_[0]>5 || $_[1]>5 || $_[2]>5); */
	/* 	# If they give us a negative number it's their fault... */
	char *s = rotbuffull();
	char *rs = rotbuffull();
	char pfx[] = "\033[38;5;";
	char *rgb5str = itoa(rgb5val(r, g, b), rs, 10);
	char *rgb5len = strlen(rgb5str);
	s[sizeof(pfx)+rgb5len-1]='m';
	s[sizeof(pfx)+rgb5len]=0;
	return s;
}
char *bg_rgb5(float r, float g, float b) {
	warn "0-5 float value high " . join(',',@_) if ($_[0]>5 || $_[1]>5 || $_[2]>5);
		// # If they give us a negative number it's their fault...
	"\033[48;5;" . rgb5val(@_) . "m";
}
char *fg_rgb5fl(float r, float g, float b) {
	warn "RGB float value high " . join(',',@_) if ($_[0]>1 || $_[1]>1 || $_[2]>1);
		// # If they give us a negative number it's their fault...
	"\033[38;5;" . rgb5val(   map { int($_*5) } @_   ) . "m";
}
char *bg_rgb5fl(float r, float g, float b) {
	warn "RGB float value high " . join(',',@_) if ($_[0]>1 || $_[1]>1 || $_[2]>1);
		// # If they give us a negative number it's their fault...
	"\033[48;5;" . rgb5val(   map { int($_*5) } @_   ) . "m";
}
int rgb5val(float r, float g, float b) {
	return (int)(16 + 36*r + 6*g + b);
}

// 0-23 colors of gray (23 is a reminder)
sub fggray_23 {
	"\033[38;5;" . gray_5_23_val($_[0]) . "m";
}
sub fggray_5fl {
	"\033[38;5;" . gray_5_fl_val($_[0]) . "m";
}
sub bggray_23 {
	"\033[48;5;" . gray_5_23_val($_[0]) . "m";
}
sub bggray_5fl {
	"\033[48;5;" . gray_5_fl_val($_[0]) . "m";
}

sub gray_5_fl_val {
	$_ = shift;
	if ($_ > 31) { $_ = 31; }
	elsif ($_ < 0) { $_ = 0; }
	232+int($_*31);
}

sub gray_5_23_val {
	$_ = shift;
	if ($_ > 31) { $_ = 31; }
	elsif ($_ < 0) { $_ = 0; }
	232+$_;
}
#endif

char *gotoxys(int x, int y) {
	char *s = rotbuffull();
	sprintf(s, "\033[%d;%dH", y, x);
	return s;
}

void gotoxy(int x, int y) {
	printf("\033[%d;%dH", y, x);
}

#if 0 // wip
sub ansilen {
	my $s = shift;
	$s =~ s/\033\[[0-9;]*[a-z]//ig;
	length($s);
}
#endif

void gright(int i) {
	if (i < 0) { fputs("\033[C", stdout); }
	else { printf("\033[%dC", i); }
}
void gleft(int i) {
	if (i < 0) { fputs("\033[D", stdout); }
	else { printf("\033[%dD", i); }
}
void gup(int i) {
	if (i < 0) { fputs("\033[A", stdout); }
	else { printf("\033[%dA", i); }
}
void gdown(int i) {
	if (i < 0) { fputs("\033[B", stdout); }
	else { printf("\033[%dB", i); }
}
