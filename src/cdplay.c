/* cdplay.c
 *
 * Copyright (c) 1998-2001  Mike Oliphant <grip@nostatic.org>
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

#include "cdplay.h"
#include "grip.h"
#include "config.h"
#include "common.h"
#include "discdb.h"
#include "cddev.h"
#include "discedit.h"
#include "dialog.h"
#include "rip.h"
#include "tray.h"
#include "grip_id3.h"
#include "vtracks.h"

static void ShutDownCB(GtkWidget *widget,gpointer data);
static void DiscDBToggle(GtkWidget *widget,gpointer data);
static void DoLookup(void *data);
static void SetCurrentTrack(GripInfo *ginfo,int track);
static void ToggleChecked(GripGUI *uinfo,int track);
static void ClickColumn(GtkTreeViewColumn *column,gpointer data);
static gboolean TracklistButtonPressed(GtkWidget *widget,GdkEventButton *event,
				       gpointer data);
static void SelectRow(GripInfo *ginfo,int track);
void SelectionChanged(GtkTreeSelection *selection,gpointer data);
static void PlaylistChanged(GtkWindow *window,GtkWidget *widget,gpointer data);
static void ToggleLoop(GtkWidget *widget,gpointer data);
static void ChangePlayMode(GtkWidget *widget,gpointer data);
static void ChangeTimeMode(GtkWidget *widget,gpointer data);
static void ToggleProg(GtkWidget *widget,gpointer data);
static void ToggleControlButtons(GtkWidget *widget,GdkEventButton *event,
				 gpointer data);
static void ToggleVol(GtkWidget *widget,gpointer data);
static void SetVolume(GtkWidget *widget,gpointer data);
static void FastFwdCB(GtkWidget *widget,gpointer data);
static void RewindCB(GtkWidget *widget,gpointer data);
static void RewindReleaseCB(GtkWidget *widget,gpointer data);
static void NextDisc(GtkWidget *widget,gpointer data);
static void PlayTrack(GripInfo *ginfo,int track);
static void InitProgram(GripInfo *ginfo);
static void ShuffleTracks(GripInfo *ginfo);
static gboolean CheckTracks(Disc *Disc);
static void MakeATrackPage(GripInfo *ginfo, DiscInstance *ins, DiscGuiInstance *gins, char *title);

void ChangeTracks(GripInfo *ginfo, DiscInstance *ins, DiscGuiInstance *gins);

static void RemoveVTracks(GripInfo *ginfo);
static void UpdateTrackTabColors(GripInfo *ginfo);


static void ShutDownCB(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  GripDie(ginfo->gui_info.app,NULL);
}

static void DiscDBToggle(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;
  
  if(ginfo->looking_up) {
    return;
  }
  else {
    if(ginfo->ripping_a_disc) {
      gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
                        _("Cannot do lookup while ripping."));

      return;
    }
 
    if(ginfo->have_disc)
      LookupDisc(ginfo,TRUE);
  }
}

void LookupDisc(GripInfo *ginfo,gboolean manual)
{
  int track;
  gboolean present;
  Disc *Disc = &ginfo->Disc;
  DiscData *ddata;
  DiscDataInstance *pins;

  ddata = &Disc->data;

  if (!Disc->instance)
	Disc->instance = &Disc->p_instance;

  RemoveVTracks(ginfo);

  pins = &Disc->instance->data;

  pins->multi_artist = FALSE;
  pins->year = 0;

  present = DiscDBStatDiscData(&ginfo->Disc);

  if(!manual&&present) {
	  DiscDBReadDiscData(&ginfo->Disc, ginfo->discdb_encoding);

    ginfo->update_required=TRUE;
    ginfo->is_new_disc=TRUE;
  } else {
    if(!manual) {
      ddata->data_id = DiscDBDiscid(&ginfo->Disc);
	  pins->genre = 7; /* "misc" */
      strcpy(pins->title, _("Unknown Disc"));
      strcpy(pins->artist, "");
      
      for(track = 0; track < ginfo->Disc.instance->info.num_tracks; track++) {
	sprintf(pins->tracks[track].track_name, _("Track %02d"),track+1);
	pins->tracks[track].track_artist[0] = '\0';
	pins->tracks[track].track_extended[0] = '\0';
	pins->tracks[track].vtracknum = track + 1;
	pins->playlist[0] = '\0';
      }

      pins->extended[0] = '\0';

      ginfo->update_required=TRUE;
    }

    if(!ginfo->local_mode && (manual?TRUE:ginfo->automatic_discdb)) {
      ginfo->looking_up=TRUE;
      
      pthread_create(&(ginfo->discdb_thread),NULL,(void *)&DoLookup,
		     (void *)ginfo);
      pthread_detach(ginfo->discdb_thread);
    }
  }
}

static void DoLookup(void *data)
{
  GripInfo *ginfo = (GripInfo *)data;
  DiscDataInstance *pins = &ginfo->Disc.p_instance.data;

  g_assert(ginfo->Disc.v_instance == NULL);

  if(!DiscDBLookupDisc(ginfo,&(ginfo->dbserver))) {
    if(*(ginfo->dbserver2.name)) {
      if(DiscDBLookupDisc(ginfo,&(ginfo->dbserver2))) {
        ginfo->ask_submit=TRUE;
      }
    }
  }

  if(pins->id3genre == -1)
    pins->id3genre = DiscDB2ID3(pins->genre);

  ginfo->looking_up=FALSE;
  pthread_exit(0);
}

gboolean DiscDBLookupDisc(GripInfo *ginfo,DiscDBServer *server)
{
  DiscDBHello hello;
  DiscDBQuery query;
  DiscDBEntry entry;
  gboolean success=FALSE;

  g_assert(ginfo->Disc.v_instance == NULL);

  if(server->use_proxy)
    LogStatus(ginfo,_("Querying %s (through %s) for disc %02x.\n"),
	      server->name,
	      server->proxy->name,
	      DiscDBDiscid(&ginfo->Disc));
  else
    LogStatus(ginfo,_("Querying %s for disc %02x.\n"),server->name,
	      DiscDBDiscid(&ginfo->Disc));

  strncpy(hello.hello_program,"Grip",256);
  strncpy(hello.hello_version,VERSION,256);

  if(ginfo->db_use_freedb && !strcasecmp(ginfo->discdb_encoding,"UTF-8"))
    hello.proto_version=6;
  else
    hello.proto_version=5;
	
  if(!DiscDBDoQuery(&ginfo->Disc, server, &hello, &query)) {
    ginfo->update_required=TRUE;
  } else {
	  int track = 0;
    switch(query.query_match) {
    case MATCH_INEXACT: {
	int discid = DiscDBDiscid(&ginfo->Disc);
	int i;

	for (i = 0; i < query.query_matches; i++) {
		/* XXX If there's more than one, we should create a
		 * vtrackset for each */
		if (query.query_list[i].list_id == discid) {
			track = i;
			break;
		}
	}
    }
	/* FALLTHROUGH */
    case MATCH_EXACT:
      LogStatus(ginfo,_("Match for \"%s / %s\"\nDownloading data...\n"),
		query.query_list[track].list_artist,
                query.query_list[track].list_artist);

      entry.entry_genre = query.query_list[track].list_genre;
      entry.entry_id = query.query_list[track].list_id;
      DiscDBRead(&ginfo->Disc, server, &hello, &entry, ginfo->discdb_encoding);

      Debug(_("Done\n"));
      success=TRUE;
		
      if(DiscDBWriteDiscData(&ginfo->Disc, NULL, TRUE, FALSE,"utf-8") < 0)
	g_print(_("Error saving disc data\n"));

      ginfo->update_required=TRUE;
      ginfo->is_new_disc=TRUE;
      break;
    case MATCH_NOMATCH:
      LogStatus(ginfo,_("No match\n"));
      break;
    }
  }

  return success;
}

void MakeVTrackPage(GripInfo *ginfo, DiscInstance *vins, DiscGuiInstance *vgins) {
  MakeATrackPage(ginfo, vins, vgins, "VTracks");
}

void RemoveVTrackPage(GripInfo *ginfo, DiscGuiInstance *vgins)
{
  GripGUI *uinfo = &ginfo->gui_info;
  int pagenum = gtk_notebook_page_num(GTK_NOTEBOOK(uinfo->notebook),
				      vgins->trackpage);

  if (gtk_notebook_get_current_page(GTK_NOTEBOOK(uinfo->notebook)) == pagenum)
	gtk_notebook_set_page(GTK_NOTEBOOK(uinfo->notebook), 0);

  /* XXX memory leak I'm sure */
  gtk_notebook_remove_page(GTK_NOTEBOOK(uinfo->notebook), pagenum);
}


void RemoveVTracks(GripInfo *ginfo) {
  GripGUI *uinfo = &ginfo->gui_info;
  Disc *Disc = &ginfo->Disc;
  DiscInstance     *ins;
  DiscGuiInstance  *gins;
  GtkWidget *w;

  g_assert(!ginfo->ripping_a_disc);

  if (!ginfo->have_disc)
	  return;

  VTrackEditPTrackset(uinfo);

  if (ginfo->Disc.num_vtrack_sets == 0)
	  return;

  ChangeTracks(ginfo, &Disc->p_instance, &uinfo->p_instance);

  g_assert(Disc->v_instance);
  g_assert(uinfo->v_instance);

  FOREACH_VTRACK_SET_WITH_GUI(Disc, uinfo, ins, gins) {
	RemoveVTrackPage(ginfo, gins);
  }

  Disc->num_vtrack_sets = 0;

  g_free(Disc->v_instance);
  Disc->v_instance = NULL;

  /* XXX Memory leak -- need to clean up GtkWidgets ? */
  g_free(uinfo->v_instance);
  uinfo->v_instance = NULL;

  /* Set label to default color */
  w = gtk_notebook_get_tab_label(GTK_NOTEBOOK(uinfo->notebook),
				 uinfo->p_instance.trackpage);
  g_assert(w);

  gtk_label_set_text(GTK_LABEL(w), uinfo->p_instance.tab_label_text);
}

int GetLengthRipWidth(GripInfo *ginfo)
{
  GtkWidget *track_list;
  int width,tot_width=0;
  PangoLayout *layout;

  track_list=ginfo->gui_info.instance->track_list;

  if(track_list) {
    layout=gtk_widget_create_pango_layout(GTK_WIDGET(track_list),
					_("Length"));

    pango_layout_get_size(layout,&width,NULL);
    
    g_object_unref(layout);
    
    tot_width+=width;
    
    layout=gtk_widget_create_pango_layout(GTK_WIDGET(track_list),
					_("Rip"));

    pango_layout_get_size(layout,&width,NULL);
    
    g_object_unref(layout);

    tot_width+=width;

    tot_width/=PANGO_SCALE;

    tot_width+=25;
  }

  return tot_width;
}


void ResizeTrackList(GripInfo *ginfo)
{
  GtkWidget *track_list;
  GtkTreeViewColumn *column;
  int tot_width=0;

  track_list=ginfo->gui_info.instance->track_list;

  if(track_list) {
    tot_width=GetLengthRipWidth(ginfo);
    column=gtk_tree_view_get_column(GTK_TREE_VIEW(track_list),
				    TRACKLIST_TRACK_COL);
    gtk_tree_view_column_set_fixed_width(column,track_list->
					 allocation.width-tot_width);
  }
}

void MakeTrackPage(GripInfo *ginfo) {
  MakeATrackPage(ginfo, &ginfo->Disc.p_instance, &ginfo->gui_info.p_instance, "Tracks");
}

void MakeATrackPage(GripInfo *ginfo, DiscInstance *ins, DiscGuiInstance *gins, char *title)
{
  GtkWidget *trackpage;
  GtkWidget *vbox;
  GripGUI *uinfo = &ginfo->gui_info;
  GtkRequisition sizereq;
  GtkTooltips *tabtooltips;
  GtkWidget *scroll;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkTreeSelection *select;

  trackpage=MakeNewPage(uinfo->notebook,_(title));
  gins->tab_label_text = _(title); /* When making this dymanic, remember to free it. */
  vbox=gtk_vbox_new(FALSE,0);
  gtk_container_border_width(GTK_CONTAINER(vbox),3);

  gins->disc_name_label=gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(vbox),gins->disc_name_label,FALSE,FALSE,0);
  gtk_widget_show(gins->disc_name_label);

  gins->disc_artist_label=gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(vbox),gins->disc_artist_label,FALSE,FALSE,0);
  gtk_widget_show(gins->disc_artist_label);


  gins->track_list_store=gtk_list_store_new(TRACKLIST_N_COLUMNS,
					     G_TYPE_STRING,
					     G_TYPE_STRING,
					     G_TYPE_BOOLEAN,
					     G_TYPE_INT);


  gins->track_list=
    gtk_tree_view_new_with_model(GTK_TREE_MODEL(gins->track_list_store));

  renderer=gtk_cell_renderer_text_new();

  column=gtk_tree_view_column_new_with_attributes(_("Track"),renderer,
						  "text",TRACKLIST_TRACK_COL,
						  NULL);

  gtk_tree_view_column_set_sizing(column,GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_fixed_width(column,
                                       uinfo->win_width-
                                       (GetLengthRipWidth(ginfo)+15));

  gtk_tree_view_append_column(GTK_TREE_VIEW(gins->track_list),column);

  column=gtk_tree_view_column_new_with_attributes(_("Length"),renderer,
						  "text",TRACKLIST_LENGTH_COL,
						  NULL);

  gtk_tree_view_column_set_alignment(column,0.5);

  gtk_tree_view_append_column(GTK_TREE_VIEW(gins->track_list),column);


  renderer=gtk_cell_renderer_toggle_new();

  column=gtk_tree_view_column_new_with_attributes(_("Rip"),renderer,
						  "active",
						  TRACKLIST_RIP_COL,
						  NULL);

  gtk_tree_view_column_set_alignment(column,0.5);
  gtk_tree_view_column_set_fixed_width(column,20);
  gtk_tree_view_column_set_sizing(column,GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_max_width(column,20);
  gtk_tree_view_column_set_clickable(column,TRUE);

  g_signal_connect(G_OBJECT(column),"clicked",
		   G_CALLBACK(ClickColumn),(gpointer)ginfo);

  gtk_tree_view_append_column(GTK_TREE_VIEW(gins->track_list),column);

  gins->rip_column = column;

  select=gtk_tree_view_get_selection(GTK_TREE_VIEW(gins->track_list));

  gtk_tree_selection_set_mode(select,GTK_SELECTION_SINGLE);

  g_signal_connect(G_OBJECT(select),"changed",
		   G_CALLBACK(SelectionChanged),(gpointer)ginfo);

    
  g_signal_connect(G_OBJECT(gins->track_list),"button_press_event",
		   G_CALLBACK(TracklistButtonPressed),(gpointer)ginfo);



  /*  g_signal_connect(G_OBJECT(gins->track_list),"cursor_changed",
		   G_CALLBACK(SelectRow),
		   (gpointer)ginfo);

  g_signal_connect(G_OBJECT(gins->track_list),"unselect_row",
		   G_CALLBACK(UnSelectRow),
		   (gpointer)uinfo);
  
  g_signal_connect(G_OBJECT(gins->track_list),"button_press_event",
		   G_CALLBACK(CListButtonPressed),(gpointer)uinfo);
  
  g_signal_connect(G_OBJECT(gins->track_list),"click_column",
  G_CALLBACK(ClickColumn),(gpointer)ginfo);*/


  scroll=gtk_scrolled_window_new(NULL,NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
				 GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(scroll),gins->track_list);

  gtk_box_pack_start(GTK_BOX(vbox),scroll,TRUE,TRUE,0);

  gtk_widget_show(scroll);

  gtk_widget_show(gins->track_list);

  gtk_widget_size_request(gins->track_list,&sizereq);
  //  gtk_widget_set_usize(trackpage,sizereq.width+30,-1);
  gtk_widget_set_usize(trackpage,500,-1);
  
  gtk_container_add(GTK_CONTAINER(trackpage),vbox);

  /* XXX this doesn't work.  Why not? */
  tabtooltips = gtk_tooltips_new();
  gtk_tooltips_set_tip(tabtooltips, trackpage, ins->data.title, NULL);
  gins->tabtooltips = tabtooltips;
  if (ins == ginfo->Disc.instance)
	  gtk_tooltips_disable(tabtooltips);
  else
	  gtk_tooltips_enable(tabtooltips);

  gins->trackpage = trackpage;

  gtk_widget_show(vbox);
}

void SetCurrentTrackIndex(GripInfo *ginfo,int track)
{
  /* Looks up the track of index track in the program */
  for(ginfo->current_track_index = 0;
      (ginfo->current_track_index < MAX_TRACKS)
	&& (ginfo->current_track_index < ginfo->prog_totaltracks)
	&& (CURRENT_TRACK != track);
      ginfo->current_track_index++)
    continue;
}

static void SetCurrentTrack(GripInfo *ginfo,int track)
{
  DiscDataInstance *ins = &ginfo->Disc.instance->data;
  char buf[256];
  int tracklen;

  GripGUI *uinfo;

  uinfo=&(ginfo->gui_info);

  if(track<0) {
    gtk_label_set(GTK_LABEL(uinfo->current_track_label),"--");
    gtk_entry_set_text(GTK_ENTRY(uinfo->start_sector_entry),"0");
    gtk_entry_set_text(GTK_ENTRY(uinfo->end_sector_entry),"0");

    g_signal_handlers_block_by_func(G_OBJECT(uinfo->track_edit_entry),
				     TrackEditChanged,(gpointer)ginfo);
    gtk_entry_set_text(GTK_ENTRY(uinfo->track_edit_entry),"");
    
    g_signal_handlers_unblock_by_func(G_OBJECT(uinfo->track_edit_entry),
	 			       TrackEditChanged,(gpointer)ginfo);
    
    g_signal_handlers_block_by_func(G_OBJECT(uinfo->
						track_artist_edit_entry),
	 			     TrackEditChanged,(gpointer)ginfo);
    
    gtk_entry_set_text(GTK_ENTRY(uinfo->track_artist_edit_entry),"");
    
    g_signal_handlers_unblock_by_func(G_OBJECT(uinfo->
						  track_artist_edit_entry),
	 			       TrackEditChanged,(gpointer)ginfo);
  }
  else {
    GtkAdjustment *adj;
    g_signal_handlers_block_by_func(G_OBJECT(uinfo->track_edit_entry),
				     TrackEditChanged,(gpointer)ginfo);
    gtk_entry_set_text(GTK_ENTRY(uinfo->track_edit_entry),
		       ins->tracks[track].track_name);

    g_signal_handlers_unblock_by_func(G_OBJECT(uinfo->track_edit_entry),
	 			       TrackEditChanged,(gpointer)ginfo);

    adj = gtk_spin_button_get_adjustment(
	GTK_SPIN_BUTTON(uinfo->set_vtracknum_spin_button));
    g_signal_handlers_block_by_func(G_OBJECT(adj), VTracknumChanged, ginfo);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(uinfo->set_vtracknum_spin_button),
					      ins->tracks[track].vtracknum);
    g_signal_handlers_unblock_by_func(G_OBJECT(adj), VTracknumChanged, ginfo);


    g_signal_handlers_block_by_func(G_OBJECT(uinfo->
						track_artist_edit_entry),
	 			     TrackEditChanged,(gpointer)ginfo);

    gtk_entry_set_text(GTK_ENTRY(uinfo->track_artist_edit_entry),
		       ins->tracks[track].track_artist);

    g_signal_handlers_unblock_by_func(G_OBJECT(uinfo->
						  track_artist_edit_entry),
	 			       TrackEditChanged,(gpointer)ginfo);
    g_snprintf(buf,80,"%02d",track+1);
    gtk_label_set(GTK_LABEL(uinfo->current_track_label),buf);
	
    gtk_entry_set_text(GTK_ENTRY(uinfo->start_sector_entry),"0");
	
    tracklen = ginfo->Disc.instance->info.tracks[track].num_frames;
    g_snprintf(buf,80,"%d",tracklen);
    gtk_entry_set_text(GTK_ENTRY(uinfo->end_sector_entry),buf);

    SetCurrentTrackIndex(ginfo,track);
  }
}

gboolean TrackIsChecked(GripGUI *uinfo,int track)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  gboolean checked;
  DiscGuiInstance *gins = uinfo->instance;

  path=gtk_tree_path_new_from_indices(track,-1);

  gtk_tree_model_get_iter(GTK_TREE_MODEL(gins->track_list_store),&iter,path);

  gtk_tree_model_get(GTK_TREE_MODEL(gins->track_list_store),
		     &iter,TRACKLIST_RIP_COL,&checked,-1);

  return checked;
}

static void ToggleChecked(GripGUI *uinfo,int track)
{
  SetChecked(uinfo,track,!TrackIsChecked(uinfo,track));
}

void SetChecked(GripGUI *uinfo,int track,gboolean checked)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  DiscGuiInstance *gins = uinfo->instance;

  path=gtk_tree_path_new_from_indices(track,-1);

  gtk_tree_model_get_iter(GTK_TREE_MODEL(gins->track_list_store),&iter,path);

  gtk_list_store_set(gins->track_list_store,&iter,
		     TRACKLIST_RIP_COL,checked,-1);

  gtk_tree_path_free(path);
}

static void aTreeViewColumnTouched(GripInfo *ginfo, GtkTreeViewColumn *column) {
  DiscInstance *ins;
  DiscGuiInstance *gins;

  if (column == ginfo->gui_info.instance->rip_column) {
	return;
  }

  FOREACH_TRACK_SET_WITH_GUI(&ginfo->Disc, &ginfo->gui_info, ins, gins) {
	if (column == gins->rip_column) {
		ChangeTracks(ginfo, ins, gins);
		ins = NULL;
		break;
	}
  }
  g_assert(ins == NULL);
}

static void aTreeViewTouched(GripInfo *ginfo, GtkTreeView *tree_view) {
  DiscInstance *ins;
  DiscGuiInstance *gins;

  if (GTK_WIDGET(tree_view) == ginfo->gui_info.instance->track_list) {
	return;
  }

  FOREACH_TRACK_SET_WITH_GUI(&ginfo->Disc, &ginfo->gui_info, ins, gins) {
	if (GTK_WIDGET(tree_view) == gins->track_list) {
		ChangeTracks(ginfo, ins, gins);
		ins = NULL;
		break;
	}
  }
  g_assert(ins == NULL);
}

static void ClickColumn(GtkTreeViewColumn *column,gpointer data)
{
  int track;
  int numsel=0;
  gboolean check;
  GripInfo *ginfo = (GripInfo *)data;
  int num_tracks;

  aTreeViewColumnTouched(ginfo, column);

  if(ginfo->have_disc) {
    num_tracks = ginfo->Disc.instance->info.num_tracks;
    for(track = 0; track < num_tracks; track++)
      if(TrackIsChecked(&(ginfo->gui_info),track)) numsel++;

    if(num_tracks > 1) {
      check = (numsel < num_tracks/2);
    }
    else {
      check=(numsel==0);
    }

    for(track = 0; track < num_tracks; track++)
      SetChecked(&(ginfo->gui_info),track,check);
  }
}

#if 0
/* XXXCONV CListButtonPressed gone */
static void CListButtonPressed(GtkWidget *widget,GdkEventButton *event,
			       gpointer data)
{
  gint row,col;
  GripInfo *ginfo = (GripInfo *)data;
  GripGUI *uinfo = &ginfo->gui_info;

  aCListTouched(ginfo, widget);

  if(event) {
    gtk_clist_get_selection_info(GTK_CLIST(uinfo->instance->trackclist),
				 event->x,event->y,
				 &row,&col);
    Debug(_("Column/Button: %d/%d\n"),col,event->button);


    if((col==2&&event->button<4) || event->button==3) {
      
#ifndef GRIPCD
      ToggleChecked(uinfo,row);
#endif
    }
  }
}

/* XXXCONV UnSelectRow gone */
static void UnSelectRow(GtkWidget *widget,gint row,gint column,
			GdkEventButton *event,gpointer data)
{
  GripGUI *uinfo;

  uinfo=(GripGUI *)data;

#ifndef GRIPCD
  if(TrackIsChecked(uinfo,row))
    gtk_clist_set_pixmap(GTK_CLIST(uinfo->instance->trackclist),row,2,
			 GTK_PIXMAP(uinfo->check_image)->pixmap,
			 GTK_PIXMAP(uinfo->check_image)->mask);
#endif
}
#endif

static gboolean TracklistButtonPressed(GtkWidget *widget,GdkEventButton *event,
				       gpointer data)
{
  GripInfo *ginfo;
  GripGUI *uinfo;
  GtkTreeViewColumn *column;
  GtkTreePath *path;
  int *indices;
  GList *cols;
  int row,col;

  ginfo=(GripInfo *)data;

  aTreeViewTouched(ginfo, GTK_TREE_VIEW(widget));


  uinfo=&(ginfo->gui_info);

  if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(uinfo->instance->track_list),
                                   event->x,event->y,
                                   &path,&column,NULL,NULL)) {
    indices=gtk_tree_path_get_indices(path);

    row=indices[0];
  
    cols=gtk_tree_view_get_columns(GTK_TREE_VIEW(column->tree_view));

    col=g_list_index(cols,(gpointer)column);

    g_list_free(cols);

    if(event->type==GDK_BUTTON_PRESS) {
      if((event->button>1) || (col==2)) {
        ToggleChecked(uinfo,row);
      }
    }
  }

  return FALSE;
}

static void SelectRow(GripInfo *ginfo,int track)
{
  GtkTreePath *path;
  GtkTreeSelection *select;
  DiscGuiInstance *gins = ginfo->gui_info.instance;
 
  path=gtk_tree_path_new_from_indices(track,-1);

  select=
    gtk_tree_view_get_selection(GTK_TREE_VIEW(gins->track_list));

  gtk_tree_selection_select_path(select,path);

  gtk_tree_path_free(path);
}

void SelectionChanged(GtkTreeSelection *selection,gpointer data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  int row=-1;
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  if(gtk_tree_selection_get_selected(selection,&model,&iter)) {
    gtk_tree_model_get(model,&iter,TRACKLIST_NUM_COL,&row,-1);
  }

  if(row!=-1)
    SetCurrentTrack(ginfo,row);

  if((ginfo->Disc.info.disc_mode==CDAUDIO_PLAYING)&&
     (ginfo->Disc.info.curr_track!=(row+1)))
    PlayTrack(ginfo,row);
}

static void PlaylistChanged(GtkWindow *window,GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  strcpy(ginfo->Disc.instance->data.playlist,
	 gtk_entry_get_text(GTK_ENTRY(ginfo->gui_info.playlist_entry)));

  InitProgram(ginfo);

  if(DiscDBWriteDiscData(&ginfo->Disc, NULL, TRUE, FALSE,
			 "utf-8") < 0)
    gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
		      _("Error saving disc data\n"));
}

static void ToggleLoop(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  ginfo->playloop=!ginfo->playloop;

  if(ginfo->playloop) 
    CopyPixmap(GTK_PIXMAP(ginfo->gui_info.loop_image),\
	       GTK_PIXMAP(ginfo->gui_info.loop_indicator));
  else
    CopyPixmap(GTK_PIXMAP(ginfo->gui_info.noloop_image),
	       GTK_PIXMAP(ginfo->gui_info.loop_indicator));

}

static void ChangePlayMode(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  ginfo->play_mode=(ginfo->play_mode+1)%PM_LASTMODE;

  CopyPixmap(GTK_PIXMAP(ginfo->gui_info.play_pix[ginfo->play_mode]),
	     GTK_PIXMAP(ginfo->gui_info.play_indicator));

  gtk_widget_set_sensitive(GTK_WIDGET(ginfo->gui_info.playlist_entry),
			   ginfo->play_mode==PM_PLAYLIST);

  InitProgram(ginfo);
}

GtkWidget *MakePlayOpts(GripInfo *ginfo)
{
  GripGUI *uinfo;
  GtkWidget *ebox;
  GtkWidget *hbox;
  GtkWidget *button;

  uinfo=&(ginfo->gui_info);

  ebox=gtk_event_box_new();
  gtk_widget_set_style(ebox,uinfo->style_wb);

  hbox=gtk_hbox_new(FALSE,2);

  uinfo->playlist_entry=gtk_entry_new_with_max_length(256);
  g_signal_connect(G_OBJECT(uinfo->playlist_entry),"focus_out_event",
  		     G_CALLBACK(PlaylistChanged),(gpointer)ginfo);
  gtk_widget_set_sensitive(GTK_WIDGET(uinfo->playlist_entry),
			   ginfo->play_mode==PM_PLAYLIST);
  gtk_box_pack_start(GTK_BOX(hbox),uinfo->playlist_entry,TRUE,TRUE,0);
  gtk_widget_show(uinfo->playlist_entry);

  uinfo->play_indicator=NewBlankPixmap(uinfo->app);
  CopyPixmap(GTK_PIXMAP(uinfo->play_pix[ginfo->play_mode]),
	     GTK_PIXMAP(uinfo->play_indicator));

  button=gtk_button_new();
  gtk_container_add(GTK_CONTAINER(button),uinfo->play_indicator);
  gtk_widget_show(uinfo->play_indicator);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
  g_signal_connect(G_OBJECT(button),"clicked",
  		     G_CALLBACK(ChangePlayMode),(gpointer)ginfo);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Rotate play mode"),NULL);
  gtk_widget_show(button);

  uinfo->loop_indicator=NewBlankPixmap(uinfo->app);

  if(ginfo->playloop)
    CopyPixmap(GTK_PIXMAP(uinfo->loop_image),
	       GTK_PIXMAP(uinfo->loop_indicator));
  else
    CopyPixmap(GTK_PIXMAP(uinfo->noloop_image),
	       GTK_PIXMAP(uinfo->loop_indicator));

  button=gtk_button_new();
  gtk_container_add(GTK_CONTAINER(button),uinfo->loop_indicator);
  gtk_widget_show(uinfo->loop_indicator);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
  g_signal_connect(G_OBJECT(button),"clicked",
  		     G_CALLBACK(ToggleLoop),(gpointer)ginfo);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Toggle loop play"),NULL);
  gtk_widget_show(button);

  gtk_container_add(GTK_CONTAINER(ebox),hbox);
  gtk_widget_show(hbox);

  return ebox;
}

GtkWidget *MakeControls(GripInfo *ginfo)
{
  GripGUI *uinfo;
  GtkWidget *vbox,*vbox3,*hbox,*imagebox,*hbox2;
  GtkWidget *indicator_box;
  GtkWidget *button;
  GtkWidget *ebox,*lcdbox;
  GtkObject *adj;
  int mycpu;

  uinfo=&(ginfo->gui_info);

  ebox=gtk_event_box_new();
  gtk_widget_set_style(ebox,uinfo->style_wb);

  vbox=gtk_vbox_new(FALSE,0);
  gtk_container_border_width(GTK_CONTAINER(vbox),0);

  vbox3=gtk_vbox_new(FALSE,2);
  gtk_container_border_width(GTK_CONTAINER(vbox3),2);

  lcdbox=gtk_event_box_new();
  g_signal_connect(G_OBJECT(lcdbox),"button_press_event",
		     G_CALLBACK(ToggleControlButtons),(gpointer)ginfo);
  gtk_widget_set_style(lcdbox,uinfo->style_LCD);

  hbox2=gtk_hbox_new(FALSE,0);

  imagebox=gtk_vbox_new(FALSE,0);

  gtk_box_pack_start(GTK_BOX(imagebox),uinfo->upleft_image,FALSE,FALSE,0);
  gtk_widget_show(uinfo->upleft_image);

  gtk_box_pack_end(GTK_BOX(imagebox),uinfo->lowleft_image,FALSE,FALSE,0);
  gtk_widget_show(uinfo->lowleft_image);

  gtk_box_pack_start(GTK_BOX(hbox2),imagebox,FALSE,FALSE,0);
  gtk_widget_show(imagebox);
  
  hbox=gtk_hbox_new(TRUE,0);
  gtk_container_border_width(GTK_CONTAINER(hbox),0);

  uinfo->current_track_label=gtk_label_new("--");
  gtk_box_pack_start(GTK_BOX(hbox),uinfo->current_track_label,FALSE,FALSE,0);
  gtk_widget_show(uinfo->current_track_label);

  button=gtk_button_new();
  gtk_widget_set_style(button,uinfo->style_LCD);

  gtk_button_set_relief(GTK_BUTTON(button),GTK_RELIEF_NONE);

  g_signal_connect(G_OBJECT(button),"clicked",
		     G_CALLBACK(ChangeTimeMode),(gpointer)ginfo);

  uinfo->play_time_label=gtk_label_new("--:--");
  gtk_container_add(GTK_CONTAINER(button),uinfo->play_time_label);
  gtk_widget_show(uinfo->play_time_label);

  gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
  gtk_widget_show(button);

  indicator_box=gtk_hbox_new(TRUE,0);

  uinfo->rip_indicator=NewBlankPixmap(GTK_WIDGET(uinfo->app));
  gtk_box_pack_start(GTK_BOX(indicator_box),uinfo->rip_indicator,TRUE,TRUE,0);
  gtk_widget_show(uinfo->rip_indicator);

  uinfo->lcd_smile_indicator=NewBlankPixmap(GTK_WIDGET(uinfo->app));
  gtk_tooltips_set_tip(MakeToolTip(),uinfo->lcd_smile_indicator,
		       _("Rip status"),NULL);
  gtk_box_pack_start(GTK_BOX(indicator_box),uinfo->lcd_smile_indicator,
		     TRUE,TRUE,0);
  gtk_widget_show(uinfo->lcd_smile_indicator);

  for(mycpu=0;mycpu<ginfo->num_cpu;mycpu++){
    uinfo->mp3_indicator[mycpu]=NewBlankPixmap(GTK_WIDGET(uinfo->app));
    gtk_box_pack_start(GTK_BOX(indicator_box),
		       uinfo->mp3_indicator[mycpu],TRUE,TRUE,0);
    gtk_widget_show(uinfo->mp3_indicator[mycpu]);
  }
  
  uinfo->discdb_indicator=NewBlankPixmap(GTK_WIDGET(uinfo->app));
  gtk_box_pack_start(GTK_BOX(indicator_box),uinfo->discdb_indicator,
		     TRUE,TRUE,0);
  gtk_widget_show(uinfo->discdb_indicator);

  gtk_box_pack_start(GTK_BOX(hbox),indicator_box,TRUE,TRUE,0);
  gtk_widget_show(indicator_box);

  gtk_container_add(GTK_CONTAINER(hbox2),hbox);
  gtk_widget_show(hbox);

  imagebox=gtk_vbox_new(FALSE,0);

  gtk_box_pack_start(GTK_BOX(imagebox),uinfo->upright_image,FALSE,FALSE,0);
  gtk_widget_show(uinfo->upright_image);

  gtk_box_pack_end(GTK_BOX(imagebox),uinfo->lowright_image,FALSE,FALSE,0);
  gtk_widget_show(uinfo->lowright_image);

  gtk_box_pack_start(GTK_BOX(hbox2),imagebox,FALSE,FALSE,0);
  gtk_widget_show(imagebox);
  
  gtk_container_add(GTK_CONTAINER(lcdbox),hbox2);
  gtk_widget_show(hbox2);

  gtk_box_pack_start(GTK_BOX(vbox3),lcdbox,FALSE,FALSE,0);
  gtk_widget_show(lcdbox);

  gtk_box_pack_start(GTK_BOX(vbox),vbox3,FALSE,FALSE,0);
  gtk_widget_show(vbox3);

  adj=gtk_adjustment_new((gfloat)ginfo->volume,0.0,255.0,1.0,1.0,0.0);
  g_signal_connect(adj,"value_changed",
  		     G_CALLBACK(SetVolume),(gpointer)ginfo);
  uinfo->volume_control=gtk_hscale_new(GTK_ADJUSTMENT(adj));

  gtk_scale_set_draw_value(GTK_SCALE(uinfo->volume_control),FALSE);
  gtk_widget_set_name(uinfo->volume_control,"darkgrey");
  gtk_box_pack_start(GTK_BOX(vbox),uinfo->volume_control,FALSE,FALSE,0);

  /*  CDGetVolume(cd_desc,&vol);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(adj),(vol.vol_front.left+
  vol.vol_front.right)/2);*/

  if(uinfo->volvis) gtk_widget_show(uinfo->volume_control);

  uinfo->control_button_box=gtk_vbox_new(TRUE,0);

  hbox=gtk_hbox_new(TRUE,0);

  button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->playpaus_image);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  g_signal_connect(G_OBJECT(button),"clicked",
  		     G_CALLBACK(PlayTrackCB),(gpointer)ginfo);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Play track / Pause play"),NULL);
  gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
  gtk_widget_show(button);

  button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->rew_image);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  g_signal_connect(G_OBJECT(button),"pressed",
  		     G_CALLBACK(RewindCB),(gpointer)ginfo);
  g_signal_connect(G_OBJECT(button),"released",
  		     G_CALLBACK(RewindReleaseCB),(gpointer)ginfo);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Rewind"),NULL);
  gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
  gtk_widget_show(button);

  button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->ff_image);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  g_signal_connect(G_OBJECT(button),"pressed",
  		     G_CALLBACK(FastFwdCB),(gpointer)ginfo);
  g_signal_connect(G_OBJECT(button),"released",
  		     G_CALLBACK(FastFwdCB),(gpointer)ginfo);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("FastForward"),NULL);
  gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
  gtk_widget_show(button);

  button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->prevtrk_image);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
  g_signal_connect(G_OBJECT(button),"clicked",
  		     G_CALLBACK(PrevTrackCB),(gpointer)ginfo);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Go to previous track"),NULL);
  gtk_widget_show(button);

  button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->nexttrk_image);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
  g_signal_connect(G_OBJECT(button),"clicked",
  		     G_CALLBACK(NextTrackCB),(gpointer)ginfo);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Go to next track"),NULL);
  gtk_widget_show(button);

  button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->progtrack_image);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
  g_signal_connect(G_OBJECT(button),"clicked",
    		     G_CALLBACK(ToggleProg),(gpointer)ginfo);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Toggle play mode options"),NULL);
  gtk_widget_show(button);

  if(ginfo->changer_slots>1) {
    button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->rotate_image);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    g_signal_connect(G_OBJECT(button),"clicked",
    		       G_CALLBACK(NextDisc),(gpointer)ginfo);
    gtk_tooltips_set_tip(MakeToolTip(),button,
			 _("Next Disc"),NULL);
    gtk_widget_show(button);
  }

  gtk_box_pack_start(GTK_BOX(uinfo->control_button_box),hbox,TRUE,TRUE,0);
  gtk_widget_show(hbox);

  hbox=gtk_hbox_new(TRUE,0);

  button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->stop_image);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  g_signal_connect(G_OBJECT(button),"clicked",
  		     G_CALLBACK(StopPlayCB),(gpointer)ginfo);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Stop play"),NULL);
  gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
  gtk_widget_show(button);

  button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->eject_image);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
  g_signal_connect(G_OBJECT(button),"clicked",
  		     G_CALLBACK(EjectDisc),(gpointer)ginfo);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Eject disc"),NULL);
  gtk_widget_show(button);

  button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->cdscan_image);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
  g_signal_connect(G_OBJECT(button),"clicked",
  		     G_CALLBACK(ScanDisc),(gpointer)ginfo);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Scan Disc Contents"),NULL);
  gtk_widget_show(button);

  button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->vol_image);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  g_signal_connect(G_OBJECT(button),"clicked",
  		     G_CALLBACK(ToggleVol),(gpointer)ginfo);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Toggle Volume Control"),NULL);
  gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
  gtk_widget_show(button);

  button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->edit_image);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  g_signal_connect(G_OBJECT(button),"clicked",
  		     G_CALLBACK(ToggleTrackEdit),(gpointer)ginfo);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Toggle disc editor"),NULL);
  gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
  gtk_widget_show(button);

  if(!ginfo->local_mode) {
    button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->discdbwht_image);
    gtk_widget_set_style(button,uinfo->style_dark_grey);
    g_signal_connect(G_OBJECT(button),"clicked",
    		       G_CALLBACK(DiscDBToggle),(gpointer)ginfo);
    gtk_tooltips_set_tip(MakeToolTip(),button,
			 _("Initiate/abort DiscDB lookup"),NULL);
    gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
    gtk_widget_show(button);
  }

  button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->minmax_image);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  g_signal_connect(G_OBJECT(button),"clicked",
  		     G_CALLBACK(MinMax),(gpointer)ginfo);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Toggle track display"),NULL);
  gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
  gtk_widget_show(button);

  button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->quit_image);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Exit Grip"),NULL);

  gtk_box_pack_start(GTK_BOX(hbox),button,TRUE,TRUE,0);
  g_signal_connect(G_OBJECT(button),"clicked",
  		     G_CALLBACK(ShutDownCB),(gpointer)ginfo);
  gtk_widget_show(button);
  
  gtk_box_pack_start(GTK_BOX(uinfo->control_button_box),hbox,TRUE,TRUE,0);
  gtk_widget_show(hbox);

  gtk_box_pack_start(GTK_BOX(vbox),uinfo->control_button_box,TRUE,TRUE,0);
  gtk_widget_show(uinfo->control_button_box);


  gtk_container_add(GTK_CONTAINER(ebox),vbox);
  gtk_widget_show(vbox);

  return ebox;
}

static void ChangeTimeMode(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  ginfo->gui_info.time_display_mode=(ginfo->gui_info.time_display_mode+1)%4;
  UpdateDisplay(ginfo);
}

void MinMax(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;
  GripGUI *uinfo;

  ginfo=(GripInfo *)data;
  uinfo=&(ginfo->gui_info);

  if(uinfo->minimized) {
    gtk_container_border_width(GTK_CONTAINER(uinfo->winbox),3);
    gtk_box_set_child_packing(GTK_BOX(uinfo->winbox),
                              uinfo->controls,FALSE,FALSE,0,GTK_PACK_START);
    gtk_widget_show(uinfo->notebook);

    CopyPixmap(GTK_PIXMAP(uinfo->lcd_smile_indicator),
	       GTK_PIXMAP(uinfo->smile_indicator));
    CopyPixmap(GTK_PIXMAP(uinfo->empty_image),
    GTK_PIXMAP(uinfo->lcd_smile_indicator));

    gtk_widget_set_size_request(GTK_WIDGET(uinfo->app),
                                WINWIDTH,WINHEIGHT);

    gtk_window_resize(GTK_WINDOW(uinfo->app),
                      uinfo->win_width,
                      uinfo->win_height);
  }
  else {
    gtk_container_border_width(GTK_CONTAINER(uinfo->winbox),0);
    gtk_box_set_child_packing(GTK_BOX(uinfo->winbox),uinfo->controls,
                              TRUE,TRUE,0,GTK_PACK_START);

    gtk_widget_hide(uinfo->notebook);

    CopyPixmap(GTK_PIXMAP(uinfo->smile_indicator),
	       GTK_PIXMAP(uinfo->lcd_smile_indicator));

    if(uinfo->track_edit_visible) ToggleTrackEdit(NULL,(gpointer)ginfo);
    if(uinfo->volvis) ToggleVol(NULL,(gpointer)ginfo);
    if(uinfo->track_prog_visible) ToggleProg(NULL,(gpointer)ginfo);

    gtk_widget_set_size_request(GTK_WIDGET(uinfo->app),
                                MIN_WINWIDTH,MIN_WINHEIGHT);

    gtk_window_resize(GTK_WINDOW(uinfo->app),
                      uinfo->win_width_min,
                      uinfo->win_height_min);

    UpdateGTK();
  }

  uinfo->minimized=!uinfo->minimized;
}

static void ToggleProg(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;
  GripGUI *uinfo;

  ginfo=(GripInfo *)data;
  uinfo=&(ginfo->gui_info);

  if(uinfo->track_prog_visible) {
    gtk_widget_hide(uinfo->playopts);
  }
  else {
    gtk_widget_show(uinfo->playopts);
  }

  uinfo->track_prog_visible=!uinfo->track_prog_visible;

  if(uinfo->minimized) {
    MinMax(NULL,ginfo);
  }
}

static void ToggleControlButtons(GtkWidget *widget,GdkEventButton *event,
				 gpointer data)
{
  GripGUI *uinfo;

  uinfo=&((GripInfo *)data)->gui_info;

  if(uinfo->control_buttons_visible) {
    gtk_widget_hide(uinfo->control_button_box);

    UpdateGTK();
  }
  else {
    gtk_widget_show(uinfo->control_button_box);
  }

  uinfo->control_buttons_visible=!uinfo->control_buttons_visible;
}

static void ToggleVol(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;
  GripGUI *uinfo;

  ginfo=(GripInfo *)data;
  uinfo=&(ginfo->gui_info);

  if(uinfo->volvis) {
    gtk_widget_hide(uinfo->volume_control);
  }
  else {
    gtk_widget_show(uinfo->volume_control);
  }

  uinfo->volvis=!uinfo->volvis;

  if(uinfo->minimized) {
    MinMax(NULL,ginfo);
  }
}

static void SetVolume(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;
  DiscVolume vol;

  ginfo=(GripInfo *)data;

  ginfo->volume=vol.vol_front.left=vol.vol_front.right=
    vol.vol_back.left=vol.vol_back.right=GTK_ADJUSTMENT(widget)->value;

  CDSetVolume(&ginfo->Disc.info, &vol);
}

static void FastFwdCB(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  if(ginfo->ripping_a_disc) {
    gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
                      _("Cannot fast forward while ripping."));

    return;
  }

  ginfo->ffwding=!ginfo->ffwding;

  if(ginfo->ffwding) FastFwd(ginfo);
}

void FastFwd(GripInfo *ginfo)
{
  DiscTime tv;

  tv.mins=0;
  tv.secs=5;

  if((ginfo->Disc.info.disc_mode==CDAUDIO_PLAYING)||
     (ginfo->Disc.info.disc_mode==CDAUDIO_PAUSED)) {
    CDAdvance(&ginfo->Disc, &tv);
  }
}

static void RewindCB(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  g_assert(!ginfo->rewinding);

  if(ginfo->ripping_a_disc) {
    gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
                      _("Cannot rewind while ripping."));

    return;
  }

  ginfo->rewinding=TRUE;

  Rewind(ginfo);
}

static void RewindReleaseCB(GtkWidget *widget, gpointer data) {
  GripInfo *ginfo = data;

  g_assert(ginfo->rewinding);
  ginfo->rewinding = FALSE;
}

void Rewind(GripInfo *ginfo)
{
  DiscTime tv;

  tv.mins=0;
  tv.secs=-5;

  if((ginfo->Disc.info.disc_mode==CDAUDIO_PLAYING)||
     (ginfo->Disc.info.disc_mode==CDAUDIO_PAUSED)) {
    CDAdvance(&ginfo->Disc, &tv);
  }
}

static void NextDisc(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  if(ginfo->ripping_a_disc) {
    gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
                      _("Cannot switch discs while ripping."));

    return;
  }

  if(ginfo->changer_slots>1) {
    ginfo->current_disc=(ginfo->current_disc+1)%ginfo->changer_slots;
    CDChangerSelectDisc(&ginfo->Disc.info, ginfo->current_disc);
    ginfo->have_disc=FALSE;
  }
}

void DoEject(GripInfo *ginfo) {
  RemoveVTracks(ginfo);
  CDEject(&ginfo->Disc.info);
}

void EjectDisc(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  LogStatus(ginfo,_("Eject disc\n"));

  if(ginfo->ripping_a_disc) {
    gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
                      _("Cannot eject while ripping."));

    return;
  }

  if(ginfo->auto_eject_countdown) return;

  /* Busy() can call UpdateTracks() by way of TimeOut() if update_required
   * is TRUE, and vtracks can certainly be set here.  */
  g_assert(ginfo->update_required == FALSE);

  Busy(&(ginfo->gui_info));

  if(ginfo->have_disc) {
    Debug(_("Have disc -- ejecting\n"));

    VTrackEditStoppedPlaying(&ginfo->gui_info);
    CDStop(&ginfo->Disc.info);
    DoEject(ginfo);
    ginfo->playing=FALSE;
    ginfo->have_disc=FALSE;
    ginfo->update_required=TRUE;
    ginfo->current_discid=0;
    ginfo->tray_open=TRUE;
  }
  else {
    if(ginfo->faulty_eject) {
      if(ginfo->tray_open) CDClose(&ginfo->Disc.info);
      else DoEject(ginfo);
    }
    else {
      if(TrayOpen(&ginfo->Disc.info) != 0) CDClose(&ginfo->Disc.info);
      else DoEject(ginfo);
    }

    ginfo->tray_open=!ginfo->tray_open;

    if(!ginfo->tray_open)
      CheckNewDisc(ginfo,FALSE);
  }

  UnBusy(&(ginfo->gui_info));
}

void StopPlayCB(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  if(ginfo->ripping_a_disc) return;

  VTrackEditStoppedPlaying(&ginfo->gui_info);
  CDStop(&ginfo->Disc.info);
  CDStat(&ginfo->Disc, FALSE);
  ginfo->stopped=TRUE;

  if(ginfo->stop_first)
    SelectRow(ginfo,0);
    
  TrayMenuShowPlay(ginfo);
}

void PlaySegment(GripInfo *ginfo,int track)
{
  CDPlayFrames(&ginfo->Disc.info,
	       ginfo->Disc.instance->info.tracks[track].start_frame + 
			   ginfo->start_sector,
	       ginfo->Disc.instance->info.tracks[track].start_frame + 
			   ginfo->end_sector);
}



void PlayTrackCB(GtkWidget *widget,gpointer data)
{
  int track;
  GripInfo *ginfo;
  DiscInfo *disc;

  ginfo=(GripInfo *)data;
  disc = &ginfo->Disc.info;

  if(ginfo->ripping_a_disc) {
    gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
                      _("Cannot play while ripping."));
   
    return;
  }

  CDStat(&ginfo->Disc, FALSE);

  if(ginfo->play_mode!=PM_NORMAL&&!((disc->disc_mode==CDAUDIO_PLAYING)||
				    disc->disc_mode==CDAUDIO_PAUSED)) {
    if(ginfo->play_mode==PM_SHUFFLE && ginfo->automatic_reshuffle)
      ShuffleTracks(ginfo);
    ginfo->current_track_index=0;
    SelectRow(ginfo,CURRENT_TRACK);
  }

  track=CURRENT_TRACK;

  if(track==(disc->curr_track-1)) {
    switch(disc->disc_mode) {
    case CDAUDIO_PLAYING:
      CDPause(disc);
      TrayMenuShowPlay(ginfo);
      return;
      break;
    case CDAUDIO_PAUSED:
      CDResume(disc);
      TrayMenuShowPause(ginfo);
      return;
      break;
    default:
      PlayTrack(ginfo,track);
	  TrayMenuShowPause(ginfo);
      break;
    }
  }
  else {
    PlayTrack(ginfo,track);
    TrayMenuShowPause(ginfo);
  }
}

long last_contiguous_track(Disc *Disc, int track) {
  int i;
  DiscInfoInstance *ins = &Disc->instance->info;
  TrackInfo *tracks = ins->tracks;
  for (i = track + 1; i < ins->num_tracks; i++) {
	TrackInfo *prev_track = &tracks[i - 1];
	TrackInfo *this_track = &tracks[i];
	long prev_ends = prev_track->start_frame + prev_track->num_frames;
	long this_starts = this_track->start_frame;

	if (prev_ends == this_starts || prev_ends + 1 == this_starts)
	  continue;
	else
	  return i - 1;
  }
  return ins->num_tracks - 1;
}

static void PlayTrack(GripInfo *ginfo,int track)
{
  int last_track = track;

  Busy(&(ginfo->gui_info));
  
  if(ginfo->play_mode==PM_NORMAL) {
	last_track = last_contiguous_track(&ginfo->Disc, track);
  }

  CDPlayTrack(&ginfo->Disc, track + 1, last_track + 1);

  UnBusy(&(ginfo->gui_info));

  VTrackEditStartedPlaying(&ginfo->gui_info);

  ginfo->playing=TRUE;
}

void NextTrackCB(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  NextTrack(ginfo);
}

void NextTrack(GripInfo *ginfo)
{
  if(ginfo->ripping_a_disc) {
    gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
                      _("Cannot switch tracks while ripping."));
    return;
  }
  
  CDStat(&ginfo->Disc, FALSE);

  if(ginfo->current_track_index<(ginfo->prog_totaltracks-1)) {
    SelectRow(ginfo,NEXT_TRACK);
  }
  else {
    if(!ginfo->playloop) {
      ginfo->stopped = TRUE;
    }

    SelectRow(ginfo,ginfo->tracks_prog[0]);
  }
}

void PrevTrackCB(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  PrevTrack(ginfo);
}

void PrevTrack(GripInfo *ginfo)
{
  DiscInfoInstance *ins = &ginfo->Disc.instance->info;
  if(ginfo->ripping_a_disc) {
    gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
                      _("Cannot switch tracks while ripping."));
    return;
  }

  CDStat(&ginfo->Disc, FALSE);

  if((ginfo->Disc.info.disc_mode==CDAUDIO_PLAYING) &&
     ((ginfo->Disc.info.curr_frame -
       ins->tracks[ginfo->Disc.info.curr_track - 1].start_frame) > 100))
    PlayTrack(ginfo,CURRENT_TRACK);
  else {
    if(ginfo->current_track_index) {
      SelectRow(ginfo,PREV_TRACK);
    }
    else {
      if(ginfo->playloop) {
	SelectRow(ginfo,ginfo->tracks_prog[ginfo->prog_totaltracks-1]);
      }
    }
  }
}

static void InitProgram(GripInfo *ginfo)
{
  int track;
  char *tok;
  int mode;
  char *plist;
  const char *tmp;

  mode=ginfo->play_mode;

  if((mode==PM_PLAYLIST)) {
    tmp=gtk_entry_get_text(GTK_ENTRY(ginfo->gui_info.playlist_entry));

    if(!tmp || !*tmp) {
      mode=PM_NORMAL;
    }
  }

  if(mode==PM_PLAYLIST) {
    plist=
      strdup(gtk_entry_get_text(GTK_ENTRY(ginfo->gui_info.playlist_entry)));

    ginfo->prog_totaltracks=0;

    tok=strtok(plist,",");

    while(tok) {
      ginfo->tracks_prog[ginfo->prog_totaltracks++]=atoi(tok)-1;

      tok=strtok(NULL,",");
    }

    free(plist);
  }
  else {
    ginfo->prog_totaltracks = ginfo->Disc.instance->info.num_tracks;
    
    for(track=0;track<ginfo->prog_totaltracks;track++) {
      ginfo->tracks_prog[track]=track;
    }
    
    if(mode==PM_SHUFFLE)
      ShuffleTracks(ginfo);
  }
}

 /* Shuffle the tracks around a bit */
static void ShuffleTracks(GripInfo *ginfo)
{
  int t1,t2,tmp,shuffle;

  for(shuffle=0;shuffle<(ginfo->prog_totaltracks*10);shuffle++) {
    t1=RRand(ginfo->prog_totaltracks);
    t2=RRand(ginfo->prog_totaltracks);
    
    tmp=ginfo->tracks_prog[t1];
    ginfo->tracks_prog[t1]=ginfo->tracks_prog[t2];
    ginfo->tracks_prog[t2]=tmp;
  }
}

void CheckNewDisc(GripInfo *ginfo,gboolean force)
{
  int new_id;
  DiscInfo *disc;

  disc = &ginfo->Disc.info;

  /* XXX */
  RemoveVTracks(ginfo);

  if(!ginfo->looking_up) {
    Debug(_("Checking for a new disc\n"));

    if(CDStat(&ginfo->Disc, FALSE)
       && disc->disc_present
       && CDStat(&ginfo->Disc, TRUE)) {
      Debug(_("CDStat found a disc, checking tracks\n"));
      
      if(CheckTracks(&ginfo->Disc)) {
	Debug(_("We have a valid disc!\n"));
	
	new_id = DiscDBDiscid(&ginfo->Disc);

	VTrackEditPTrackset(&ginfo->gui_info);
	InitProgram(ginfo);

        if(ginfo->play_first)
          if(disc->disc_mode == CDAUDIO_COMPLETED ||
	     disc->disc_mode == CDAUDIO_NOSTATUS) {
	    SelectRow(ginfo,0);

	    disc->curr_track = 1;
          }
	
	if(new_id || force) {
	  ginfo->have_disc=TRUE;

	  if(ginfo->play_on_insert) PlayTrackCB(NULL,(gpointer)ginfo);

	  LookupDisc(ginfo,FALSE);
	}
      }
      else {
	if(ginfo->have_disc)
	  ginfo->update_required=TRUE;
	
	ginfo->have_disc=FALSE;
	Debug(_("No non-zero length tracks\n"));
      }
    }
    else {
      if(ginfo->have_disc) {
	ginfo->update_required=TRUE;
      }

      ginfo->have_disc=FALSE;
      Debug(_("CDStat said no disc\n"));
    }
  }
}

/* Check to make sure we didn't get a false alarm from the cdrom device */

static gboolean CheckTracks(Disc *Disc)
{
  DiscInfoInstance *pins = &Disc->p_instance.info;
  int track;
  gboolean have_track=FALSE;

  for(track = 0; track < pins->num_tracks; track++)
    if(pins->tracks[track].length.mins ||
       pins->tracks[track].length.secs) have_track = TRUE;

  return have_track;
}

/* Scan the disc */
void ScanDisc(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  CheckNewDisc(ginfo,TRUE);

  ginfo->update_required=TRUE;
}


/* XXX This will resize based on the active vtrackset, not the visible
 * one */
void UpdateDisplay(GripInfo *ginfo)
{
  /* Note: need another solution other than statics if we ever want to be
     reentrant */
  static int play_counter=0;
  static int discdb_counter=0;
  char buf[80]="";
  char icon_buf[80];
  static int frames;
  static int secs;
  static int mins;
  static int old_width=0;
  int totsecs;
  GripGUI *uinfo;
  DiscInfo *disc;
  TrackInfo *tracks;

  uinfo=&(ginfo->gui_info);
  disc = &ginfo->Disc.info;
  tracks = ginfo->Disc.instance->info.tracks;

  if(!uinfo->minimized) {
    if(uinfo->track_edit_visible) {
      gtk_window_get_size(GTK_WINDOW(uinfo->app),&uinfo->win_width,
                          &uinfo->win_height_edit);
    }
    else
      gtk_window_get_size(GTK_WINDOW(uinfo->app),&uinfo->win_width,
                          &uinfo->win_height);
    
    if(old_width &&
       (old_width != uinfo->instance->track_list->allocation.width)) {
      ResizeTrackList(ginfo);
    }
    
    old_width=uinfo->instance->track_list->allocation.width;
  }
  else {
    gtk_window_get_size(GTK_WINDOW(uinfo->app),&uinfo->win_width_min,
                        &uinfo->win_height_min);
}



  if(!ginfo->looking_up) {
    if(discdb_counter%2)
      discdb_counter++;
  }
  else
    CopyPixmap(GTK_PIXMAP(uinfo->discdb_pix[discdb_counter++%2]),
	       GTK_PIXMAP(uinfo->discdb_indicator));

  if(!ginfo->update_required) {
    if(ginfo->have_disc) {
      /* Allow disc time to spin down after ripping before checking for a new
	 disc. Some drives report no disc when spinning down. */
      if(ginfo->rip_finished) {
	if((time(NULL)-ginfo->rip_finished)>5) {
	  ginfo->rip_finished=0;
	}
      }

      if(!ginfo->rip_finished) {
	CDStat(&ginfo->Disc,FALSE);
	
	if(!disc->disc_present) {
	  RemoveVTracks(ginfo);
	  ginfo->have_disc=FALSE;
	  ginfo->update_required=TRUE;
	}
      }
    }
  }

  if(!ginfo->update_required) {
    if(ginfo->have_disc) {
      if((disc->disc_mode==CDAUDIO_PLAYING)||
	 (disc->disc_mode==CDAUDIO_PAUSED)) {
	if(disc->disc_mode==CDAUDIO_PAUSED) {
	  if((play_counter++%2)==0) {
	    strcpy(buf,"");
	  }
	  else {
	    g_snprintf(buf,80,"%02d:%02d",mins,secs);
	  }
	}
	else {
	  if((disc->curr_track-1)!=CURRENT_TRACK) {
	    SelectRow(ginfo,disc->curr_track-1);
	  }

	  frames = disc->curr_frame - tracks[disc->curr_track-1].start_frame;

	  switch(uinfo->time_display_mode) {
	  case TIME_MODE_TRACK:
	    mins=disc->track_time.mins;
	    secs=disc->track_time.secs;
	    break;
	  case TIME_MODE_DISC:
	    mins=disc->disc_time.mins;
	    secs=disc->disc_time.secs;
	    break;
	  case TIME_MODE_LEFT_TRACK:
	    secs=(disc->track_time.mins*60)+disc->track_time.secs;
	    totsecs=(tracks[CURRENT_TRACK].length.mins*60)+
	      tracks[CURRENT_TRACK].length.secs;
	    
	    totsecs-=secs;
	    
	    mins=totsecs/60;
	    secs=totsecs%60;
	    break;
	  case TIME_MODE_LEFT_DISC:
	    secs=(disc->disc_time.mins*60)+disc->disc_time.secs;
	    totsecs=(disc->length.mins*60)+disc->length.secs;
	    
	    totsecs-=secs;
	    
	    mins=totsecs/60;
	    secs=totsecs%60;
	    break;
	  }
	  
	  g_snprintf(buf,80,_("Current sector: %6d"),frames);
	  gtk_label_set(GTK_LABEL(uinfo->play_sector_label),buf);

          if(uinfo->time_display_mode == TIME_MODE_LEFT_TRACK ||
	     uinfo->time_display_mode == TIME_MODE_LEFT_DISC)
            g_snprintf(buf,80,"-%02d:%02d",mins,secs);
          else
	    g_snprintf(buf,80,"%02d:%02d",mins,secs);
	}
      }
      else {
	if(ginfo->playing&&((disc->disc_mode==CDAUDIO_COMPLETED)||
			    ((disc->disc_mode==CDAUDIO_NOSTATUS)&&
			     !ginfo->stopped))) {
	  NextTrack(ginfo);
	  strcpy(buf,"00:00");
	  if(!ginfo->stopped) PlayTrack(ginfo,CURRENT_TRACK);
	}
	else if(ginfo->stopped) {
	  VTrackEditStoppedPlaying(uinfo);
	  CDStop(disc);

	  frames=secs=mins=0;
	  g_snprintf(buf,80,_("Current sector: %6d"),frames);
	  gtk_label_set(GTK_LABEL(uinfo->play_sector_label),buf);
	  
	  strcpy(buf,"00:00");
	  
	  ginfo->stopped=FALSE;
	  ginfo->playing=FALSE;
	}
	else return;
      }
      
      gtk_label_set(GTK_LABEL(uinfo->play_time_label),buf);
      g_snprintf(icon_buf,sizeof(icon_buf),"%02d %s %s",
		 disc->curr_track,buf,PACKAGE);
      gdk_window_set_icon_name(uinfo->app->window,icon_buf);
    }
  }

/* XXX I'm leaving this here because I'm not yet convinced it's not needed.
  CDStat(disc, FALSE);
  if(disc->curr_frame >=   disc->track[disc->curr_track - 1].start_frame
                         + disc->track[disc->curr_track - 1].num_frames) {
	printf("going to next track from here\n");
    NextTrack(ginfo);
  }
*/

  if(ginfo->update_required) {
    UpdateTracks(ginfo);

    ginfo->update_required=FALSE;

    if(ginfo->have_disc) {
      g_snprintf(buf,80,"%02d:%02d",disc->length.mins,
		 disc->length.secs);
      g_snprintf(icon_buf, sizeof(icon_buf),"%02d %s %s",
		 disc->curr_track,buf,PACKAGE);
	       
      gtk_label_set(GTK_LABEL(uinfo->play_time_label),buf);
      
      if(!ginfo->looking_up) {
	CopyPixmap(GTK_PIXMAP(uinfo->empty_image),
		   GTK_PIXMAP(uinfo->discdb_indicator));

	if(ginfo->auto_rip&&ginfo->is_new_disc) {
	  ClickColumn(ginfo->gui_info.p_instance.rip_column,ginfo);
	  DoRip(NULL,ginfo);
	}

	ginfo->is_new_disc=FALSE;
      }
      
      if(!ginfo->no_interrupt)
	SelectRow(ginfo,0);
      else
 	SelectRow(ginfo,disc->curr_track-1);
    }
    else {
      gtk_label_set(GTK_LABEL(uinfo->play_time_label),"--:--");
      strncpy(icon_buf,PACKAGE,sizeof(icon_buf));
      
      SetCurrentTrack(ginfo,-1);
    }

    gdk_window_set_icon_name(uinfo->app->window,icon_buf);
  }
}

static void UpdateTrackset_with_col_strings(GripInfo *ginfo, DiscInstance *ins, DiscGuiInstance *gins, char *col_strings[3]) {
  Disc *Disc = &ginfo->Disc;
  DiscDataInstance *dins = &ins->data;
  DiscInfoInstance *iins = &ins->info;
  GtkTreeIter iter;
  int track;

  SetTitle(ginfo, ins, gins, ins->data.title);
  SetArtist(ginfo, ins, gins, ins->data.artist);
  
  if(!ginfo->first_time) {
    gtk_list_store_clear(gins->track_list_store);
  }
  if (ins == &Disc->p_instance) { /* XXX huh? */
    SetCurrentTrackIndex(ginfo, Disc->info.curr_track - 1);
  }

  for (track = 0; track < iins->num_tracks; track++) {
      int tracknum = dins->tracks[track].vtracknum;

      if(*dins->tracks[track].track_artist) {
	g_snprintf(col_strings[0], 260, "%02d  %s (%s)", tracknum,
		   dins->tracks[track].track_name,
		   dins->tracks[track].track_artist);
      }
      else
	g_snprintf(col_strings[0], 260, "%02d  %s", tracknum,
		   dins->tracks[track].track_name);

      g_snprintf(col_strings[1], 6, "%2d:%02d",
		 iins->tracks[track].length.mins,
		 iins->tracks[track].length.secs);

      gtk_list_store_append(gins->track_list_store,&iter);

      gtk_list_store_set(gins->track_list_store,&iter,
			 TRACKLIST_TRACK_COL,col_strings[0],
			 TRACKLIST_LENGTH_COL,col_strings[1],
			 TRACKLIST_RIP_COL,FALSE,
			 TRACKLIST_NUM_COL,track,-1);
  }

  if (ins == Disc->instance)
    SelectRow(ginfo,CURRENT_TRACK);
}

void UpdateTrackset(GripInfo *ginfo, DiscInstance *ins, DiscGuiInstance *gins) {
  char string1[260], string2[6];
  char *col_strings[] = {string1, string2, NULL};

  UpdateTrackset_with_col_strings(ginfo, ins, gins, col_strings);
}

void UpdateCurrentTrackset(GripInfo *ginfo) {
  UpdateTrackset(ginfo, ginfo->Disc.instance, ginfo->gui_info.instance);
}

void UpdateTracks(GripInfo *ginfo)
{
  char *col_strings[3];
  gboolean multi_artist_backup;
  gboolean has_vtracks;
  GripGUI *uinfo;
  Disc *Disc;
  DiscDataInstance *pdins;
  DiscInstance *ins;
  DiscGuiInstance *gins;
  EncodeTrack enc_track;

  uinfo=&(ginfo->gui_info);
  Disc  = &ginfo->Disc;
  pdins = &Disc->p_instance.data;

  g_assert(uinfo->v_instance == NULL);

  has_vtracks = Disc->num_vtrack_sets > 0;

  VTracksGuiInit(ginfo);

  if(ginfo->have_disc) {
    /* Reset to make sure we don't eject twice */
    ginfo->auto_eject_countdown=0;

    ginfo->current_discid = DiscDBDiscid(Disc);

    col_strings[0] = g_malloc(260);
    col_strings[1] = g_malloc(6);
    col_strings[2] = NULL;

    FOREACH_TRACK_SET_WITH_GUI(Disc, uinfo, ins, gins) {
      UpdateTrackset_with_col_strings(ginfo, ins, gins, col_strings);
    }

    free(col_strings[0]);
    free(col_strings[1]);

    SetYear(ginfo, pdins->year);
    SetID3Genre(ginfo, pdins->id3genre);

    multi_artist_backup = pdins->multi_artist;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(uinfo->multi_artist_button),
				 pdins->multi_artist);

    pdins->multi_artist = multi_artist_backup;

    UpdateMultiArtist(NULL,(gpointer)ginfo);

    VTrackEditPTrackset(uinfo);

    if(*(ginfo->cdupdate)) {
      FillInTrackInfo(ginfo,0,&enc_track);

      TranslateAndLaunch(ginfo->cdupdate,TranslateSwitch,&enc_track,
			 FALSE,&(ginfo->sprefs),CloseStuff,(void *)ginfo);
    }
  } else {
    g_assert(!has_vtracks);
    SetTitle(ginfo, &Disc->p_instance, &uinfo->p_instance, _("No Disc"));
    SetArtist(ginfo, &Disc->p_instance, &uinfo->p_instance, "");
    SetYear(ginfo,0);
    SetID3Genre(ginfo,17);

    if(!ginfo->first_time)
      gtk_list_store_clear(uinfo->p_instance.track_list_store);
    else
      SetCurrentTrackIndex(ginfo,Disc->info.curr_track - 1);

    VTrackEditDisableAllButtons(uinfo);
  }

  UpdateTrackTabColors(ginfo);

  gtk_entry_set_text(GTK_ENTRY(uinfo->playlist_entry),
		     pdins->playlist);

  if(ginfo->ask_submit) {
    gnome_app_ok_cancel_modal
      ((GnomeApp *)uinfo->app,
       _("This disc has been found on your secondary server,\n"
       "but not on your primary server.\n\n"
       "Do you wish to submit this disc information?"),
       SubmitEntry,(gpointer)ginfo);
    
    ginfo->ask_submit=FALSE;
  }

  ginfo->first_time=0;
}

void SubmitEntry(gint reply,gpointer data)
{
  GripInfo *ginfo;
  int fd;
  FILE *efp;
  char mailcmd[256];
  char filename[256];

  if(reply) return;

  ginfo=(GripInfo *)data;

  sprintf(filename,"/tmp/grip.XXXXXX");
  fd = mkstemp(filename);

  if(fd == -1) {
    gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
                      _("Error: Unable to create temporary file."));
    return;
  }

  efp=fdopen(fd,"w");

  if(!efp) {
    close(fd);
    gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
                      _("Error: Unable to create temporary file."));
  }
  else {
    fprintf(efp,"To: %s\nFrom: %s\nSubject: cddb %s %02x\n",
	    ginfo->discdb_submit_email,
	    ginfo->user_email,
	    DiscDBGenre(ginfo->Disc.instance->data.genre),
	    ginfo->Disc.data.data_id);

    if(ginfo->db_use_freedb) {
      fprintf(efp,
	      "MIME-Version: 1.0\nContent-type: text/plain; charset=UTF-8\n\n");
    }

    if(DiscDBWriteDiscData(&ginfo->Disc, efp, FALSE,
			   ginfo->db_use_freedb,
			   ginfo->db_use_freedb ? 
			   "UTF-8" : ginfo->discdb_encoding) < 0) {
      gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
                        _("Error: Unable to write disc data."));
      fclose(efp);
    }
    else {
      fclose(efp);
      close(fd);

      g_snprintf(mailcmd,256,"%s < %s",MAILER,filename);

      Debug(_("Mailing entry to %s\n"),ginfo->discdb_submit_email);

      system(mailcmd);

      remove(filename);
    }
  }
}

void RetargetMultiArtistCheckButton(GripInfo *ginfo, gboolean *oldvar);

void ChangeTracks(GripInfo *ginfo, DiscInstance *ins, DiscGuiInstance *gins)
{
  gboolean old_stop_first;
  gboolean *old_multi_artist;

  if (ins == ginfo->Disc.instance)
	return;

  if (ginfo->ripping_a_disc) {
      gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
                        _("Cannot change tracksets while ripping."));
      return;
  }

  g_assert(ginfo->Disc.num_vtrack_sets > 0);

  /* If update_required, we can end up in UpdateTracks in Busy() and
   * UnBusy() */
  g_assert(!ginfo->update_required);

  old_stop_first = ginfo->stop_first;
  ginfo->stop_first = TRUE;
  Busy(&ginfo->gui_info);
  CDStop(&ginfo->Disc.info);
  UnBusy(&ginfo->gui_info);
  ginfo->stop_first = old_stop_first;

  old_multi_artist = &ginfo->Disc.instance->data.multi_artist;

  gtk_tooltips_enable(ginfo->gui_info.instance->tabtooltips);

  ginfo->current_track_index = 0;
  ginfo->Disc.info.curr_track = 0;

  if (ins == &ginfo->Disc.p_instance) {
	printf("ChangeTracks: changing to physical\n");
	VTrackEditPTrackset(&ginfo->gui_info);
  } else {
	printf("ChangeTracks: changing to virtual set %d\n", ins - ginfo->Disc.v_instance);
	VTrackEditVTrackset(&ginfo->gui_info);
  }

  ginfo->Disc.instance = ins;
  ginfo->gui_info.instance = gins;

  UpdateTitleEdit(ginfo);
  UpdateArtistEdit(ginfo);
  SetYear(ginfo, ins->data.year);
  SetID3Genre(ginfo, ins->data.id3genre);

  RetargetMultiArtistCheckButton(ginfo, old_multi_artist);

  gtk_tooltips_disable(gins->tabtooltips);

  UpdateTrackTabColors(ginfo);
  InitProgram(ginfo);
}


static void UpdateTrackTabColors(GripInfo *ginfo) {
  Disc *Disc = &ginfo->Disc;
  GripGUI *uinfo = &ginfo->gui_info;
  DiscInstance *ins;
  DiscGuiInstance *gins;
  GtkWidget *w;
  gchar *label_text;

  FOREACH_TRACK_SET_WITH_GUI(Disc, uinfo, ins, gins) {
	w = gtk_notebook_get_tab_label(GTK_NOTEBOOK(uinfo->notebook),
				       gins->trackpage);
	g_assert(w);

	if (ginfo->Disc.num_vtrack_sets  &&  gins == uinfo->instance) {
		/* Set uinfo->instance label to "interesting" */
		label_text = g_strdup_printf("<span foreground=\"red\">%s</span>",
					       gins->tab_label_text);
		gtk_label_set_markup(GTK_LABEL(w), label_text);
		g_free(label_text);
	} else {
		/* Set label to default color */
		gtk_label_set_text(GTK_LABEL(w), gins->tab_label_text);
	}
  }
}
