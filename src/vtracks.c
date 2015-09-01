/* vtracks.c
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
 */
#include <gtk/gtk.h>

#include "grip.h"
#include "common.h"
#include "grip_id3.h"
#include "cdplay.h"
#include "vtracks.h"

void MakeVTrackPage(GripInfo *, DiscInstance *, DiscGuiInstance *);
void RemoveVTrackPage(GripInfo *, DiscGuiInstance *);
void UpdateTracks(GripInfo *);
void ChangeTracks(GripInfo *, DiscInstance *, DiscGuiInstance *);

static void NewVTrackset(GripInfo *ginfo, int first_track, int num_tracks) {
  GripGUI *uinfo = &ginfo->gui_info;
  Disc *Disc = &ginfo->Disc;
  DiscInstance *vins;
  DiscGuiInstance *vgins;
  int n;
  int pagenum;
  extern void SelectionChanged(GtkTreeSelection *selection,gpointer data);
  GtkTreeSelection *select;

  pagenum = gtk_notebook_current_page(GTK_NOTEBOOK(uinfo->notebook));

  g_assert(Disc->saved_instance_offset == SAVED_INSTANCE_NONE);

  /* XXX It is necessary to remove all the vtrack GUI stuff because the
   * v_instances are arrays, so we have to reallocate them.
   *
   * The gtk elements shouldn't have pointers into this stuff; can we just
   * copy it all?  Are some of the callbacks registered with these
   * pointers?  */

  select = gtk_tree_view_get_selection(GTK_TREE_VIEW(ginfo->gui_info.instance->track_list));

  g_signal_handlers_block_by_func(G_OBJECT(select),
				  SelectionChanged,(gpointer)ginfo);

  if (Disc->instance == &Disc->p_instance) {
	Disc->saved_instance_offset = SAVED_INSTANCE_PHYS;
  } else {
	Disc->saved_instance_offset = Disc->instance - Disc->v_instance;
	g_assert(Disc->saved_instance_offset == uinfo->instance - uinfo->v_instance);
	ChangeTracks(ginfo, &Disc->p_instance, &uinfo->p_instance);
  }
  
  FOREACH_VTRACK_SET_WITH_GUI(Disc, uinfo, vins, vgins) {
	RemoveVTrackPage(ginfo, vgins);
  }
  g_free(uinfo->v_instance);
  uinfo->v_instance = NULL;

  n = VTracksetAddEmpty(Disc);

  vins = &Disc->v_instance[n];

  VTracksetFinishInit(Disc, vins, first_track, num_tracks);

  UpdateTracks(ginfo);

  g_signal_handlers_unblock_by_func(G_OBJECT(select),
				    SelectionChanged,(gpointer)ginfo);

  if (Disc->saved_instance_offset != SAVED_INSTANCE_PHYS) {
	ChangeTracks(ginfo, Disc->v_instance + Disc->saved_instance_offset,
				 uinfo->v_instance + Disc->saved_instance_offset);
  }
  Disc->saved_instance_offset = SAVED_INSTANCE_NONE;

  if (pagenum > 4)
	gtk_notebook_set_page(GTK_NOTEBOOK(uinfo->notebook), pagenum);
}

void NewVTracksetClicked(GtkWidget *widget,gpointer data) {
  GripInfo *ginfo = data;

  NewVTrackset(ginfo, 0, -1);
}

void NewVTracksetPrecedingClicked(GtkWidget *widget, gpointer data) {
  GripInfo *ginfo = data;
  NewVTrackset(ginfo, 0, CURRENT_TRACK + 1);
}

void NewVTracksetFollowingClicked(GtkWidget *widget, gpointer data) {
  GripInfo *ginfo = data;
  NewVTrackset(ginfo, CURRENT_TRACK, -1);
}

void NewVTracksetStrictlyPrecedingClicked(GtkWidget *widget, gpointer data) {
  GripInfo *ginfo = data;
  NewVTrackset(ginfo, 0, CURRENT_TRACK);
}

void NewVTracksetStrictlyFollowingClicked(GtkWidget *widget, gpointer data) {
  GripInfo *ginfo = data;
  NewVTrackset(ginfo, CURRENT_TRACK + 1, -1);
}

void RemoveVTrackset(GripInfo *ginfo, DiscInstance *dins, DiscGuiInstance *dgins) {
  GripGUI *uinfo = &ginfo->gui_info;
  Disc *Disc = &ginfo->Disc;
  DiscInstance *vins;
  DiscGuiInstance *vgins;
  int pagenum;

  pagenum = gtk_notebook_current_page(GTK_NOTEBOOK(uinfo->notebook));

  g_assert(Disc->saved_instance_offset == SAVED_INSTANCE_NONE);

  if (Disc->instance == &Disc->p_instance) {
	Disc->saved_instance_offset = SAVED_INSTANCE_PHYS;
  } else if (Disc->instance == dins) {
	Disc->saved_instance_offset = SAVED_INSTANCE_PHYS;
	pagenum = 0;
	ChangeTracks(ginfo, &Disc->p_instance, &uinfo->p_instance);
  } else {
	Disc->saved_instance_offset = Disc->instance - Disc->v_instance;
	ChangeTracks(ginfo, &Disc->p_instance, &uinfo->p_instance);
  }
  
  FOREACH_VTRACK_SET_WITH_GUI(Disc, uinfo, vins, vgins) {
	RemoveVTrackPage(ginfo, vgins);
  }
  g_free(uinfo->v_instance);
  uinfo->v_instance = NULL;

  Disc->num_vtrack_sets--;

  if (Disc->num_vtrack_sets) {
	  memmove(dins, dins + 1,
		  (Disc->num_vtrack_sets - (dins - Disc->v_instance)) * sizeof(DiscInstance));
  } else {
	  g_free(Disc->v_instance);
	  Disc->v_instance = NULL;
  }

  UpdateTracks(ginfo);

  if (Disc->saved_instance_offset != SAVED_INSTANCE_PHYS) {
	ChangeTracks(ginfo, Disc->v_instance + Disc->saved_instance_offset,
				 uinfo->v_instance + Disc->saved_instance_offset);
  }
  Disc->saved_instance_offset = SAVED_INSTANCE_NONE;

  if (pagenum > 4)
	gtk_notebook_set_page(GTK_NOTEBOOK(uinfo->notebook), pagenum);
}

void RemoveVTracksetClicked(GtkWidget *widget, gpointer data) {
  GripInfo *ginfo = data;
  /* Actually, should disable this button when p_instance selected. */
  if (ginfo->Disc.instance == &ginfo->Disc.p_instance)
	return;
  RemoveVTrackset(ginfo, ginfo->Disc.instance, ginfo->gui_info.instance);
}

static void copy_phys_track_to_virt(Disc *Disc, DiscInstance *dins, int dst_i,
				    DiscInstance *sins, int src_i) {
  DiscDataInstance *sdins = &sins->data;
  DiscInfoInstance *siins = &sins->info;
  TrackData *strack = &sdins->tracks[src_i];
  TrackInfo *sitrack = &siins->tracks[src_i];

  DiscDataInstance *ddins = &dins->data;
  DiscInfoInstance *diins = &dins->info;
  TrackData *dtrack = &ddins->tracks[dst_i];
  TrackInfo *ditrack = &diins->tracks[dst_i];

  if (!dtrack->track_name[0]) {
	  strcpy(dtrack->track_name, strack->track_name);
	  strcpy(dtrack->track_artist, strack->track_artist);
	  strcpy(dtrack->track_extended, strack->track_extended);
  }
  
  /* If the tracknum hasn't been specified, set it to one more than the
   * number of the preceding track, or 1 if this is the first track. */
  if (dtrack->vtracknum == -1) {
	  if (dst_i > 0) {
		  TrackData *pdtrack = &ddins->tracks[dst_i - 1];
		  dtrack->vtracknum = pdtrack->vtracknum + 1;
	  } else {
		  dtrack->vtracknum = 1;
	  }
  }

  if (ditrack->start_frame == 0 && ditrack->num_frames == 0)
	  memcpy(ditrack, sitrack, sizeof(TrackInfo));
}


/*
 * first_track, num_tracks describe what to do if the first vtracks aren't
 * filled in.  vtrack[0] is copied from ptrack[first_track], and up to
 * num_tracks are copied in total.
 *
 * dins is the destination instance
 * sins is the source instance
 */
static void fill_in_initial_virt_tracks(Disc *Disc, DiscInstance *dins,
					DiscInstance *sins, int first_track,
					int num_tracks) {
  int i;

  for(i = 0; i < num_tracks; i++) {
	  copy_phys_track_to_virt(Disc, dins, i, sins, first_track + i);
  }
  if (dins->info.num_tracks == 0) {
	  dins->info.num_tracks = num_tracks;
  }
}


/* Put in a fake track for compatibility */
static void add_fake_trailing_vtrack(Disc *Disc, DiscInstance *vins) {
  DiscInfoInstance *viins = &vins->info;
  int num_tracks = viins->num_tracks;

  memset(&viins->tracks[num_tracks], 0, sizeof(TrackInfo));
  viins->tracks[num_tracks].start_frame = 
	viins->tracks[num_tracks].start_frame +
	viins->tracks[num_tracks].num_frames;
}


int VTracksetAddEmpty(Disc *Disc) {
  DiscDataInstance *pdins = &Disc->p_instance.data;
  DiscInstance *vins;
  DiscDataInstance *vdins;
  int index;
  int n = Disc->num_vtrack_sets++;

  Disc->v_instance = g_realloc(Disc->v_instance,
			       Disc->num_vtrack_sets * sizeof (DiscInstance));

  vins = &Disc->v_instance[n];
  vdins = &vins->data;

  memset(vins, 0, sizeof(DiscInstance));

  for (index = 0; index < MAX_TRACKS; index++)
	  vdins->tracks[index].vtracknum = -1;

  /* These we fill in now because we can only replace them, and we
   * don't want to try to detect the genre especially not being
   * changed. */
  vdins->genre = pdins->genre;
  vdins->id3genre = pdins->id3genre;
  vdins->year = pdins->year;

  vdins->multi_artist = FALSE;

  return n;
}


static void set_vtracknums(DiscInstance *vins) {
  int i;

  if (vins->data.tracks[0].vtracknum == -1)
	  vins->data.tracks[0].vtracknum = 1;

  for (i = 1; i < vins->info.num_tracks; i++) {
	  if (vins->data.tracks[i].vtracknum == -1)
		  vins->data.tracks[i].vtracknum =
		      vins->data.tracks[i - 1].vtracknum + 1;
  }
}


/*
 * num_tracks == -1 indicates copy through end of current trackset
 *
 * The vtrackset has been at least partly initialized by reading the
 * vtrackinfo file, so don't change anything that was set there.
 */
void VTracksetFinishInit(Disc *Disc, DiscInstance *vins, int first_track, int num_tracks) {
  DiscDataInstance *vdins = &vins->data;
  DiscInstance *sins;
  DiscDataInstance *sdins;

  /*
   * If saved_instance_offset is NONE, we are constructing the vtracksets
   * from the .vtrackinfo file.
   *
   * Otherwise, we're creating a new vtrackset, and we had to remove the
   * vtrack instances to create the new one.  The set that was selected at
   * the time is indicated by saved_instance_offset.
   */

  g_assert(first_track != -1);

  g_assert(Disc->instance == &Disc->p_instance);

  if (Disc->saved_instance_offset == SAVED_INSTANCE_NONE)
	  sins = Disc->instance;
  else if (Disc->saved_instance_offset == SAVED_INSTANCE_PHYS)
	  sins = &Disc->p_instance;
  else
	  sins = &Disc->v_instance[Disc->saved_instance_offset];
  sdins = &sins->data;

  if (num_tracks == -1)
	  num_tracks = sins->info.num_tracks - first_track;
  else
	  g_assert(num_tracks <= sins->info.num_tracks - first_track);

  if (!vdins->title[0])
	strcpy(vdins->title, sdins->title);
  if (!vdins->artist[0])
	strcpy(vdins->artist, sdins->artist);

/*
  XXX Need to do this in a manner that won't mess up reading a vtrackset
  file that does/doesn't set the year.  This will probably mostly involve
  fixing VTracksetAddEmpty and all callers.

  if (vdins->year == 0)
	  vdins->year = sdins->year;
*/

  /* If the vtrackinfo file didn't specify the initial tracks, copy them
   * from the physical tracks */
  fill_in_initial_virt_tracks(Disc, vins, sins, first_track, num_tracks);

  set_vtracknums(vins);

  add_fake_trailing_vtrack(Disc, vins);
}


void VTracksGuiInit(GripInfo *ginfo) {
  Disc *Disc = &ginfo->Disc;
  GripGUI *uinfo = &ginfo->gui_info;
  DiscInstance *vins;
  DiscGuiInstance *vgins;

  if (Disc->num_vtrack_sets == 0)
	return;

  ginfo->gui_info.v_instance = g_malloc(Disc->num_vtrack_sets * sizeof(DiscGuiInstance));
  FOREACH_VTRACK_SET_WITH_GUI(Disc, uinfo, vins, vgins) {
  	MakeVTrackPage(ginfo, vins, vgins);
  }
}


static void VTrackCheckSkippage(Disc *Disc, int v,
				int *_start_track, int *_end_track, 
				DiscDataInstance *vdins, DiscInfoInstance *viins,
				gboolean *_match_physical,
				gboolean *_write_title, gboolean *_write_artist,
				gboolean *_write_frames,
				gboolean *_write_vtracknum) {
  DiscInfoInstance *piins = &Disc->p_instance.info;
  DiscDataInstance *pdins = &Disc->p_instance.data;

  gboolean match_physical = *_match_physical;
  gboolean write_title = TRUE;
  gboolean write_artist = TRUE;
  gboolean write_frames = TRUE;
  gboolean write_vtracknum;

  int start_track = -1;
  int end_track = -1;

  int p;

  TrackData *vtdata = &vdins->tracks[v];
  TrackInfo *vtinfo = &viins->tracks[v];


  /*
   * Write the vtracknum if it's not the one we expect; that is, - the
   * first track is expected to have vtracknum 1 - other tracks are
   * expected to have a vtracknum one higher than the preceeding track.
   */

  write_vtracknum = 
      (v == 0 && vdins->tracks[0].vtracknum != 1) ||
      (v != 0 && vtdata->vtracknum != (vtdata - 1)->vtracknum + 1);

  /*
   * Check to see if this virtual track corresponds to one or
   * more physical tracks
   */
  for (p = 0; p < piins->num_tracks; p++) {
	  TrackInfo *ptinfo = &piins->tracks[p];
	  if (start_track == -1) {
		  if (abs(vtinfo->start_frame - ptinfo->start_frame) <= 1) {
			  start_track = p;
		  }
	  }
	  if (abs((vtinfo->start_frame + vtinfo->num_frames) -
		  (ptinfo->start_frame + ptinfo->num_frames))
	      <= 1) {
		  end_track = p;
		  break;
	  }
  }

  *_start_track = start_track;
  *_end_track = end_track;

  /*
   * We're going to try to write out a minimal vtrackinfo
   * file, hopefully the same file that we read, modulo the
   * user's changes.  
   *
   * If the vtrack corresponds to exactly one physical track,
   * we may be able to skip something.
   */
  if (start_track != -1 && start_track == end_track) {
	  int p = start_track;
	  TrackData *ptdata = &pdins->tracks[p];

	  /*
	   * Even if we're going to write a record, don't write out
	   * the track name / artist if it's the same as that of
	   * the physical track this one corresponds to.
	   */
	  if(!strcmp(vtdata->track_name, ptdata->track_name))
		  write_title = FALSE;

	  if(!strcmp(vtdata->track_artist, ptdata->track_artist))
		  write_artist = FALSE;

	  /*
	   * Don't write out leading virtual tracks that correspond
	   * exactly to the leading physical tracks.  Note, we
	   * don't stop skipping tracks if only the vtracknum is
	   * changed.
	   */
	  if (v != p || write_title || write_artist)
		  match_physical = FALSE;

	  if (v == p) {
		  write_frames = FALSE;

		  if (match_physical && 
		      !write_title && !write_artist) {

			  /*
			   * Don't skip this one if the
			   * vtracknum is unexpected.
			   */
			  if (!write_vtracknum) {
				  /*
				   * Skip it unless it's the last
				   * track of the vtrack set, but
				   * doesn't correspond to the last
				   * physical track.
				   *
				   * This one needs to be present
				   * to keep grip from copying all
				   * the physical tracks to the new
				   * vtrackset.
				   */
				  if (v != viins->num_tracks - 1 ||
				      p == piins->num_tracks -1) {
					  goto out;
				  }
				  write_frames = TRUE;
			  }
		  }
	  }
  } else {
	  /* This doesn't correspond exactly to a single physical
	   * track */
	  match_physical = FALSE;
  }

  out:

  *_write_title = write_title;
  *_write_artist = write_artist;
  *_write_frames = write_frames;
  *_write_vtracknum = write_vtracknum;
  *_match_physical = match_physical;

  return;
}


void DiscDBWriteLine(char *header,int num,char *data,FILE *outfile,
                            char *encoding);

void VtracksWriteVtrackinfoFile(Disc *Disc, FILE *f, char *encoding) {
  DiscInstance *pins = &Disc->p_instance;
  DiscDataInstance *pdins = &pins->data;

  DiscInstance *vins;
  gboolean first = TRUE;

  FOREACH_VTRACK_SET(Disc, vins) {
	gboolean match_physical = TRUE;
	DiscDataInstance *vdins = &vins->data;
	DiscInfoInstance *viins = &vins->info;
	char pt[100];
	int v;

	if (first) {
		first = FALSE;
	} else {
		fprintf(f, "VNEWALBUM\n");
	}

	if (strcmp(vdins->title, pdins->title))
		DiscDBWriteLine("VDTITLE", -1, vdins->title, f, encoding);
	if (strcmp(vdins->artist, pdins->artist))
		DiscDBWriteLine("VDARTIST", -1, vdins->artist, f, encoding);
	if (vdins->year != 0 && vdins->year != pdins->year) {
		sprintf(pt, "%d", vdins->year);
		DiscDBWriteLine("VYEAR", -1, pt, f, encoding);
	}

/* 	if (vdins->genre != pdins->genre) { */
/* 		sprintf */

	for (v = 0; v < viins->num_tracks; v++) {
		int start_track = -1, end_track = -1;
		gboolean write_title, write_artist, write_frames;
		gboolean write_vtracknum;
		TrackData *vtdata = &vdins->tracks[v];
		TrackInfo *vtinfo = &viins->tracks[v];

		VTrackCheckSkippage(Disc, v, &start_track, &end_track,
				    vdins, viins, &match_physical,
				    &write_title, &write_artist, &write_frames,
				    &write_vtracknum);

		if (write_title)
			DiscDBWriteLine("VTITLE", v, vtdata->track_name, f, encoding);
		if (write_artist && vdins->multi_artist)
			DiscDBWriteLine("VARTIST", v, vtdata->track_artist, f, encoding);
		if (write_vtracknum)
			fprintf(f, "VTRACKNUM%d=%d\n", v, vtdata->vtracknum);

		if (write_frames) {
			if (start_track != -1) {
				if (end_track != -1) {
					if (start_track == end_track) {
						sprintf(pt, "PTRACK%d", start_track);
					} else {
						sprintf(pt, "PTRACK%d-PTRACK%d", start_track, end_track);
					}
				} else {
					sprintf(pt, "PTRACK%d-%d", start_track,
						vtinfo->start_frame + vtinfo->num_frames);
				}
			} else if (end_track != -1) {
				sprintf(pt, "%d-PTRACK%d", vtinfo->start_frame, end_track);
			} else {
				sprintf(pt, "%d-%d", vtinfo->start_frame,
					vtinfo->start_frame + vtinfo->num_frames);
			}

			DiscDBWriteLine("VFRAMES", v, pt, f, encoding);
		}
	} /* End vtrack */
  } /* End valbum */

}


/*
 *   changing:  beginning or ending
 *              forwards  or backwards
 *              constant amount or "to here" or to adjacent track
 *              adjusting or not  (adjacent tracks)
 *
 * If TO_HERE is not specified, the adjustment is by the amount the user
 * specifies in adjustment_count_spin_button and adjustment_unit_combo.
 *
 *   splitting track
 *   removing track
 *     -joining track
 */

#define ADJUST_ADJACENT   0x1
#define UPDATE_TRACKLIST  0x2
#define NO_CHECKS         0x4

#define MOVE_BACKWARD   0x100
#define MOVE_FORWARD    0x200
#define TO_HERE         0x400  /* Adjust to the current position */


void AdjustTrack(GripInfo *ginfo, int tracknum, int start_delta, int end_delta, int flags) {
  Disc *Disc = &ginfo->Disc;
  TrackInfo *prev, *track, *next;

  int adjust = flags & ADJUST_ADJACENT;

  int new_start, new_end;
  int disc_last_frame;

  Debug(_("adjusting track %d, deltas %d deltae %d flags 0x%x\n"),
	tracknum, start_delta, end_delta, flags);

  if (Disc->instance == &Disc->p_instance) {
	extern void DisplayMsg(char *msg);
	DisplayMsg(_("Can't edit physical tracks"));
	return;
  }

  if (tracknum > 0)
	prev = &Disc->instance->info.tracks[tracknum - 1];
  else
	prev = NULL;

  track = &Disc->instance->info.tracks[tracknum];

  if (tracknum < Disc->instance->info.num_tracks - 1)
	next = &Disc->instance->info.tracks[tracknum + 1];
  else
	next = NULL;

  new_start = track->start_frame + start_delta;
  new_end   = track->start_frame + track->num_frames + end_delta;

  disc_last_frame = Disc->p_instance.info.tracks[Disc->p_instance.info.num_tracks].start_frame;

  if (new_start < 0) {
	  g_warning(_("AdjustTrack: Tried to make start_frame negative"));
	  return;
  }

  if (new_start >= new_end) {
	  g_warning(_("AdjustTrack: Tried to move first frame %d past end frame "
		      "%d"), new_start, new_end);
	  return;
  }

  if (new_end > disc_last_frame) {
	  g_warning(_("AdjustTrack: Tried to move end frame past "
		      "disc's end frame, %d"), disc_last_frame);
	  return;
  }


  /* Should handle these next two by removing the other frame */
  if (prev && new_start < prev->start_frame) {
	  g_warning(_("AdjustTrack: Tried to move start frame before "
		      "previous start frame"));
	  return;
  }

  if (next && new_end > next->start_frame + next->num_frames) {
	  g_warning(_("AdjustTrack: Tried to move end frame after "
		      "next end frame"));
	  return;
  }

  if (!(flags & NO_CHECKS)) {
	  if (!adjust) {
		  if (prev && new_start < prev->start_frame + prev->num_frames) {
			  g_warning(_("AdjustTrack: Tried to move start frame "
				      "before previous end frame "
				      "without adjusting"));
			  return;
		  }
		  if (next && new_end > next->start_frame) {
			  g_warning(_("AdjustTrack: Tried to move end past next "
				      "first frame without adjusting"));
			  return;
		  }
	  }
  }

  if (adjust) {
	  if (prev) {
		  if (start_delta > 0  &&
		      (prev->start_frame + prev->num_frames == track->start_frame ||
		       prev->start_frame + prev->num_frames + 1 == track->start_frame)){
			  prev->num_frames += start_delta;
		  } else if (new_start < prev->start_frame + prev->num_frames) {
			  g_assert(start_delta < 0);
			  prev->num_frames = new_start - prev->start_frame;
		  }
		  ReinitTrack(prev);
	  }

	  if (next) {
		  if (end_delta < 0  &&
		      (track->start_frame + track->num_frames == next->start_frame ||
		       track->start_frame + track->num_frames + 1 == next->start_frame)){
			  next->start_frame += end_delta;
			  next->num_frames  -= end_delta;
		  } else if (new_end > next->start_frame) {
			  g_assert(end_delta > 0);
			  next->num_frames -= new_end + 1 - next->start_frame;
			  next->start_frame = new_end + 1;
		  }
		  ReinitTrack(next);
	  }
  }


  track->start_frame = new_start;
  track->num_frames =  new_end - new_start;

  ReinitTrack(track);


  if (flags & UPDATE_TRACKLIST) {
	  extern void SelectionChanged(GtkTreeSelection *selection,gpointer data);
	  GtkTreeSelection *select = gtk_tree_view_get_selection(GTK_TREE_VIEW(ginfo->gui_info.instance->track_list));

	  g_signal_handlers_block_by_func(G_OBJECT(select),
					  SelectionChanged,(gpointer)ginfo);
	  UpdateCurrentTrackset(ginfo);
	  g_signal_handlers_unblock_by_func(G_OBJECT(select),
					    SelectionChanged,(gpointer)ginfo);
  }
}

/*
 * We split the track by making a copy, and discarding after the current
 * frame from the original, and from before the current frame from the
 * copy.
 */
void SplitTrackHereClicked(GtkWidget *widget, gpointer data) {
  GripInfo *ginfo = data;
  int tracknum = CURRENT_TRACK;
  TrackInfo *track = &ginfo->Disc.instance->info.tracks[tracknum];
  TrackData *trackd = &ginfo->Disc.instance->data.tracks[tracknum];
  int num_tracks = ginfo->Disc.instance->info.num_tracks;
  int i;

  /* Oughta disable this button automatically */
  if (num_tracks + 1 == MAX_TRACKS) {
	  extern void DisplayMsg(char *msg);
	  DisplayMsg(_("You can only have 100 tracks per trackset"));
	  return;
  }

  CDStat(&ginfo->Disc, FALSE);

  memmove(track + 1, track, (num_tracks - tracknum) * sizeof(TrackInfo));
  memmove(trackd + 1, trackd, (num_tracks - tracknum) * sizeof(TrackData));

  for (i = tracknum + 1; i < num_tracks + 1; i++)
	  ginfo->Disc.instance->data.tracks[i].vtracknum++;

  for (i = 0; i < ginfo->prog_totaltracks; i++) {
	  if (ginfo->tracks_prog[i] > tracknum)
		  ginfo->tracks_prog[i]++;
	  if (ginfo->tracks_prog[i] == tracknum) {
		  memmove(&ginfo->tracks_prog[i + 1],
			  &ginfo->tracks_prog[i],
			  (ginfo->prog_totaltracks - i) * sizeof(int));
		  ginfo->tracks_prog[i + 1] = tracknum + 1;
		  ginfo->prog_totaltracks++;
		  i++;
	  }
  }

  ginfo->Disc.instance->info.num_tracks++;

  /* Note - doesn't update the gui, as the state is inconsistent. */
  AdjustTrack(ginfo, tracknum, 0, ginfo->Disc.info.curr_frame - (track->start_frame + track->num_frames), NO_CHECKS);

  tracknum++;
  track++;

  AdjustTrack(ginfo, tracknum, ginfo->Disc.info.curr_frame - track->start_frame, 0, UPDATE_TRACKLIST);
}

void RemoveTrack(GripInfo *ginfo, int tracknum) {
  TrackInfo *track = &ginfo->Disc.instance->info.tracks[tracknum];
  TrackData *trackd = &ginfo->Disc.instance->data.tracks[tracknum];
  int num_tracks;
  int i;

  num_tracks = --ginfo->Disc.instance->info.num_tracks;

  memmove(track, track + 1, (num_tracks - tracknum) * sizeof(TrackInfo));
  memmove(trackd, trackd + 1, (num_tracks - tracknum) * sizeof(TrackData));

  for (i = tracknum; i < num_tracks; i++)
	  if (ginfo->Disc.instance->data.tracks[i].vtracknum > 0)
		  ginfo->Disc.instance->data.tracks[i].vtracknum--;

  for (i = 0; i < ginfo->prog_totaltracks; i++) {
	  if (ginfo->tracks_prog[i] == tracknum) {
		  ginfo->prog_totaltracks--;
		  memmove(&ginfo->tracks_prog[i],
			  &ginfo->tracks_prog[i+1],
			  ginfo->prog_totaltracks - i);
		  i--;
	  } else if (ginfo->tracks_prog[i] > tracknum) {
		  ginfo->tracks_prog[i]--;
	  }
  }

}

void JoinToPrevClicked(GtkWidget *widget, gpointer data) {
  GripInfo *ginfo = data;
  int tracknum = CURRENT_TRACK;
  TrackInfo *track = &ginfo->Disc.instance->info.tracks[tracknum];
  int num_frames;
  TrackInfo *prev;
  

  if (tracknum == 0) {
	  return;
  }

  prev = &ginfo->Disc.instance->info.tracks[tracknum - 1];

  if (abs(prev->start_frame + prev->num_frames - track->start_frame) > 1) {
	  g_warning(_("Can't append track to non-adjacent previous track"));
	  return;
  }

  num_frames = track->num_frames;

  RemoveTrack(ginfo, tracknum);

  AdjustTrack(ginfo, tracknum - 1, 0, num_frames, UPDATE_TRACKLIST);

  if (!ginfo->playing) {
	  PrevTrack(ginfo);
  }
}

void JoinToNextClicked(GtkWidget *widget, gpointer data) {
  GripInfo *ginfo = data;
  int tracknum = CURRENT_TRACK;
  TrackInfo *track = &ginfo->Disc.instance->info.tracks[tracknum];
  int num_frames;
  TrackInfo *next;

  if (tracknum == ginfo->Disc.instance->info.num_tracks - 1) {
	  return;
  }

  next = &ginfo->Disc.instance->info.tracks[tracknum + 1];

  if (abs(track->start_frame + track->num_frames - next->start_frame) > 1) {
	  g_warning(_("Can't prepend track to non-adjacent next track"));
	  return;
  }

  num_frames = track->num_frames;

  RemoveTrack(ginfo, tracknum);

  AdjustTrack(ginfo, tracknum, -num_frames, 0, UPDATE_TRACKLIST);
}

void RemoveTrackClicked(GtkWidget *widget, gpointer data) {
  GripInfo *ginfo = data;
  int tracknum = CURRENT_TRACK;

  RemoveTrack(ginfo, tracknum);

  UpdateCurrentTrackset(ginfo);
}

static int get_vtrack_adjustment(GripGUI *uinfo) {
  int adjustment = 0;
  int count;
  char *unit;

  count = gtk_spin_button_get_value_as_int(
      GTK_SPIN_BUTTON(uinfo->adjustment_count_spin_button));
  unit = gtk_editable_get_chars(
      GTK_EDITABLE(GTK_COMBO(uinfo->adjustment_unit_combo)->entry), 0, -1);

  if (!strcmp(unit, _("frames")))
	  adjustment = count;
  else if (!strcmp(unit, _("seconds")))
	  adjustment = count * 75;
  else if (!strcmp(unit, _("minutes")))
	  adjustment = count * 75 * 60;
  else
	  g_error("Invalid unit: %s", unit);

  g_free(unit);
  return adjustment;
}

void MoveBeginning(GtkWidget *widget, gpointer data, int flags) {
  GripInfo *ginfo = data;
  int tracknum;
  TrackInfo *track;
  int adjustment;

  CDStat(&ginfo->Disc, FALSE);

  tracknum = CURRENT_TRACK;
  track = &ginfo->Disc.instance->info.tracks[tracknum];
  
  if (flags & TO_HERE) {
	  adjustment = ginfo->Disc.info.curr_frame - track->start_frame;
  } else {
	  adjustment = get_vtrack_adjustment(&ginfo->gui_info);

	  if (flags & MOVE_BACKWARD)
		  adjustment *= -1;
	  else
		  g_assert(flags & MOVE_FORWARD);
  }

  flags &= ~(TO_HERE|MOVE_BACKWARD|MOVE_FORWARD);

  AdjustTrack(ginfo, tracknum, adjustment, 0, flags | UPDATE_TRACKLIST);
}


void MoveBeginningForwardNoAdjustClicked(GtkWidget *widget, gpointer data) {
  MoveBeginning(widget, data, MOVE_FORWARD);
}

void MoveBeginningForwardAdjustClicked(GtkWidget *widget, gpointer data) {
  MoveBeginning(widget, data, MOVE_FORWARD | ADJUST_ADJACENT);
}

void MoveBeginningBackwardClicked(GtkWidget *widget, gpointer data) {
  MoveBeginning(widget, data, MOVE_BACKWARD | ADJUST_ADJACENT);
}

void MoveBeginningToHereAdjustClicked(GtkWidget *widget, gpointer data) {
  MoveBeginning(widget, data, TO_HERE | ADJUST_ADJACENT);
}

void MoveBeginningToHereNoAdjustClicked(GtkWidget *widget, gpointer data) {
  MoveBeginning(widget, data, TO_HERE);
}


void MoveEnd(GtkWidget *widget, gpointer data, int flags) {
  GripInfo *ginfo = data;
  int tracknum;
  TrackInfo *track;
  int adjustment;
  
  CDStat(&ginfo->Disc, FALSE);

  tracknum = CURRENT_TRACK;
  track = &ginfo->Disc.instance->info.tracks[tracknum];
  
  if (flags & TO_HERE) {
	  adjustment = ginfo->Disc.info.curr_frame - 
	      (track->start_frame + track->num_frames);
  } else {
	  adjustment = get_vtrack_adjustment(&ginfo->gui_info);

	  if (flags & MOVE_BACKWARD)
		  adjustment *= -1;
	  else
		  g_assert(flags & MOVE_FORWARD);
  }

  flags &= ~(TO_HERE|MOVE_BACKWARD|MOVE_FORWARD);
  AdjustTrack(ginfo, tracknum, 0, adjustment, flags | UPDATE_TRACKLIST);
}


void MoveEndBackwardNoAdjustClicked(GtkWidget *widget, gpointer data) {
  MoveEnd(widget, data, MOVE_BACKWARD);
}

void MoveEndBackwardAdjustClicked(GtkWidget *widget, gpointer data) {
  MoveEnd(widget, data, MOVE_BACKWARD | ADJUST_ADJACENT);
}

void MoveEndForwardClicked(GtkWidget *widget, gpointer data) {
  MoveEnd(widget, data, MOVE_FORWARD | ADJUST_ADJACENT);
}

void MoveEndToHereAdjustClicked(GtkWidget *widget, gpointer data) {
  MoveEnd(widget, data, TO_HERE | ADJUST_ADJACENT);
}

void MoveEndToHereNoAdjustClicked(GtkWidget *widget, gpointer data) {
  MoveEnd(widget, data, TO_HERE);
}


#define BUTTONS_ADDTRACKSET_ALL		0x000001
#define BUTTONS_ADDTRACKSET_PREV	0x000002
#define BUTTONS_ADDTRACKSET_FOLL	0x000004
#define BUTTONS_ADDTRACKSET_SPREV	0x000008
#define BUTTONS_ADDTRACKSET_SFOLL	0x000010
#define BUTTONS_ADDTRACKSETS		0x00001f
#define BUTTONS_DELTRACKSET		0x000020
#define BUTTONS_TRACKSETS		(BUTTONS_DELTRACKSET|BUTTONS_ADDTRACKSETS)

#define BUTTONS_SPLIT			0x000040
#define BUTTONS_JOIN_PREV		0x000080
#define BUTTONS_JOIN_FOLL		0x000100
#define BUTTONS_DEL_TRACK		0x000200
#define BUTTONS_MISC			(BUTTONS_SPLIT|BUTTONS_DEL_TRACK| \
					 BUTTONS_JOIN_PREV|BUTTONS_JOIN_FOLL)

#define BUTTONS_MOVE_TO_HERE		0x001000
#define BUTTONS_MOVE_OTHER		0x002000
#define BUTTONS_MOVE			(BUTTONS_MOVE_TO_HERE|BUTTONS_MOVE_OTHER)

#define BUTTONS_SETVTRACKNUM		0x004000

#define BUTTONS_ALL (BUTTONS_TRACKSETS|BUTTONS_MISC|BUTTONS_MOVE|BUTTONS_SETVTRACKNUM)


void VTrackEditSetSensitive(GripGUI *uinfo, int buttons, gboolean s) {
  if (s)
	  uinfo->vtrack_buttons_sensitive |= buttons;
  else
	  uinfo->vtrack_buttons_sensitive &= ~buttons;

  if (buttons & BUTTONS_ADDTRACKSET_ALL)
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->newset_all_button), s);

  if (buttons & BUTTONS_ADDTRACKSET_PREV)
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->newset_preceding_button), s);

  if (buttons & BUTTONS_ADDTRACKSET_FOLL)
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->newset_following_button), s);

  if (buttons & BUTTONS_ADDTRACKSET_SPREV)
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->newset_preceding_strictly_button), s);

  if (buttons & BUTTONS_ADDTRACKSET_SFOLL)
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->newset_following_strictly_button), s);

  if (buttons & BUTTONS_DELTRACKSET)
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->removeset_button), s);


  if (buttons & BUTTONS_SPLIT)
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->split_track_button), s);

  if (buttons & BUTTONS_JOIN_PREV)
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->join_to_prev_button), s);

  if (buttons & BUTTONS_JOIN_FOLL)
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->join_to_next_button), s);

  if (buttons & BUTTONS_DEL_TRACK)
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->remove_track_button), s);


  if (buttons & BUTTONS_MOVE_TO_HERE) {
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->move_beginning_here_adjust_button), s);
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->move_beginning_here_noadjust_button), s);
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->move_end_here_adjust_button), s);
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->move_end_here_noadjust_button), s);
  }

  if (buttons & BUTTONS_MOVE_OTHER) {
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->move_beginning_forward_adjust_button), s);
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->move_beginning_forward_noadjust_button), s);
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->move_beginning_back_adjust_button), s);
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->move_end_back_adjust_button), s);
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->move_end_back_noadjust_button), s);
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->move_end_forward_adjust_button), s);
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->adjustment_count_spin_button), s);
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->adjustment_unit_combo), s);
  }

  if (buttons & BUTTONS_SETVTRACKNUM) {
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->set_vtracknum_spin_button), s);
	  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->set_vtracknum_adjust_check_button), s);
  }
}

void VTrackEditDisableAllButtons(GripGUI *uinfo) {
  VTrackEditSetSensitive(uinfo, BUTTONS_ALL, FALSE);
}

void VTrackEditPTrackset(GripGUI *uinfo) {
  VTrackEditSetSensitive(uinfo, BUTTONS_ALL & ~BUTTONS_ADDTRACKSETS, FALSE);
  VTrackEditSetSensitive(uinfo, BUTTONS_ADDTRACKSETS, TRUE);
}

void VTrackEditVTrackset(GripGUI *uinfo) {
  VTrackEditSetSensitive(uinfo, BUTTONS_ALL & ~(BUTTONS_MOVE_TO_HERE|BUTTONS_SPLIT), TRUE);
}

void VTrackEditStartedPlaying(GripGUI *uinfo) {
  if (uinfo->instance != &uinfo->p_instance)
	  VTrackEditSetSensitive(uinfo, BUTTONS_SPLIT | BUTTONS_MOVE_TO_HERE, TRUE);
}

void VTrackEditStoppedPlaying(GripGUI *uinfo) {
  if (uinfo->instance != &uinfo->p_instance)
	  VTrackEditSetSensitive(uinfo, BUTTONS_SPLIT | BUTTONS_MOVE_TO_HERE, FALSE);
}

/* XXX Should this go in the ginfo, which isn't available here, or the
 * uinfo, or somewhere else?  It shouldn't just be a static like this. */
static int buttons_that_were_sensitive = 0;

void VTrackEditStartedRipping(GripGUI *uinfo) {
  g_assert(buttons_that_were_sensitive == 0);
  buttons_that_were_sensitive = uinfo->vtrack_buttons_sensitive;
  VTrackEditDisableAllButtons(uinfo);
}

void VTrackEditStoppedRipping(GripGUI *uinfo) {
  g_assert(uinfo->vtrack_buttons_sensitive == 0);
  VTrackEditSetSensitive(uinfo, buttons_that_were_sensitive, TRUE);
  buttons_that_were_sensitive = 0;
}

static void ChangeVTracknum(GripInfo *ginfo, int tracknum,
			    int vtracknum, gboolean adjust) {

  int num_tracks = ginfo->Disc.instance->info.num_tracks;
  DiscDataInstance *ins = &ginfo->Disc.instance->data;
  TrackData *track;
  int delta;

  track = &ins->tracks[tracknum];
  delta = vtracknum - track->vtracknum;

  do {
	  track->vtracknum += delta;
	  tracknum++;
	  track++;
  } while (adjust && tracknum < num_tracks);

  UpdateCurrentTrackset(ginfo);
}


void VTracknumChanged(GtkWidget *widget, gpointer data) {
  GripInfo *ginfo = data;
  GripGUI *uinfo  = &ginfo->gui_info;
  int tracknum, newtracknum;
  gboolean adjust;

  g_assert(ginfo->Disc.instance != &ginfo->Disc.p_instance);

  tracknum = CURRENT_TRACK;
  newtracknum = gtk_spin_button_get_value_as_int(
      GTK_SPIN_BUTTON(uinfo->set_vtracknum_spin_button));

  adjust = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(uinfo->set_vtracknum_adjust_check_button));

  ChangeVTracknum(ginfo, tracknum, newtracknum, adjust);
}


/* Return values:
 *  0 - Just a regular line
 *  1 - New VAlbum start
 */
int vtrack_process_line(Disc *Disc, DiscInstance *vins, char buf[]) {
  DiscDataInstance *vdins = &vins->data;
  DiscInfoInstance *viins = &vins->info;
  DiscDataInstance *pdins = &Disc->p_instance.data;
  DiscInfoInstance *piins = &Disc->p_instance.info;
  int n;
  char *tname, *aname;
  char *dtitle, *dartist;
  int dyear;
  int start_frame, end_frame;
  int vtracknum;

  /*
   * Make sure there's enough room for the next track.
   */

  if(sscanf(buf, "VTITLE%d=", &n) == 1
     || sscanf(buf, "VARTIST%d=", &n) == 1
     || sscanf(buf, "VFRAMES%d=", &n) == 1
     || sscanf(buf, "VTRACKNUM%d=", &n) == 1) {

	if (n >= MAX_TRACKS) {
		extern void DisplayMsg(char *msg);
		/* Tell the user the track number counting from 1. */
		DisplayMsg(_("Can only have 100 tracks per trackset."));
		return 0;
	}

	if (n + 1 > viins->num_tracks) {
		if (viins->num_tracks != 0  &&  viins->num_tracks != n) {
			g_warning(_("vtrack %d follows vtrack %d, not %d.\n"),
				  n, viins->num_tracks - 1, n - 1);
			Disc->vtrack_problems = TRUE;
		}

		/* Don't set num_tracks based on a VTRACKNUM line.  This
		 * lets us set just the first track's vtracknum, and let
		 * the rest be filled in automatically. */
		if (strncmp(buf, "VTRACKNUM", 9)) {
			viins->num_tracks = n + 1;
		}
	}
  }

  /* Comment, no leading spaces for now. */
  if (buf[0] == '#') {

  } else if (sscanf(buf, "VTITLE%d=%m[^\r\n]", &n, &tname) == 2) {
	int ostrlen = strlen(vdins->tracks[n].track_name);
	strncat(vdins->tracks[n].track_name, tname, sizeof(vdins->tracks[n].track_name) - ostrlen);
	if (strlen(vdins->tracks[n].track_name) != ostrlen + strlen(tname)) {
	  g_warning(_("vtrack %d title too long"), n);
	  Disc->vtrack_problems = TRUE;
	}
	free(tname);
  } else if (sscanf(buf, "VARTIST%d=%m[^\r\n]", &n, &aname) == 2) {
	int ostrlen = strlen(vdins->tracks[n].track_artist);
	strncat(vdins->tracks[n].track_artist, aname, sizeof(vdins->tracks[n].track_artist) - ostrlen);
	if (strlen(vdins->tracks[n].track_artist) != ostrlen + strlen(aname)) {
	  g_warning(_("vtrack %d artist too long"), n);
	  Disc->vtrack_problems = TRUE;
	}
	free(aname);
  } else if (sscanf(buf, "VTRACKNUM%d=%d", &n, &vtracknum) == 2) {
	  if (vtracknum < 0) {
		  g_warning("vtrack %d vtracknum can't be negative (%d)",
			    n, vtracknum);
		  Disc->vtrack_problems = TRUE;
	  } else if (vdins->tracks[n].vtracknum != -1) {
		  g_warning(_("vtrack %d vtracknum respecified"), n);
		  Disc->vtrack_problems = TRUE;
	  } else {
		  vdins->tracks[n].vtracknum = vtracknum;
	  }
  } else if (sscanf(buf, "VFRAMES%d=", &n) == 1  && strchr(buf, '=')) {
	int start_track, end_track;
	char *b = strchr(buf, '=') + 1;
	int disc_last_frame;

	if (sscanf(b, "%d-%d", &start_frame, &end_frame) == 2) {
	} else if (sscanf(b, "%d-PTRACK%d", &start_frame, &end_track) == 2) {
	  end_frame = piins->tracks[end_track].start_frame +
	  	          piins->tracks[end_track].num_frames - 1;
	} else if (sscanf(b, "PTRACK%d-%d", &start_track, &end_frame) == 2) {
	  start_frame = piins->tracks[start_track].start_frame;
	} else if (sscanf(b, "PTRACK%d-PTRACK%d", &start_track, &end_track) == 2) {
	  start_frame = piins->tracks[start_track].start_frame;
	  end_frame = piins->tracks[end_track].start_frame +
	  	          piins->tracks[end_track].num_frames - 1;
	} else if (sscanf(b, "PTRACK%d", &start_track) == 1) {
	  start_frame = piins->tracks[start_track].start_frame;
	  end_frame = piins->tracks[start_track].start_frame +
	  	          piins->tracks[start_track].num_frames - 1;
	  if (vdins->tracks[n].track_name[0] == '\0') {
		strcpy(vdins->tracks[n].track_name, pdins->tracks[start_track].track_name);
	  }
	} else {
	  g_warning(_("Invalid vtrackinfo VFRAMES line:\n%s"), buf);
	  start_frame = 0; end_frame = 0;
	  Disc->vtrack_problems = TRUE;
	}

	if (start_frame < 0) {
	  g_warning(_("vtrack %d had start_frame %d, less than 0\n"),
		    n, start_frame);
	  start_frame = end_frame = 0;
	  Disc->vtrack_problems = TRUE;
	}

	disc_last_frame = piins->tracks[piins->num_tracks].start_frame - 1;
	if (end_frame > disc_last_frame) {
	  g_warning(_("vtrack %d had end_frame %d greater than "
		      "disc last frame %d\n"),
				n, end_frame, disc_last_frame);
	  end_frame = disc_last_frame;
	  Disc->vtrack_problems = TRUE;
	}
	if (start_frame > end_frame) {
	  g_warning(_("vtrack %d had start_frame %d after end_frame %d\n"),
				n, start_frame, end_frame);
	  end_frame = start_frame;
	  Disc->vtrack_problems = TRUE;
	}

	InitTrack(&viins->tracks[n], start_frame, end_frame - start_frame);

  } else if (sscanf(buf, "VDTITLE=%m[^\r\n]", &dtitle) == 1) {
	int ostrlen = strlen(vdins->title);
	strncat(vdins->title, dtitle, sizeof(vdins->title) - ostrlen);
	if (strlen(vdins->title) != ostrlen + strlen(dtitle)) {
	  g_warning(_("virtual album title %s... too long"), vdins->title);
	  Disc->vtrack_problems = TRUE;
	}
	free(dtitle);
  } else if (sscanf(buf, "VDARTIST=%m[^\r\n]", &dartist) == 1) {
	int ostrlen = strlen(vdins->artist);
	strncat(vdins->artist, dartist, sizeof(vdins->artist) - ostrlen);
	if (strlen(vdins->artist) != ostrlen + strlen(dartist)) {
	  g_warning(_("virtual album artist %s... too long"), vdins->artist);
	  Disc->vtrack_problems = TRUE;
	}
	free(dartist);
  } else if (sscanf(buf, "VYEAR=%d", &dyear) == 1) {
	vdins->year = dyear;
  } else if (strstr(buf, "VNEWALBUM")) {
	return 1;
  } else if (strchr(buf, '#')) {
	/* XXX For now, we'll assume that any "invalid" line with a # in it
	 * anywhere is a comment. */
  } else {
	g_warning(_("Invalid line in vtrack info file: \"%s\""),
		  g_strstrip(buf));
	Disc->vtrack_problems = TRUE;
  }
  return 0;
}


/* Read from the local database */
int DiscDBReadVirtTracksFromvtrackinfo(Disc *Disc, char *root_dir) {
  DiscData *ddata = &Disc->data;
  FILE *vtrack_data;
  char file[256];

  /* handle virtual track things */
  Disc->vtrack_problems = FALSE;
  g_snprintf(file, 256, "%s/%08x.vtrackinfo", root_dir, ddata->data_id);
  vtrack_data = fopen(file, "r");
  if (vtrack_data != NULL) {
	DiscInstance *vins;
	int status;
	int another = 1;
	char inbuf[256];

	g_assert(Disc->v_instance == NULL);
	g_assert(Disc->num_vtrack_sets == 0);

	while (another) {
	  int n;

	  n = VTracksetAddEmpty(Disc);
	  vins = &Disc->v_instance[n];

	  another = 0;
	  while(fgets(inbuf, 512, vtrack_data)) {
		status = vtrack_process_line(Disc, vins, inbuf);
		if (status == 0)
		  continue;
		if (status == 1) {
		  another = 1;
		  break;
		}
		g_assert(0);
	  }

	  VTracksetFinishInit(Disc, vins, 0, -1);
	}

	fclose(vtrack_data);

  } else if (errno != ENOENT) {
	g_warning(_("can't open vtrackinfo file %s: %s\n"), file, g_strerror(errno));
  }

  return 0;
}
