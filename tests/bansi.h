#ifndef _BANSI_H
#define _BANSI_H

#define COLOR_BUFS    30
#define COLOR_BUF_LEN   15
#define COLOR_BUF_I_LEN 7

#ifndef _IN_BANSI_C
// left separate so they can be reorganized if someone wants
extern char *bgbla; extern char *bgred; extern char *bggre;
extern char *bgbla; extern char *bgred; extern char *bggre;
extern char *bgbro; extern char *bgblu; extern char *bgmag;
extern char *bgcya; extern char *bggra;
// FG
extern char *bla; extern char *red; extern char *gre;
extern char *bro; extern char *blu; extern char *mag;
extern char *cya; extern char *gra;
// Bright FG (FG with intensity)
extern char *bbla; extern char *bred; extern char *bgre;
extern char *yel; extern char *bblu; extern char *bmag;
extern char *bcya; extern char *whi;
// Other...
extern char *rst; extern char *inv;
extern char *cll; extern char *cllr;
extern char *cls; extern char *clsb;
extern char *ahome;
#endif // _IN_BANSI_C

// These are useful for: "C compile-time string" BRED ". concatenation" RST
// Otherwise, probably use the lowercase variables
// BG
#define BGBLA "\033[40m"
#define BGRED "\033[41m"
#define BGGRE "\033[42m"
#define BGBRO "\033[43m"
#define BGBLU "\033[44m"
#define BGMAG "\033[45m"
#define BGCYA "\033[46m"
#define BGGRA "\033[47m"
// FG
#define BLA "\033[30m"
#define RED "\033[31m"
#define GRE "\033[32m"
#define BRO "\033[33m"
#define BLU "\033[34m"
#define MAG "\033[35m"
#define CYA "\033[36m"
#define GRA "\033[37m"
// Bright FG (FG with intensity)
#define BBLA "\033[30;1m"
#define BRED "\033[31;1m"
#define BGRE "\033[32;1m"
#define YEL "\033[33;1m"
#define BBLU "\033[34;1m"
#define BMAG "\033[35;1m"
#define BCYA "\033[36;1m"
#define WHI "\033[37;1m"
// Other...
#define RST "\033[0m"
#define INV "\033[7m"
#define CLL "\033[2K"
#define CLLR "\033[2K"
#define CLS "\033[2J"
#define CLSB "\033[J"
#define AHOME   "\033[1H"

void uncolor(); // remove colors (no putting them back!)
char *rotbuffull(void);
char *rotbufi(void);    // number-buffers only (6 digit max. no protection!)

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

# 0-23 colors of gray (23 is a reminder)
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

char *gotoxys(int x, int y);
void gotoxy(int x, int y);

#if 0 // wip
sub ansilen();
#endif

void gright(int i);
void gleft(int i);
void gup(int i);
void gdown(int i);
#endif // _BANSI_H
