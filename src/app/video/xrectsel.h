/*
 * This code is based and adapted from:
 * https://github.com/lolilolicon/FFcast2/blob/master/xrectsel.c
 *
 * now located at:
 * https://github.com/lolilolicon/xrectsel/blob/master/xrectsel.c
 *
 * xrectsel.c -- print the geometry of a rectangular screen region.
 * Copyright (C) 2011-2014 lolilolicon <lolilolicon@gmail.com>
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __XRECTSEL_H__
#define __XRECTSEL_H__

G_BEGIN_DECLS

void xrectsel(unsigned *x_sel, unsigned *y_sel, unsigned *w_sel, unsigned *h_sel);

G_END_DECLS

#endif /* __XRECTSEL_H__ */