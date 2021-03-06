static void tile(void)
{
	unsigned int i, m, n, nx, ny, nw, nh, nm, mw, th;
	Client *c;

	for (n = 0, m = 0, c = nextvisible(clients); c; c = nextvisible(c->next), n++)
		if (c->minimized)
			m++;
	nm = n - m;
	/* window geoms */
	mw = (n == 1 || n - 1 == m) ? waw : screen.mfact * waw;
	/* check if there are at least 2 non minimized clients */
	if (n - 1 > m)
		th = (wah - m) / (nm - 1);

	nx = wax;
	ny = way;
	for (i = 0, c = nextvisible(clients); c; c = nextvisible(c->next), i++) {
		if (i == 0) {	/* master */
			nw = mw;
			nh = (n - 1 > m) ? wah : wah - m;
		} else {	/* tile window */
			if (!c->minimized) {
				if (i == 1) {
					ny = way;
					nx += mw;
					nw = waw - mw;
					mvvline(ny, nx, THEME_VERT_CHAR, wah);
					mvaddch(ny, nx, THEME_TT_CHAR);
					nx++, nw--;
				}
				/* remainder */
				if (m == 0 && i + 1 == n)	/* no minimized clients */
					nh = (way + wah) - ny;
				else if (i == nm - 1)	/* last not minimized client */
					nh = (way + wah - (n - i - 1)) - ny;
				else
					nh = th;
			} else {
				nh = 1;
				ny = way + wah - (n - i);
			}
			if (i > 1 && nm > 1)
				mvaddch(ny, nx - 1, THEME_LT_CHAR);
		}
		resize(c, nx, ny, nw, nh);
		if (n > 1 && th != wah)
			ny += nh;
	}
}
