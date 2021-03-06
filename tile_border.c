static void tile_border(void)
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
			nx++;
			nw = mw;
			nw = nw - 2;
			nh = (n - 1 > m) ? wah : wah - m;
			nh = nh - 1;
			mvvline(ny+1, nw+1, THEME_VERT_CHAR, nh-1);
			mvhline(ny+nh, nx, THEME_HORI_CHAR, nw);
			mvvline(ny+1, nx-1, THEME_VERT_CHAR, nh-1);
			mvaddch(ny, nx - 1, THEME_UL_CHAR);
			mvaddch(ny+nh, nx - 1, THEME_LL_CHAR);
			mvaddch(ny, nx+nw, THEME_UR_CHAR);
			mvaddch(ny+nh, nx+nw, THEME_LR_CHAR);
		} else {	/* tile window */
			if (!c->minimized) {
				if (i == 1) {
					ny = way;
					nx = wax;
					nx += mw;
					nw = waw - mw;
					nx++;
					nw = nw - 2;
				}
				/* remainder */
				if (m == 0 && i + 1 == n) {
					/* no minimized clients */
					if (i != 1) {
						ny = ny + 1;
					}
					nh = (way + wah) - ny - 1;
				} else if (i == nm - 1) {
					/* last not minimized client */
					ny = ny + 1;
					nh = ny + 1;
					nh = (way + wah - (n - i - 1) - ny);
				} else {
					nh = th;
					nh = nh - 1;
					if (i == 1) {
						ny = ny;
					} else {
						ny = ny + 1;
					}
				}
				mvvline(ny+1, nx+nw, THEME_VERT_CHAR, nh-1);
				mvhline(ny+nh, nx, THEME_HORI_CHAR, nw);
				mvvline(ny+1, nx-1, THEME_VERT_CHAR, nh-1);
				mvaddch(ny, nx - 1, THEME_UL_CHAR);
				mvaddch(ny+nh, nx - 1, THEME_LL_CHAR);
				mvaddch(ny, nx+nw, THEME_UR_CHAR);
				mvaddch(ny+nh, nx+nw, THEME_LR_CHAR);
			} else {
				nh = 1;
				ny = way + wah - (n - i);
			}
		}
		resize(c, nx, ny, nw, nh);
		if (n > 1 && th != wah)
			ny += nh;
	}
}
