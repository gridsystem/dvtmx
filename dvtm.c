/*
 * The initial "port" of dwm to curses was done by
 *
 * © 2007-2013 Marc André Tanner <mat at brain-dump dot org>
 *
 * It is highly inspired by the original X11 dwm and
 * reuses some code of it which is mostly
 *
 * © 2006-2007 Anselm R. Garbe <garbeam at gmail dot com>
 *
 * See LICENSE for details.
 */

#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <curses.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <locale.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#ifdef __CYGWIN__
# include <termios.h>
#endif
#include "vt.h"

#ifdef PDCURSES
int ESCDELAY;
#endif

#ifndef NCURSES_REENTRANT
# define set_escdelay(d) (ESCDELAY = (d))
#endif

typedef struct {
	float mfact;
	int history;
	int w;
	int h;
	bool need_resize;
} Screen;

typedef struct {
	const char *symbol;
	void (*arrange)(void);
} Layout;

typedef struct Client Client;
struct Client {
	WINDOW *window;
	Vt *term;
	const char *cmd;
	char title[255];
	int order;
	pid_t pid;
	int pty;
	unsigned short int id;
	unsigned short int x;
	unsigned short int y;
	unsigned short int w;
	unsigned short int h;
	bool minimized;
	bool died;
	Client *next;
	Client *prev;
	Client *snext;
	bool tags[1];
};

typedef struct {
	const char *title;
	unsigned attrs;
	short fg;
	short bg;
} ColorRule;

#define ALT(k)      ((k) + (161 - 'a'))
#if defined CTRL && defined _AIX
  #undef CTRL
#endif
#ifndef CTRL
  #define CTRL(k)   ((k) & 0x1F)
#endif
#define CTRL_ALT(k) ((k) + (129 - 'a'))

#define MAX_ARGS 3

typedef struct {
	void (*cmd)(const char *args[]);
	/* needed to avoid an error about initialization
	 * of nested flexible array members */
	const char *args[MAX_ARGS];
} Action;

typedef struct {
	unsigned int mod;
	unsigned int code;
	Action action;
} Key;

typedef struct {
	mmask_t mask;
	Action action;
} Button;

typedef struct {
	const char *name;
	Action action;
} Cmd;

enum { BAR_TOP, BAR_BOTTOM, BAR_OFF };
enum { ALIGN_LEFT, ALIGN_RIGHT };

typedef struct {
	int fd;
	int pos;
	unsigned short int h;
	unsigned short int y;
	char text[512];
	const char *file;
} StatusBar;

typedef struct {
	int fd;
	const char *file;
	unsigned short int id;
} CmdFifo;

#define countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#define sstrlen(str) (sizeof(str) - 1)
#define max(x, y) ((x) > (y) ? (x) : (y))

#ifdef NDEBUG
 #define debug(format, args...)
#else
 #define debug eprint
#endif

/* commands for use by keybindings */
static void create(const char *args[]);
static void copymode(const char *args[]);
static void focusn(const char *args[]);
static void focusnext(const char *args[]);
static void focusnextnm(const char *args[]);
static void focusprev(const char *args[]);
static void focusprevnm(const char *args[]);
static void killclient(const char *args[]);
static void lock(const char *key[]);
static void paste(const char *args[]);
static void quit(const char *args[]);
static void redraw(const char *args[]);
static void scrollback(const char *args[]);
static void send(const char *args[]);
static void setlayout(const char *args[]);
static void setmfact(const char *args[]);
static void startup(const char *args[]);
static void tag(const char *args[]);
static void togglebar(const char *args[]);
static void togglebell(const char *key[]);
static void toggleminimize(const char *args[]);
static void togglemouse(const char *args[]);
static void togglerunall(const char *args[]);
static void toggletag(const char *args[]);
static void toggleview(const char *args[]);
static void viewprevtag(const char *args[]);
static void view(const char *args[]);
static void zoom(const char *args[]);

/* commands for use by mouse bindings */
static void mouse_focus(const char *args[]);
static void mouse_fullscreen(const char *args[]);
static void mouse_minimize(const char *args[]);
static void mouse_zoom(const char *args[]);

/* functions and variables available to layouts via config.h */
static Client* nextvisible(Client *c);
static void focus(Client *c);
static void resize(Client *c, int x, int y, int w, int h);
extern Screen screen;
static unsigned int waw, wah, wax, way;
static Client *clients = NULL;
static char *title;
#define COLOR(fg, bg) COLOR_PAIR(vt_color_reserve(fg, bg))
#define NOMOD ERR

#include "config.h"

/* global variables */
Screen screen = { MFACT, SCROLL_HISTORY };
static Client *stack = NULL;
static Client *sel = NULL;
static Client *msel = NULL;
bool seltags[countof(tags)] = {[0] = true};
bool prevtags[countof(tags)];
static bool mouse_events_enabled = ENABLE_MOUSE;
static Layout *layout = layouts;
static StatusBar bar = { -1, BAR_POS, 1 };
static CmdFifo cmdfifo = { -1 };
static const char *shell;
static char *copybuf;
static volatile sig_atomic_t running = true;
static bool runinall = false;

static void
eprint(const char *errstr, ...) {
	va_list ap;
	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
}

static void
error(const char *errstr, ...) {
	va_list ap;
	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

static bool
isarrange(void (*func)()) {
	return func == layout->arrange;
}

static bool
isvisible(Client *c) {
	for (unsigned int i = 0; i < countof(tags); i++)
		if (c->tags[i] && seltags[i])
			return true;
	return false;
}

static bool
is_content_visible(Client *c) {
	if (!c)
		return false;
	if (isarrange(fullscreen))
		return sel == c;
	return isvisible(c) && !c->minimized;
}

static Client*
nextvisible(Client *c) {
	for (; c && !isvisible(c); c = c->next);
	return c;
}

static Client*
nextbytag(Client *c, int tag) {
	for (; c && !c->tags[tag]; c = c->next);
	return c;
}

bool
isoccupied(unsigned int t) {
	for (Client *c = clients; c; c = c->next)
		if (c->tags[t])
			return true;
	return false;
}

static void
reorder(int tag) {
	Client *c;
	uint8_t order = 0;
	if (tag < 0)
		for (c = nextvisible(clients); c; c = nextvisible(c->next))
			c->order = ++order;
	else
		for (c = nextbytag(clients, tag); c; c = nextbytag(c->next, tag))
			c->order = ++order;
}

static void
drawbar() {
	int sx, sy, x = 0;
	if (bar.pos == BAR_OFF)
		return;
	getyx(stdscr, sy, sx);
	
	move(bar.y, 0);
	for (unsigned int i = 0; i < countof(tags); i++){
		if (seltags[i])
			attrset(TAG_SEL);
		else if (isoccupied(i))
			attrset(TAG_OCCUPIED);
		else
			attrset(TAG_NORMAL);
		printw(TAG_SYMBOL, tags[i]);
		/* -2 because we assume %s is contained in TAG_SYMBOL */
		x = x + ( mbstowcs(NULL, TAG_SYMBOL, 0) - 2
			+ mbstowcs(NULL, tags[i], 0) );
	}
	
	attrset(BAR_ATTR);
	addstr(BAR_SEP_L_CHAR);
	x = x + strlen(BAR_SEP_L_CHAR); 
	
	attrset(BAR_ATTR);
	printw(LAYOUT_SYMBOL, layout->symbol);
	x = x + strlen(layout->symbol) + 2;
	
	wchar_t wbuf[sizeof bar.text];
	int w, maxwidth = screen.w - x;
	
	if (mbstowcs(wbuf, bar.text, sizeof bar.text) == (size_t)-1)
		return;
	if ((w = wcswidth(wbuf, maxwidth)) == -1)
		return;
	
	if (BAR_ALIGN == ALIGN_RIGHT) {
		for (int i = 0; i + w < maxwidth; i++)
			addch(THEME_STATUS_CHAR);
	}
	
	addstr(bar.text);
	
	if (BAR_ALIGN == ALIGN_LEFT) {
		for (; w < maxwidth; w++)
			addch(THEME_STATUS_CHAR);
	}
	
	mvaddstr(bar.y, screen.w - 1, BAR_SEP_R_CHAR);
	
	attrset(NORMAL_ATTR);
	move(sy, sx);
	wnoutrefresh(stdscr);
}

static void
draw_border(Client *c) {
	char t = '\0';
	int x, y, maxlen;

	wattrset(c->window, (sel == c || (runinall && !c->minimized)) ? SELECTED_ATTR : NORMAL_ATTR);
	getyx(c->window, y, x);
	mvwhline(c->window, 0, 0, THEME_HORI_CHAR, c->w);
	maxlen = c->w - (2 + sstrlen(TITLE) - sstrlen("%s%sd")  + sstrlen(SEPARATOR) + 2);
	if (maxlen < 0)
		maxlen = 0;
	if ((size_t)maxlen < sizeof(c->title)) {
		t = c->title[maxlen];
		c->title[maxlen] = '\0';
	}
	
	mvwprintw(c->window, 0, 2, TITLE,
	          *c->title ? c->title : "",
	          *c->title ? SEPARATOR : "",
	          c->order);
	if (t)
		c->title[maxlen] = t;
	wmove(c->window, y, x);
}

static void
draw_content(Client *c) {
	vt_draw(c->term, c->window, 1, 0);
}

static void
draw(Client *c) {
	if (is_content_visible(c)) {
		redrawwin(c->window);
		draw_content(c);
	}
	if (!isarrange(fullscreen) || sel == c)
		draw_border(c);
	wnoutrefresh(c->window);
}

static void
draw_all() {
	Client *c;

	if (!nextvisible(clients)) {
		sel = NULL;
		erase();
		drawbar();
		doupdate();
		return;
	}

	if (!isarrange(fullscreen)) {
		for (c = nextvisible(clients); c; c = nextvisible(c->next)) {
			if (c == sel)
				continue;
			draw(c);
		}
	}
	/* as a last step the selected window is redrawn,
	 * this has the effect that the cursor position is
	 * accurate
	 */
	if (sel)
		draw(sel);
}

static void
arrange() {
	erase();
	attrset(NORMAL_ATTR);
	layout->arrange();
	reorder(-1);
	focus(NULL);
	wnoutrefresh(stdscr);
	drawbar();
	draw_all();
}

static void
attach(Client *c) {
	if (clients)
		clients->prev = c;
	c->next = clients;
	c->prev = NULL;
	clients = c;
	for (int o = 1; c; c = nextvisible(c->next), o++)
		c->order = o;
}

static void
attachafter(Client *c, Client *a) { /* attach c after a */
	if (c == a)
		return;
	if (!a)
		for (a = clients; a && a->next; a = a->next);

	if (a) {
		if (a->next)
			a->next->prev = c;
		c->next = a->next;
		c->prev = a;
		a->next = c;
		for (int o = a->order; c; c = nextvisible(c->next))
			c->order = ++o;
	}
}

static void
attachstack(Client *c) {
	c->snext = stack;
	stack = c;
}

static void
detach(Client *c) {
	Client *d;
	if (c->prev)
		c->prev->next = c->next;
	if (c->next) {
		c->next->prev = c->prev;
		for (d = nextvisible(c->next); d; d = nextvisible(d->next))
			--d->order;
	}
	if (c == clients)
		clients = c->next;
	c->next = c->prev = NULL;
}

static void
settitle(Client *c) {
	char *term, *t = title;
	if (!t && sel == c && *c->title)
		t = c->title;
	if (t && (term = getenv("TERM")) && !strstr(term, "linux"))
		printf("\033]0;%s\007", t);
}

static void
detachstack(Client *c) {
	Client **tc;
	for (tc=&stack; *tc && *tc != c; tc=&(*tc)->snext);
	*tc = c->snext;
}

static void
focus(Client *c) {
	Client *tmp = sel;
	if (!c)
		for (c = stack; c && !isvisible(c); c = c->snext);
	if (sel == c)
		return;
	sel = c;
	if (tmp && !isarrange(fullscreen)) {
		draw_border(tmp);
		wnoutrefresh(tmp->window);
	}

	if (c) {
		detachstack(c);
		attachstack(c);
		settitle(c);
		if (isarrange(fullscreen)) {
			draw(c);
		} else {
			draw_border(c);
			wnoutrefresh(c->window);
		}
	}
	curs_set(c && !c->minimized && vt_cursor(c->term));
}

static void
applycolorrules(Client *c) {
	const ColorRule *r = colorrules;
	short fg = r->fg, bg = r->bg;
	unsigned attrs = r->attrs;

	for (unsigned int i = 1; i < countof(colorrules); i++) {
		r = &colorrules[i];
		if (strstr(c->title, r->title)) {
			attrs = r->attrs;
			fg = r->fg;
			bg = r->bg;
			break;
		}
	}

	vt_set_default_colors(c->term, attrs, fg, bg);
}

static void
term_event_handler(Vt *term, int event, void *event_data) {
	Client *c = (Client *)vt_get_data(term);
	switch (event) {
	case VT_EVENT_TITLE:
		if (event_data)
			strncpy(c->title, event_data, sizeof(c->title) - 1);
		c->title[event_data ? sizeof(c->title) - 1 : 0] = '\0';
		settitle(c);
		if (!isarrange(fullscreen) || sel == c)
			draw_border(c);
		applycolorrules(c);
		break;
	case VT_EVENT_COPY_TEXT:
		if (event_data) {
			free(copybuf);
			copybuf = event_data;
		}
		break;
	}
}

static void
move_client(Client *c, int x, int y) {
	if (c->x == x && c->y == y)
		return;
	debug("moving, x: %d y: %d\n", x, y);
	if (mvwin(c->window, y, x) == ERR)
		eprint("error moving, x: %d y: %d\n", x, y);
	else {
		c->x = x;
		c->y = y;
	}
}

static void
resize_client(Client *c, int w, int h) {
	if (c->w == w && c->h == h)
		return;
	debug("resizing, w: %d h: %d\n", w, h);
	if (wresize(c->window, h, w) == ERR)
		eprint("error resizing, w: %d h: %d\n", w, h);
	else {
		c->w = w;
		c->h = h;
	}
	vt_resize(c->term, h - 1, w);
}

static void
resize(Client *c, int x, int y, int w, int h) {
	resize_client(c, w, h);
	move_client(c, x, y);
}

static Client*
get_client_by_pid(pid_t pid) {
	Client *c;
	for (c = clients; c; c = c->next) {
		if (c->pid == pid)
			return c;
	}
	return NULL;
}

static Client*
get_client_by_coord(unsigned int x, unsigned int y) {
	Client *c;
	if (y < way || y >= wah)
		return NULL;
	if (isarrange(fullscreen))
		return sel;
	for (c = nextvisible(clients); c; c = nextvisible(c->next)) {
		if (x >= c->x && x < c->x + c->w && y >= c->y && y < c->y + c->h) {
			debug("mouse event, x: %d y: %d client: %d\n", x, y, c->order);
			return c;
		}
	}
	return NULL;
}

static void
sigchld_handler(int sig) {
	int errsv = errno;
	int status;
	pid_t pid;
	Client *c;

	while ((pid = waitpid(-1, &status, WNOHANG)) != 0) {
		if (pid == -1) {
			if (errno == ECHILD) {
				/* no more child processes */
				break;
			}
			eprint("waitpid: %s\n", strerror(errno));
			break;
		}
		debug("child with pid %d died\n", pid);
		if ((c = get_client_by_pid(pid)))
			c->died = true;
	}

	errno = errsv;
}

static void
sigwinch_handler(int sig) {
	screen.need_resize = true;
}

static void
sigterm_handler(int sig) {
	running = false;
}

static void
updatebarpos(void) {
	bar.y = 0;
	wax = 0;
	way = 0;
	wah = screen.h;
	if (bar.pos == BAR_TOP) {
		wah -= bar.h;
		way += bar.h;
	} else if (bar.pos == BAR_BOTTOM) {
		wah -= bar.h;
		bar.y = wah;
	}
}

static void
resize_screen() {
	struct winsize ws;

	if (ioctl(0, TIOCGWINSZ, &ws) == -1) {
		getmaxyx(stdscr, screen.h, screen.w);
	} else {
		screen.w = ws.ws_col;
		screen.h = ws.ws_row;
	}

	debug("resize_screen(), w: %d h: %d\n", screen.w, screen.h);

	resizeterm(screen.h, screen.w);
	wresize(stdscr, screen.h, screen.w);

	waw = screen.w;
	wah = screen.h;
	updatebarpos();
	clear();
	arrange();
}

static bool
is_modifier(unsigned int mod) {
	unsigned int i;
	for (i = 0; i < countof(keys); i++) {
		if (keys[i].mod == mod)
			return true;
	}
	return false;
}

static Key*
keybinding(unsigned int mod, unsigned int code) {
	unsigned int i;
	for (i = 0; i < countof(keys); i++) {
		if (keys[i].mod == mod && keys[i].code == code)
			return &keys[i];
	}
	return NULL;
}

static unsigned int
idxoftag(const char *tag) {
	unsigned int i;
	for (i = 0; (i < countof(tags)) && (tags[i] != tag); i++);
	return (i < countof(tags)) ? i : 0;
}

static void
tagschanged() {
	Client *c;
	bool nm = false;
	for (c = nextvisible(clients); c; c = nextvisible(c->next)) {
		if (!c->minimized) {
			nm = true;
			break;
		}
	}
	if (!nm && nextvisible(clients)) {
		focus(NULL);
		toggleminimize(NULL);
	} else
		arrange();
}

static void
tag(const char *args[]) {
	unsigned int i;

	if (!sel)
		return;
	for (i = 0; i < countof(tags); i++)
		sel->tags[i] = (NULL == args[0]);
	i = idxoftag(args[0]);
	sel->tags[i] = true;
	reorder(i);
	tagschanged();
}

static void
toggletag(const char *args[]) {
	unsigned int i, j;

	if (!sel)
		return;
	i = idxoftag(args[0]);
	sel->tags[i] = !sel->tags[i];
	for (j = 0; j < countof(tags) && !sel->tags[j]; j++);
	if (j == countof(tags))
		sel->tags[i] = true; /* at least one tag must be enabled */
	tagschanged();
}

static void
toggleview(const char *args[]) {
	unsigned int i, j;

	i = idxoftag(args[0]);
	seltags[i] = !seltags[i];
	for (j = 0; j < countof(tags) && !seltags[j]; j++);
	if (j == countof(tags))
		seltags[i] = true; /* at least one tag must be viewed */
	tagschanged();
}

static void
view(const char *args[]) {
	memcpy(prevtags, seltags, sizeof seltags);
	for (unsigned int i = 0; i < countof(tags); i++)
		seltags[i] = (NULL == args[0]);
	seltags[idxoftag(args[0])] = true;
	tagschanged();
}

static void
viewprevtag(const char *args[]) {
	static bool tmp[countof(tags)];
	memcpy(tmp, seltags, sizeof seltags);
	memcpy(seltags, prevtags, sizeof seltags);
	memcpy(prevtags, tmp, sizeof seltags);
	tagschanged();
}

static void
keypress(int code) {
	Client *c;
	unsigned int len = 1;
	char buf[8] = { '\e' };

	if (code == '\e') {
		/* pass characters following escape to the underlying app */
		nodelay(stdscr, TRUE);
		for (int t; len < sizeof(buf) && (t = getch()) != ERR; len++)
			buf[len] = t;
		nodelay(stdscr, FALSE);
	}

	for (c = runinall ? nextvisible(clients) : sel; c; c = nextvisible(c->next)) {
		if (is_content_visible(c)) {
			if (code == '\e')
				vt_write(c->term, buf, len);
			else
				vt_keypress(c->term, code);
		}
		if (!runinall)
			break;
	}
}

static void
mouse_setup() {
#ifdef CONFIG_MOUSE
	mmask_t mask = 0;

	if (mouse_events_enabled) {
		mask = BUTTON1_CLICKED | BUTTON2_CLICKED;
		for (unsigned int i = 0; i < countof(buttons); i++)
			mask |= buttons[i].mask;
	}
	mousemask(mask, NULL);
#endif /* CONFIG_MOUSE */
}

static void
setup() {
	if (!(shell = getenv("SHELL")))
		shell = "/bin/sh";
	setlocale(LC_CTYPE, "");
	initscr();
	start_color();
	noecho();
	keypad(stdscr, TRUE);
	mouse_setup();
	raw();
	vt_init();
	vt_set_keytable(keytable, countof(keytable));
	resize_screen();
	struct sigaction sa;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = sigwinch_handler;
	sigaction(SIGWINCH, &sa, NULL);
	sa.sa_handler = sigchld_handler;
	sigaction(SIGCHLD, &sa, NULL);
	sa.sa_handler = sigterm_handler;
	sigaction(SIGTERM, &sa, NULL);
}

static void
destroy(Client *c) {
	Client *t;
	if (sel == c)
		focusnextnm(NULL);
	detach(c);
	detachstack(c);
	if (sel == c) {
		if ((t = nextvisible(clients))) {
			focus(t);
			toggleminimize(NULL);
		} else
			sel = NULL;
	}
	werase(c->window);
	wnoutrefresh(c->window);
	vt_destroy(c->term);
	delwin(c->window);
	if (!clients && countof(actions)) {
		if (!strcmp(c->cmd, shell))
			quit(NULL);
		else
			create(NULL);
	}
	free(c);
	arrange();
}

static void
cleanup() {
	while (clients)
		destroy(clients);
	vt_shutdown();
	endwin();
	free(copybuf);
	if (bar.fd > 0)
		close(bar.fd);
	if (bar.file)
		unlink(bar.file);
	if (cmdfifo.fd > 0)
		close(cmdfifo.fd);
	if (cmdfifo.file)
		unlink(cmdfifo.file);
}

static char *getcwd_by_pid(Client *c) {
	if (!c)
		return NULL;
	char buf[32];
	snprintf(buf, sizeof buf, "/proc/%d/cwd", c->pid);
	return realpath(buf, NULL);
}

/* commands for use by keybindings */
static void
create(const char *args[]) {
	Client *c = calloc(1, sizeof(Client) + sizeof(seltags));
	if (!c)
		return;
	memcpy(c->tags, seltags, sizeof seltags);
	const char *cmd = (args && args[0]) ? args[0] : shell;
	const char *pargs[] = { "/bin/sh", "-c", cmd, NULL };
	c->id = ++cmdfifo.id;
	char buf[8], *cwd = NULL;
	snprintf(buf, sizeof buf, "%d", c->id);
	const char *env[] = {
		"DVTM", VERSION,
		"DVTM_WINDOW_ID", buf,
		NULL
	};

	if (!(c->window = newwin(wah, waw, way, wax))) {
		free(c);
		return;
	}

	if (!(c->term = vt_create(screen.h - 1, screen.w, screen.history))) {
		delwin(c->window);
		free(c);
		return;
	}

	c->cmd = cmd;
	if (args && args[1]) {
		strncpy(c->title, args[1], sizeof(c->title) - 1);
		c->title[sizeof(c->title) - 1] = '\0';
	}
	if (args && args[2])
		cwd = !strcmp(args[2], "$CWD") ? getcwd_by_pid(sel) : (char*)args[2];
	c->pid = vt_forkpty(c->term, "/bin/sh", pargs, cwd, env, &c->pty);
	if (args && args[2] && !strcmp(args[2], "$CWD"))
		free(cwd);
	vt_set_data(c->term, c);
	vt_set_event_handler(c->term, term_event_handler);
	c->w = screen.w;
	c->h = screen.h;
	c->x = wax;
	c->y = way;
	c->order = 0;
	c->minimized = false;
	debug("client with pid %d forked\n", c->pid);
	attach(c);
	focus(c);
	arrange();
}

static void
copymode(const char *args[]) {
	if (!sel)
		return;
	vt_copymode_enter(sel->term);
	if (args[0]) {
		vt_copymode_keypress(sel->term, args[0][0]);
		draw(sel);
	}
}

static void
focusn(const char *args[]) {
	Client *c;

	for (c = nextvisible(clients); c; c = nextvisible(c->next)) {
		if (c->order == atoi(args[0])) {
			focus(c);
			if (c->minimized)
				toggleminimize(NULL);
			return;
		}
	}
}

static void
focusnext(const char *args[]) {
	Client *c;

	if (!sel)
		return;

	for (c = sel->next; c && !isvisible(c); c = c->next);
	if (!c)
		for (c = clients; c && !isvisible(c); c = c->next);
	if (c)
		focus(c);
}

static void
focusnextnm(const char *args[]) {
	Client *c;

	if (!sel)
		return;
	c = sel;
	do {
		c = nextvisible(c->next);
		if (!c)
			c = nextvisible(clients);
	} while (c->minimized && c != sel);
	focus(c);
}

static void
focusprev(const char *args[]) {
	Client *c;

	if (!sel)
		return;
	for (c = sel->prev; c && !isvisible(c); c = c->prev);
	if (!c) {
		for (c = clients; c && c->next; c = c->next);
		for (; c && !isvisible(c); c = c->prev);
	}
	if (c)
		focus(c);
}

static void
focusprevnm(const char *args[]) {
	Client *c;

	if (!sel)
		return;
	c = sel;
	do {
		for (c = c->prev; c && !isvisible(c); c = c->prev);
		if (!c) {
			for (c = clients; c && c->next; c = c->next);
			for (; c && !isvisible(c); c = c->prev);
		}
	} while (c->minimized && c != sel);
	focus(c);
}

static void
killclient(const char *args[]) {
	if (!sel)
		return;
	debug("killing client with pid: %d\n", sel->pid);
	kill(-sel->pid, SIGKILL);
}

static void
lock(const char *args[]) {
	size_t len = 0, i = 0;
	char buf[16], *pass = buf;
	int c;

	erase();
	curs_set(0);

	if (args && args[0]) {
		len = strlen(args[0]);
		pass = (char *)args[0];
	} else {
		mvprintw(LINES / 2, COLS / 2 - 7, "Enter password");
		while (len < sizeof buf && (c = getch()) != '\n')
			if (c != ERR)
				buf[len++] = c;
	}

	mvprintw(LINES / 2, COLS / 2 - 7, "Screen locked!");

	while (i != len) {
		for(i = 0; i < len; i++) {
			if (getch() != pass[i])
				break;
		}
	}

	arrange();
}

static void
paste(const char *args[]) {
	if (sel && copybuf)
		vt_write(sel->term, copybuf, strlen(copybuf));
}

static void
quit(const char *args[]) {
	cleanup();
	exit(EXIT_SUCCESS);
}

static void
redraw(const char *args[]) {
	for (Client *c = clients; c; c = c->next) {
		if (!c->minimized) {
			vt_dirty(c->term);
			wclear(c->window);
			wnoutrefresh(c->window);
		}
	}
	resize_screen();
}

static void
scrollback(const char *args[]) {
	if (!sel) return;

	if (!args[0] || atoi(args[0]) < 0)
		vt_scroll(sel->term, -sel->h/2);
	else
		vt_scroll(sel->term,  sel->h/2);

	draw(sel);
}

static void
send(const char *args[]) {
	if (sel && args && args[0])
		vt_write(sel->term, args[0], strlen(args[0]));
}

static void
setlayout(const char *args[]) {
	unsigned int i;

	if (!args || !args[0]) {
		if (++layout == &layouts[countof(layouts)])
			layout = &layouts[0];
	} else {
		for (i = 0; i < countof(layouts); i++)
			if (!strcmp(args[0], layouts[i].symbol))
				break;
		if (i == countof(layouts))
			return;
		layout = &layouts[i];
	}
	arrange();
}

static void
setmfact(const char *args[]) {
	float delta;

	if (isarrange(fullscreen) || isarrange(grid))
		return;
	/* arg handling, manipulate mfact */
	if (args[0] == NULL)
		screen.mfact = MFACT;
	else if (1 == sscanf(args[0], "%f", &delta)) {
		if (args[0][0] == '+' || args[0][0] == '-')
			screen.mfact += delta;
		else
			screen.mfact = delta;
		if (screen.mfact < 0.1)
			screen.mfact = 0.1;
		else if (screen.mfact > 0.9)
			screen.mfact = 0.9;
	}
	arrange();
}

static void
startup(const char *args[]) {
	for (unsigned int i = 0; i < countof(actions); i++)
		actions[i].cmd(actions[i].args);
}

static void
togglebar(const char *args[]) {
	if (bar.pos == BAR_OFF)
		bar.pos = (BAR_POS == BAR_OFF) ? BAR_TOP : BAR_POS;
	else
		bar.pos = BAR_OFF;
	updatebarpos();
	arrange();
}

static void
togglebell(const char *args[]) {
	vt_togglebell(sel->term);
}

static void
toggleminimize(const char *args[]) {
	Client *c, *m, *t;
	unsigned int n;
	if (!sel)
		return;
	/* the last window can't be minimized */
	if (!sel->minimized) {
		for (n = 0, c = nextvisible(clients); c; c = nextvisible(c->next))
			if (!c->minimized)
				n++;
		if (n == 1)
			return;
	}
	sel->minimized = !sel->minimized;
	m = sel;
	/* check whether the master client was minimized */
	if (sel == nextvisible(clients) && sel->minimized) {
		c = nextvisible(sel->next);
		detach(c);
		attach(c);
		focus(c);
		detach(m);
		for (; c && (t = nextvisible(c->next)) && !t->minimized; c = t);
		attachafter(m, c);
	} else if (m->minimized) {
		/* non master window got minimized move it above all other
		 * minimized ones */
		focusnextnm(NULL);
		detach(m);
		for (c = nextvisible(clients); c && (t = nextvisible(c->next)) && !t->minimized; c = t);
		attachafter(m, c);
	} else { /* window is no longer minimized, move it to the master area */
		vt_dirty(m->term);
		detach(m);
		attach(m);
	}
	arrange();
}

static void
togglemouse(const char *args[]) {
	mouse_events_enabled = !mouse_events_enabled;
	mouse_setup();
}

static void
togglerunall(const char *args[]) {
	runinall = !runinall;
	draw_all();
}

static void
zoom(const char *args[]) {
	Client *c;

	if (!sel)
		return;
	if ((c = sel) == nextvisible(clients))
		if (!(c = nextvisible(c->next)))
			return;
	detach(c);
	attach(c);
	focus(c);
	if (c->minimized)
		toggleminimize(NULL);
	arrange();
}

/* commands for use by mouse bindings */
static void
mouse_focus(const char *args[]) {
	focus(msel);
	if (msel->minimized)
		toggleminimize(NULL);
}

static void
mouse_fullscreen(const char *args[]) {
	mouse_focus(NULL);
	if (isarrange(fullscreen))
		setlayout(NULL);
	else
		setlayout(args);
}

static void
mouse_minimize(const char *args[]) {
	focus(msel);
	toggleminimize(NULL);
}

static void
mouse_zoom(const char *args[]) {
	focus(msel);
	zoom(NULL);
}

static Cmd *
get_cmd_by_name(const char *name) {
	for (unsigned int i = 0; i < countof(commands); i++) {
		if (!strcmp(name, commands[i].name))
			return &commands[i];
	}
	return NULL;
}

static void
handle_cmdfifo() {
	int r;
	char *p, *s, cmdbuf[512], c;
	Cmd *cmd;
	switch (r = read(cmdfifo.fd, cmdbuf, sizeof cmdbuf - 1)) {
	case -1:
	case 0:
		cmdfifo.fd = -1;
		break;
	default:
		cmdbuf[r] = '\0';
		p = cmdbuf;
		while (*p) {
			/* find the command name */
			for (; *p == ' ' || *p == '\n'; p++);
			for (s = p; *p && *p != ' ' && *p != '\n'; p++);
			if ((c = *p))
				*p++ = '\0';
			if (*s && (cmd = get_cmd_by_name(s)) != NULL) {
				bool quote = false;
				int argc = 0;
				/* XXX: initializer assumes MAX_ARGS == 2 use a initialization loop? */
				const char *args[MAX_ARGS] = { NULL, NULL, NULL}, *arg;
				/* if arguments were specified in config.h ignore the one given via
				 * the named pipe and thus skip everything until we find a new line
				 */
				if (cmd->action.args[0] || c == '\n') {
					debug("execute %s", s);
					cmd->action.cmd(cmd->action.args);
					while (*p && *p != '\n')
						p++;
					continue;
				}
				/* no arguments were given in config.h so we parse the command line */
				while (*p == ' ')
					p++;
				arg = p;
				for (; (c = *p); p++) {
					switch (*p) {
					case '\\':
						/* remove the escape character '\\' move every
						 * following character to the left by one position
						 */
						switch (p[1]) {
							case '\\':
							case '\'':
							case '\"': {
								char *t = p+1;
								do {
									t[-1] = *t;
								} while (*t++);
							}
						}
						break;
					case '\'':
					case '\"':
						quote = !quote;
						break;
					case ' ':
						if (!quote) {
					case '\n':
							/* remove trailing quote if there is one */
							if (*(p - 1) == '\'' || *(p - 1) == '\"')
								*(p - 1) = '\0';
							*p++ = '\0';
							/* remove leading quote if there is one */
							if (*arg == '\'' || *arg == '\"')
								arg++;
							if (argc < MAX_ARGS)
								args[argc++] = arg;

							while (*p == ' ')
								++p;
							arg = p--;
						}
						break;
					}

					if (c == '\n' || *p == '\n') {
						if (!*p)
							p++;
						debug("execute %s", s);
						for(int i = 0; i < argc; i++)
							debug(" %s", args[i]);
						debug("\n");
						cmd->action.cmd(args);
						break;
					}
				}
			}
		}
	}
}

static void
handle_mouse() {
#ifdef CONFIG_MOUSE
	MEVENT event;
	unsigned int i;
	if (getmouse(&event) != OK)
		return;
	msel = get_client_by_coord(event.x, event.y);

	if (!msel)
		return;

	debug("mouse x:%d y:%d cx:%d cy:%d mask:%d\n", event.x, event.y, event.x - msel->x, event.y - msel->y, event.bstate);

	vt_mouse(msel->term, event.x - msel->x, event.y - msel->y, event.bstate);

	for (i = 0; i < countof(buttons); i++) {
		if (event.bstate & buttons[i].mask)
			buttons[i].action.cmd(buttons[i].action.args);
	}

	msel = NULL;
#endif /* CONFIG_MOUSE */
}

static void
handle_statusbar() {
	char *p;
	int r;
	switch (r = read(bar.fd, bar.text, sizeof bar.text - 1)) {
		case -1:
			strncpy(bar.text, strerror(errno), sizeof bar.text - 1);
			bar.text[sizeof bar.text - 1] = '\0';
			bar.fd = -1;
			break;
		case 0:
			bar.fd = -1;
			break;
		default:
			bar.text[r] = '\0';
			p = bar.text + r - 1;
			for (; p >= bar.text && *p == '\n'; *p-- = '\0');
			for (; p >= bar.text && *p != '\n'; --p);
			if (p >= bar.text)
				memmove(bar.text, p + 1, strlen(p));
			drawbar();
	}
}

static int
open_or_create_fifo(const char *name, const char **name_created) {
	struct stat info;
	int fd;

	do {
		if ((fd = open(name, O_RDWR|O_NONBLOCK)) == -1) {
			if (errno == ENOENT && !mkfifo(name, S_IRUSR|S_IWUSR)) {
				*name_created = name;
				continue;
			}
			error("%s\n", strerror(errno));
		}
	} while (fd == -1);

	if (fstat(fd, &info) == -1)
		error("%s\n", strerror(errno));
	if (!S_ISFIFO(info.st_mode))
		error("%s is not a named pipe\n", name);
	return fd;
}

static void
usage() {
	cleanup();
	eprint("usage: dvtm [-v] [-M] [-m mod] [-d delay] [-h lines] [-t title] "
	       "[-s status-fifo] [-c cmd-fifo] [cmd...]\n");
	exit(EXIT_FAILURE);
}

static bool
parse_args(int argc, char *argv[]) {
	int arg;
	bool init = false;

	if (!getenv("ESCDELAY"))
		set_escdelay(100);
	for (arg = 1; arg < argc; arg++) {
		if (argv[arg][0] != '-') {
			const char *args[] = { argv[arg], NULL, NULL };
			if (!init) {
				setup();
				init = true;
			}
			create(args);
			continue;
		}
		if (argv[arg][1] != 'v' && argv[arg][1] != 'M' && (arg + 1) >= argc)
			usage();
		switch (argv[arg][1]) {
			case 'v':
				puts("dvtm-"VERSION" © 2007-2013 Marc André Tanner");
				exit(EXIT_SUCCESS);
			case 'M':
				mouse_events_enabled = !mouse_events_enabled;
				break;
			case 'm': {
				char *mod = argv[++arg];
				if (mod[0] == '^' && mod[1])
					*mod = CTRL(mod[1]);
				for (unsigned int i = 0; i < countof(keys); i++)
					keys[i].mod = *mod;
				break;
			}
			case 'd':
				set_escdelay(atoi(argv[++arg]));
				if (ESCDELAY < 50)
					set_escdelay(50);
				else if (ESCDELAY > 1000)
					set_escdelay(1000);
				break;
			case 'h':
				screen.history = atoi(argv[++arg]);
				break;
			case 't':
				title = argv[++arg];
				break;
			case 's':
				bar.fd = open_or_create_fifo(argv[++arg], &bar.file);
				updatebarpos();
				break;
			case 'c': {
				const char *fifo;
				cmdfifo.fd = open_or_create_fifo(argv[++arg], &cmdfifo.file);
				if (!(fifo = realpath(argv[arg], NULL)))
					error("%s\n", strerror(errno));
				setenv("DVTM_CMD_FIFO", fifo, 1);
				break;
			}
			default:
				usage();
		}
	}
	return init;
}

int
main(int argc, char *argv[]) {
	int mod = ERR;

	if (!parse_args(argc, argv)) {
		setup();
		startup(NULL);
	}

	while (running) {
		Client *c, *t;
		int r, nfds = 0;
		fd_set rd;

		if (screen.need_resize) {
			resize_screen();
			screen.need_resize = false;
		}

		FD_ZERO(&rd);
		FD_SET(STDIN_FILENO, &rd);

		if (cmdfifo.fd != -1) {
			FD_SET(cmdfifo.fd, &rd);
			nfds = cmdfifo.fd;
		}

		if (bar.fd != -1) {
			FD_SET(bar.fd, &rd);
			nfds = max(nfds, bar.fd);
		}

		for (c = clients; c; ) {
			if (c->died) {
				t = c->next;
				destroy(c);
				c = t;
				continue;
			}
			FD_SET(c->pty, &rd);
			nfds = max(nfds, c->pty);
			c = c->next;
		}
		doupdate();
		r = select(nfds + 1, &rd, NULL, NULL, NULL);

		if (r == -1 && errno == EINTR)
			continue;

		if (r < 0) {
			perror("select()");
			exit(EXIT_FAILURE);
		}

		if (FD_ISSET(STDIN_FILENO, &rd)) {
			int code = getch();
			Key *key;
			if (code >= 0) {
				if (mod >= 0) {
					if ((key = keybinding(mod, code)))
						key->action.cmd(key->action.args);
					mod = ERR;
				} else if (code == KEY_MOUSE) {
					handle_mouse();
				} else if (is_modifier(code)) {
					mod = code;
				} else if ((key = keybinding(ERR, code))) {
					key->action.cmd(key->action.args);
				} else if (sel && vt_copymode(sel->term)) {
					vt_copymode_keypress(sel->term, code);
					draw(sel);
				} else {
					keypress(code);
				}
			}
			if (r == 1) /* no data available on pty's */
				continue;
		}

		if (cmdfifo.fd != -1 && FD_ISSET(cmdfifo.fd, &rd))
			handle_cmdfifo();

		if (bar.fd != -1 && FD_ISSET(bar.fd, &rd))
			handle_statusbar();

		for (c = clients; c; ) {
			if (FD_ISSET(c->pty, &rd) && !vt_copymode(c->term)) {
				if (vt_process(c->term) < 0 && errno == EIO) {
					/* client probably terminated */
					t = c->next;
					destroy(c);
					c = t;
					continue;
				}
				if (c != sel && is_content_visible(c)) {
					draw_content(c);
					wnoutrefresh(c->window);
				}
			}
			c = c->next;
		}

		if (is_content_visible(sel)) {
			draw_content(sel);
			curs_set(vt_cursor(sel->term));
			wnoutrefresh(sel->window);
		}
	}

	cleanup();
	return 0;
}
