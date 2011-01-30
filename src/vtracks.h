/* vtracks.h
 *
 * Copyright (c) 2003-2005  Paul Cassella
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
 *
 */
#ifndef GRIP_VTRACKS_H
#define GRIP_VTRACKS_H

#include "grip.h"

#define FOREACH_VTRACK_SET(Disc, vins)				\
for((vins) = &(Disc)->v_instance[0];				\
	(vins) < &(Disc)->v_instance[(Disc)->num_vtrack_sets];	\
	(vins)++)

#define FOREACH_VTRACK_SET_WITH_GUI(Disc, uinfo, vins, vgins)		\
for((vins) = &(Disc)->v_instance[0], (vgins) = &(uinfo)->v_instance[0];	\
	(vins) < &(Disc)->v_instance[(Disc)->num_vtrack_sets];		\
	(vins)++, (vgins)++)

#define FOREACH_TRACK_SET(Disc, ins)					\
for ((ins) = &(Disc)->p_instance;					\
	 (ins) == &(Disc)->p_instance || (ins) < &(Disc)->v_instance[(Disc)->num_vtrack_sets]; \
	 ((ins) == &(Disc)->p_instance) ? ((ins) = &(Disc)->v_instance[0]) : (ins)++)

#define FOREACH_TRACK_SET_WITH_GUI(Disc, uinfo, ins, gins)			\
for ((ins) = &(Disc)->p_instance, (gins) = &(uinfo)->p_instance;		\
	 (ins) == &(Disc)->p_instance || (ins) < &(Disc)->v_instance[(Disc)->num_vtrack_sets]; \
	 ((ins) == &(Disc)->p_instance) ?					\
	   ((ins) = &(Disc)->v_instance[0], (gins) = &(uinfo)->v_instance[0])	\
	   : ((ins)++, (gins)++))

void VTracksGuiInit(GripInfo *ginfo);

int DiscDBReadVirtTracksFromvtrackinfo(Disc *Disc, char *root_dir);
void VtracksWriteVtrackinfoFile(Disc *Disc, FILE *f, char *encoding);

int VTracksetAddEmpty(Disc *Disc);
void VTracksetFinishInit(Disc *Disc, DiscInstance *vins, int first_track, int num_tracks);

void VTrackEditDisableAllButtons(GripGUI *uinfo);

void VTrackEditPTrackset(GripGUI *uinfo);
void VTrackEditVTrackset(GripGUI *uinfo);
void VTrackEditStartedPlaying(GripGUI *uinfo);
void VTrackEditStoppedPlaying(GripGUI *uinfo);
void VTrackEditStartedRipping(GripGUI *uinfo);
void VTrackEditStoppedRipping(GripGUI *uinfo);


/* Callbacks for vtrack editing */
void NewVTracksetClicked(GtkWidget *widget,gpointer data);
void NewVTracksetPrecedingClicked(GtkWidget *widget, gpointer data);
void NewVTracksetFollowingClicked(GtkWidget *widget, gpointer data);
void NewVTracksetStrictlyPrecedingClicked(GtkWidget *widget, gpointer data);
void NewVTracksetStrictlyFollowingClicked(GtkWidget *widget, gpointer data);

void RemoveVTracksetClicked(GtkWidget *widget, gpointer data);

void SplitTrackHereClicked(GtkWidget *widget, gpointer data);
void JoinToPrevClicked(GtkWidget *widget, gpointer data);
void JoinToNextClicked(GtkWidget *widget, gpointer data);
void RemoveTrackClicked(GtkWidget *widget, gpointer data);

void MoveBeginningForwardNoAdjustClicked(GtkWidget *widget, gpointer data);
void MoveBeginningForwardAdjustClicked(GtkWidget *widget, gpointer data);
void MoveBeginningBackwardClicked(GtkWidget *widget, gpointer data);
void MoveBeginningToHereAdjustClicked(GtkWidget *widget, gpointer data);
void MoveBeginningToHereNoAdjustClicked(GtkWidget *widget, gpointer data);

void MoveEndBackwardNoAdjustClicked(GtkWidget *widget, gpointer data);
void MoveEndBackwardAdjustClicked(GtkWidget *widget, gpointer data);
void MoveEndForwardClicked(GtkWidget *widget, gpointer data);
void MoveEndToHereAdjustClicked(GtkWidget *widget, gpointer data);
void MoveEndToHereNoAdjustClicked(GtkWidget *widget, gpointer data);

void VTracknumChanged(GtkWidget *widget, gpointer data);

#endif /* GRIP_VTRACKS_H */
