/* discedit.h
 *
 * Copyright (c) 1998-2002  Mike Oliphant <oliphant@gtk.org>
 *
 *   http://www.nostatic.org/grip
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef GRIP_DISCEDIT_H
#define GRIP_DISCEDIT_H

#include "grip.h"

#include "vtracks.h"

GtkWidget *MakeEditBox(GripInfo *ginfo);
void TrackEditChanged(GtkWidget *widget,gpointer data);
void UpdateMultiArtist(GtkWidget *widget,gpointer data);
void UpdateVTrackEdit(GtkWidget *widget, gpointer data);
void ToggleTrackEdit(GtkWidget *widget,gpointer data);
void UpdateTitleEdit(GripInfo *ginfo);
void SetTitle(GripInfo *ginfo, DiscInstance *ins, DiscGuiInstance *gins, char *title);
void UpdateArtistEdit(GripInfo *ginfo);
void SetArtist(GripInfo *ginfo, DiscInstance *ins, DiscGuiInstance *gins, char *artist);
void SetYear(GripInfo *ginfo,int year);
void SetID3Genre(GripInfo *ginfo,int id3_genre);

#endif /* ifndef GRIP_DISCEDIT_H */
