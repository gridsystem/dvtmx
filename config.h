/* valid curses attributes are listed below they can be ORed
 *
 * A_NORMAL        Normal display (no highlight)
 * A_STANDOUT      Best highlighting mode of the terminal.
 * A_UNDERLINE     Underlining
 * A_REVERSE       Reverse video
 * A_BLINK         Blinking
 * A_DIM           Half bright
 * A_BOLD          Extra bright or bold
 * A_PROTECT       Protected mode
 * A_INVIS         Invisible or blank mode
 */
#define BLUE            (COLOR_BLUE)
/* curses attributes for the currently focused window */
#define SELECTED_ATTR   COLOR(BLUE, -1) | A_NORMAL
/* curses attributes for normal (not selected) windows */
#define NORMAL_ATTR     COLOR(-1, -1) | A_NORMAL
/* curses attributes for the status bar */
#define BAR_ATTR        COLOR(BLUE, -1) | A_NORMAL
/* status bar (command line option -s) position */
#define BAR_POS		BAR_TOP /* BAR_BOTTOM, BAR_OFF */
/* determines whether the statusbar text should be right or left aligned */
#define BAR_ALIGN       ALIGN_RIGHT

#define BAR_SEP_L_CHAR ' '
#define BAR_SEP_R_CHAR ' '

/* separator between window title and window number */
#define SEPARATOR "  "
/* printf format string for the window title, first %s
 * is replaced by the title, second %s is replaced by
 * the SEPARATOR, %d stands for the window number */
#define TITLE " %s%s#%d "
/* master width factor [0.1 .. 0.9] */
#define MFACT 0.5
/* scroll back buffer size in lines */
#define SCROLL_HISTORY 500
/* printf format string for the tag in the status bar */
#define TAG_SYMBOL   "%s"
/* curses attributes for the currently selected tags */
#define TAG_SEL      COLOR(BLUE, -1) | A_BOLD
/* curses attributes for not selected tags which contain no windows */
#define TAG_NORMAL   A_NORMAL
/* curses attributes for not selected tags which contain windows */
#define TAG_OCCUPIED COLOR(BLUE, -1)

const char tags[][8] = { "╸", "╸"};

/*
 * http://www.mkssoftware.com/docs/man3/curs_addch.3.asp
 *
 * ACS_CKBOARD   halftone block
 * ACS_VLINE     vertical line
 * ACS_HLINE     horizontal line
 * ACS_TTEE      corner - top tee
 * ACS_LTEE      corner - left tee
 * ACS_RTEE      corner - right tee
 * ACS_PLUS      corner - plus
 */

#define THEME_VERT_CHAR    ACS_CKBOARD
#define THEME_HORI_CHAR    ACS_CKBOARD
#define THEME_TT_CHAR      ACS_CKBOARD
#define THEME_LT_CHAR      ACS_CKBOARD
#define THEME_RT_CHAR      ACS_CKBOARD
#define THEME_PLUS_CHAR    ACS_CKBOARD
#define THEME_STATUS_CHAR  ACS_CKBOARD

#include "tile.c"
#include "grid.c"
#include "bstack.c"
#include "fullscreen.c"

/* by default the first layout entry is used */
static Layout layouts[] = {
	{ "╦", bstack },
	{ "╠", tile },
	{ "╬", grid },
	{ "◊", fullscreen }
};

#define MOD  CTRL('d')

/* you can at most specifiy MAX_ARGS (3) number of arguments */
static Key keys[] = {
	{ MOD, 'n',       { create,         { NULL }                    } },
	{ MOD, 'N',       { create,         { NULL, NULL, "$CWD" }      } },
	{ MOD, 'w',       { killclient,     { NULL }                    } },
	{ MOD, ' ',       { setlayout,      { NULL }                    } },
	{ MOD, '\n',      { zoom ,          { NULL }                    } },
	{ MOD, 'h',       { setmfact,       { "-0.05" }                 } },
	{ MOD, 'l',       { setmfact,       { "+0.05" }                 } },
	{ MOD, '.',       { toggleminimize, { NULL }                    } },
	{ MOD, 's',       { togglebar,      { NULL }                    } },
	
	{ MOD, 'j',       { focusnext,      { NULL }                    } },
	{ MOD, 'k',       { focusprev,      { NULL }                    } },
	{ MOD, '1',       { focusn,         { "1" }                     } },
	{ MOD, '2',       { focusn,         { "2" }                     } },
	{ MOD, '3',       { focusn,         { "3" }                     } },
	{ MOD, '4',       { focusn,         { "4" }                     } },
	{ MOD, '5',       { focusn,         { "5" }                     } },
	{ MOD, '6',       { focusn,         { "6" }                     } },
	{ MOD, '7',       { focusn,         { "7" }                     } },
	{ MOD, '8',       { focusn,         { "8" }                     } },
	{ MOD, '9',       { focusn,         { "9" }                     } },
	
	{ MOD, 'q',       { quit,           { NULL }                    } },
	{ MOD, 'r',       { redraw,         { NULL }                    } },
	{ MOD, 'X',       { lock,           { NULL }                    } },
	
	{ MOD, 'a',       { togglerunall,   { NULL }                    } },
	{ MOD, 'v',       { copymode,       { NULL }                    } },
	{ MOD, '/',       { copymode,       { "/" }                     } },
	{ MOD, '?',       { copymode,       { "?" }                     } },
	{ MOD, 'p',       { paste,          { NULL }                    } },
	{ MOD, MOD,       { send,           { (const char []){MOD, 0} } } },
	{ MOD, 'H',       { create,         { "man dvtm", "dvtm help" } } },

	{ MOD, 'u',       { view,           { tags[0] }                 } },
	{ MOD, 'i',       { view,           { tags[1] }                 } },
	{ MOD, 'U',       { tag,            { tags[0] }                 } },
	{ MOD, 'I',       { tag,            { tags[1] }                 } },

	{ MOD, KEY_PPAGE, { scrollback,     { "-1" }                    } },
	{ MOD, KEY_NPAGE, { scrollback,     { "1"  }                    } },

	{ NOMOD, KEY_SPREVIOUS, { scrollback, { "-1" }                  } },
	{ NOMOD, KEY_SNEXT,     { scrollback, { "1"  }                  } },
};

static const ColorRule colorrules[] = {
	{ "", A_NORMAL, -1, -1 }, /* default */
#if 0
	/* title attrs     fgcolor      bgcolor */
	{ "ssh", A_NORMAL, COLOR_BLACK, 224      },
#endif
};

#define ENABLE_MOUSE false /* whether to enable mouse events by default */

static Cmd commands[] = {
	{ "create", { create,	{ NULL } } },
};

/* gets executed when dvtm is started */
static Action actions[] = {
	{ create, { NULL } },
};

static char const * const keytable[] = {};
