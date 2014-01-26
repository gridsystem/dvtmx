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
#define BLUE            (COLORS==256 ? 68 : COLOR_BLUE)
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

#define BAR_SEP_L_CHAR "["
#define BAR_SEP_R_CHAR "]"

/* separator between window title and window number */
#define SEPARATOR " | "
/* printf format string for the window title, first %s
 * is replaced by the title, second %s is replaced by
 * the SEPARATOR, %d stands for the window number */
#define TITLE "[%s%s#%d]"
/* master width factor [0.1 .. 0.9] */
#define MFACT 0.5
/* scroll back buffer size in lines */
#define SCROLL_HISTORY 500
/* printf format string for the tag in the status bar */
#define TAG_SYMBOL   "[%s]"
/* printf format string for the layout in the status bar */
#define LAYOUT_SYMBOL   " %s "
/* curses attributes for the currently selected tags */
#define TAG_SEL      COLOR(BLUE, -1) | A_BOLD
/* curses attributes for not selected tags which contain no windows */
#define TAG_NORMAL   A_NORMAL
/* curses attributes for not selected tags which contain windows */
#define TAG_OCCUPIED COLOR(BLUE, -1)

const char tags[][8] = { "1", "2", "3", "4", "5" };

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
	{ "[]=", tile },
	{ "+++", grid },
	{ "TTT", bstack },
	{ "[ ]", fullscreen },
};

#define MOD  CTRL('g')
#define MOD2 CTRL('t')
#define MOD3 CTRL('v')
#define MOD4 CTRL('r')

/* you can at most specifiy MAX_ARGS (3) number of arguments */
static Key keys[] = {
	{ MOD, 'c',       { create,         { NULL }                    } },
	{ MOD, 'C',       { create,         { NULL, NULL, "$CWD" }      } },
	{ MOD, 'x',       { killclient,     { NULL }                    } },
	{ MOD, 'j',       { focusnext,      { NULL }                    } },
	{ MOD, 'u',       { focusnextnm,    { NULL }                    } },
	{ MOD, 'i',       { focusprevnm,    { NULL }                    } },
	{ MOD, 'k',       { focusprev,      { NULL }                    } },
	{ MOD, 't',       { setlayout,      { "[]=" }                   } },
	{ MOD, 'g',       { setlayout,      { "+++" }                   } },
	{ MOD, 'b',       { setlayout,      { "TTT" }                   } },
	{ MOD, 'm',       { setlayout,      { "[ ]" }                   } },
	{ MOD, ' ',       { setlayout,      { NULL }                    } },
	{ MOD, 'h',       { setmfact,       { "-0.05" }                 } },
	{ MOD, 'l',       { setmfact,       { "+0.05" }                 } },
	{ MOD, '.',       { toggleminimize, { NULL }                    } },
	{ MOD, 's',       { togglebar,      { NULL }                    } },
	{ MOD, 'M',       { togglemouse,    { NULL }                    } },
	{ MOD, '\n',      { zoom ,          { NULL }                    } },
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
	{ MOD, 'a',       { togglerunall,   { NULL }                    } },
	{ MOD, 'r',       { redraw,         { NULL }                    } },
	{ MOD, 'X',       { lock,           { NULL }                    } },
	{ MOD, 'B',       { togglebell,     { NULL }                    } },
	{ MOD, 'v',       { copymode,       { NULL }                    } },
	{ MOD, '/',       { copymode,       { "/" }                     } },
	{ MOD, '?',       { copymode,       { "?" }                     } },
	{ MOD, 'p',       { paste,          { NULL }                    } },
	{ MOD, KEY_PPAGE, { scrollback,     { "-1" }                    } },
	{ MOD, KEY_NPAGE, { scrollback,     { "1"  }                    } },
	{ MOD, MOD,       { send,           { (const char []){MOD, 0} } } },
	{ MOD, 'H',       { create,         { "man dvtm", "dvtm help" } } },

	{ MOD, '\t',      { viewprevtag,    { NULL }                    } },
	{ MOD, '0',       { view,           { NULL }                    } },
	{ MOD, KEY_F(1),  { view,           { tags[0] }                 } },
	{ MOD, KEY_F(2),  { view,           { tags[1] }                 } },
	{ MOD, KEY_F(3),  { view,           { tags[2] }                 } },
	{ MOD, KEY_F(4),  { view,           { tags[3] }                 } },
	{ MOD, KEY_F(5),  { view,           { tags[4] }                 } },

	{ MOD2, '0',      { tag,            { NULL }                    } },
	{ MOD2, KEY_F(1), { tag,            { tags[0] }                 } },
	{ MOD2, KEY_F(2), { tag,            { tags[1] }                 } },
	{ MOD2, KEY_F(3), { tag,            { tags[2] }                 } },
	{ MOD2, KEY_F(4), { tag,            { tags[3] }                 } },
	{ MOD2, KEY_F(5), { tag,            { tags[4] }                 } },

	{ MOD3, KEY_F(1), { toggleview,     { tags[0] }                 } },
	{ MOD3, KEY_F(2), { toggleview,     { tags[1] }                 } },
	{ MOD3, KEY_F(3), { toggleview,     { tags[2] }                 } },
	{ MOD3, KEY_F(4), { toggleview,     { tags[3] }                 } },
	{ MOD3, KEY_F(5), { toggleview,     { tags[4] }                 } },

	{ MOD4, KEY_F(1), { toggletag,      { tags[0] }                 } },
	{ MOD4, KEY_F(2), { toggletag,      { tags[1] }                 } },
	{ MOD4, KEY_F(3), { toggletag,      { tags[2] }                 } },
	{ MOD4, KEY_F(4), { toggletag,      { tags[3] }                 } },
	{ MOD4, KEY_F(5), { toggletag,      { tags[4] }                 } },
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

/* possible values for the mouse buttons are listed below:
 *
 * BUTTON1_PRESSED          mouse button 1 down
 * BUTTON1_RELEASED         mouse button 1 up
 * BUTTON1_CLICKED          mouse button 1 clicked
 * BUTTON1_DOUBLE_CLICKED   mouse button 1 double clicked
 * BUTTON1_TRIPLE_CLICKED   mouse button 1 triple clicked
 * BUTTON2_PRESSED          mouse button 2 down
 * BUTTON2_RELEASED         mouse button 2 up
 * BUTTON2_CLICKED          mouse button 2 clicked
 * BUTTON2_DOUBLE_CLICKED   mouse button 2 double clicked
 * BUTTON2_TRIPLE_CLICKED   mouse button 2 triple clicked
 * BUTTON3_PRESSED          mouse button 3 down
 * BUTTON3_RELEASED         mouse button 3 up
 * BUTTON3_CLICKED          mouse button 3 clicked
 * BUTTON3_DOUBLE_CLICKED   mouse button 3 double clicked
 * BUTTON3_TRIPLE_CLICKED   mouse button 3 triple clicked
 * BUTTON4_PRESSED          mouse button 4 down
 * BUTTON4_RELEASED         mouse button 4 up
 * BUTTON4_CLICKED          mouse button 4 clicked
 * BUTTON4_DOUBLE_CLICKED   mouse button 4 double clicked
 * BUTTON4_TRIPLE_CLICKED   mouse button 4 triple clicked
 * BUTTON_SHIFT             shift was down during button state change
 * BUTTON_CTRL              control was down during button state change
 * BUTTON_ALT               alt was down during button state change
 * ALL_MOUSE_EVENTS         report all button state changes
 * REPORT_MOUSE_POSITION    report mouse movement
 */

#ifdef NCURSES_MOUSE_VERSION
# define CONFIG_MOUSE /* compile in mouse support if we build against ncurses */
#endif

#define ENABLE_MOUSE true /* whether to enable mouse events by default */

#ifdef CONFIG_MOUSE
static Button buttons[] = {
	{ BUTTON1_CLICKED,        { mouse_focus,      { NULL  } } },
	{ BUTTON1_DOUBLE_CLICKED, { mouse_fullscreen, { "[ ]" } } },
	{ BUTTON2_CLICKED,        { mouse_zoom,       { NULL  } } },
	{ BUTTON3_CLICKED,        { mouse_minimize,   { NULL  } } },
};
#endif /* CONFIG_MOUSE */

static Cmd commands[] = {
	{ "create", { create,	{ NULL } } },
};

/* gets executed when dvtm is started */
static Action actions[] = {
	{ create, { NULL } },
};

static char const * const keytable[] = {
	/* add your custom key escape sequences */
};
