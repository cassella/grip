/* discedit.c
 *
 * Copyright (c) 1998-2004  Mike Oliphant <grip@nostatic.org>
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

#include "grip.h"
#include "cdplay.h"
#include "dialog.h"
#include "grip_id3.h"
#include "common.h"
#include "discedit.h"

static void SaveDiscInfo(GtkWidget *widget,gpointer data);
static void TitleEditChanged(GtkWidget *widget,gpointer data);
static void ArtistEditChanged(GtkWidget *widget,gpointer data);
static void YearEditChanged(GtkWidget *widget,gpointer data);
static void EditNextTrack(GtkWidget *widget,gpointer data);
static void ID3GenreChanged(GtkWidget *widget,gpointer data);
static void SeparateFields(char *buf,char *field1,char *field2,char *sep);
static void SplitTitleArtist(GtkWidget *widget,gpointer data);
static void SubmitEntryCB(GtkWidget *widget,gpointer data);
static void GetDiscDBGenre(GripInfo *ginfo);
static void DiscDBGenreChanged(GtkWidget *widget,gpointer data);

typedef void OldGtkSignalFunc(GtkWidget *widget,gpointer data);


GtkButton *VTEditImageButton(GripInfo *ginfo, OldGtkSignalFunc func, GtkWidget *image, char *tooltip, GtkWidget *hbox) {
  GripGUI *uinfo = &ginfo->gui_info;
  GtkWidget *button = ImageButton(GTK_WIDGET(uinfo->app), image);
  gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(func), ginfo);
  gtk_tooltips_set_tip(uinfo->vtracktooltips, button, _(tooltip), NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

  return GTK_BUTTON(button);
}

GtkWidget *MakeEditBox(GripInfo *ginfo)
{
  GripGUI *uinfo;
  GtkWidget *vbox,*hbox;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *frame;
  GtkWidget *item;
  GtkWidget *check;
  GtkWidget *entry;
  GtkWidget *vtbox, *vthbox;
  GtkWidget *sep;
  GtkObject *adj;
  GList *list;
  ID3Genre *id3_genre;
  gint id3_genre_count;
  gint position;
  char *ttt; /* tooltiptext */
  int len;
  int dub_size;
  PangoLayout *layout;
  int i;
  int tlen;

  static const char *fields[] = {"Disc title", "Disc artist", "ID3 Genre", "Disc year",
				 "Track name", "Track artist"};

  uinfo=&(ginfo->gui_info);

  frame=gtk_frame_new(NULL);

  vbox=gtk_vbox_new(FALSE,0);

  hbox=gtk_hbox_new(FALSE,3);

  label=gtk_label_new(_("Disc title"));

  /* Find the length of the longest label in the Track edit section */

  len = 0;
  for (i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
	  layout = gtk_widget_create_pango_layout(GTK_WIDGET(label),
						  _(fields[i]));
	  pango_layout_get_size(layout, &tlen, NULL);
	  tlen /= PANGO_SCALE;
	  g_object_unref(layout);
	  if (tlen > len)
		  len = tlen;
  }

/*   len += 25; */

  layout=gtk_widget_create_pango_layout(GTK_WIDGET(label),
					_("W"));

  pango_layout_get_size(layout,&dub_size,NULL);

  dub_size/=PANGO_SCALE;

  g_object_unref(layout);


  gtk_widget_set_usize(label,len,0);
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
  gtk_widget_show(label);

  uinfo->title_edit_entry=gtk_entry_new_with_max_length(256);
  gtk_signal_connect(GTK_OBJECT(uinfo->title_edit_entry),"changed",
		     GTK_SIGNAL_FUNC(TitleEditChanged),(gpointer)ginfo);
  gtk_entry_set_position(GTK_ENTRY(uinfo->title_edit_entry),0);
  gtk_box_pack_start(GTK_BOX(hbox),uinfo->title_edit_entry,TRUE,TRUE,0);
  gtk_widget_show(uinfo->title_edit_entry);

  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  gtk_widget_show(hbox);

  hbox=gtk_hbox_new(FALSE,3);

  label=gtk_label_new(_("Disc artist"));
  gtk_widget_set_usize(label,len,0);
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
  gtk_widget_show(label);



  uinfo->artist_edit_entry=gtk_entry_new_with_max_length(256);
  gtk_signal_connect(GTK_OBJECT(uinfo->artist_edit_entry),"changed",
		     GTK_SIGNAL_FUNC(ArtistEditChanged),(gpointer)ginfo);
  gtk_entry_set_position(GTK_ENTRY(uinfo->artist_edit_entry),0);
  gtk_box_pack_start(GTK_BOX(hbox),uinfo->artist_edit_entry,TRUE,TRUE,0);
  gtk_widget_show(uinfo->artist_edit_entry);

  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  gtk_widget_show(hbox);

  hbox=gtk_hbox_new(FALSE,3);

  label=gtk_label_new(_("ID3 genre"));
  gtk_widget_set_usize(label,len,0);
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
  gtk_widget_show(label);

  uinfo->id3_genre_combo=gtk_combo_new();

  for(id3_genre_count=0;(id3_genre=ID3GenreByNum(id3_genre_count));
      id3_genre_count++) {
    item = gtk_list_item_new_with_label(id3_genre->name);
    gtk_object_set_user_data(GTK_OBJECT(item),
			     (gpointer)(id3_genre->num));
    uinfo->id3_genre_item_list=g_list_append(uinfo->id3_genre_item_list,item);
    gtk_signal_connect(GTK_OBJECT(item),"select",
		       GTK_SIGNAL_FUNC(ID3GenreChanged),
		       (gpointer)ginfo);
    gtk_container_add(GTK_CONTAINER(GTK_COMBO(uinfo->id3_genre_combo)->list),
		      item);
    gtk_widget_show(item);
  }

  gtk_box_pack_start(GTK_BOX(hbox),uinfo->id3_genre_combo,TRUE,TRUE,0);
  gtk_widget_show(uinfo->id3_genre_combo);

  SetID3Genre(ginfo,ginfo->Disc.instance->data.id3genre);

  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  gtk_widget_show(hbox);

  hbox=gtk_hbox_new(FALSE,3);

  label=gtk_label_new(_("Disc year"));
  gtk_widget_set_usize(label,len,0);
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
  gtk_widget_show(label);

  adj=gtk_adjustment_new(0,0,9999,1.0,5.0,0);

  uinfo->year_spin_button=gtk_spin_button_new(GTK_ADJUSTMENT(adj),0.5,0);
  gtk_signal_connect(GTK_OBJECT(uinfo->year_spin_button),"value_changed",
		     GTK_SIGNAL_FUNC(YearEditChanged),(gpointer)ginfo);
  gtk_box_pack_start(GTK_BOX(hbox),uinfo->year_spin_button,TRUE,TRUE,0);
  gtk_widget_show(uinfo->year_spin_button);

  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  gtk_widget_show(hbox);

  hbox=gtk_hbox_new(FALSE,3);

  label=gtk_label_new(_("Track name"));
  gtk_widget_set_usize(label,len,0);
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
  gtk_widget_show(label);

  uinfo->track_edit_entry=gtk_entry_new_with_max_length(256);
  gtk_signal_connect(GTK_OBJECT(uinfo->track_edit_entry),"changed",
		     GTK_SIGNAL_FUNC(TrackEditChanged),(gpointer)ginfo);
  gtk_signal_connect(GTK_OBJECT(uinfo->track_edit_entry),"activate",
		     GTK_SIGNAL_FUNC(EditNextTrack),(gpointer)ginfo);
  gtk_box_pack_start(GTK_BOX(hbox),uinfo->track_edit_entry,TRUE,TRUE,0);
  gtk_widget_show(uinfo->track_edit_entry);

  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  gtk_widget_show(hbox);

  uinfo->multi_artist_box=gtk_vbox_new(FALSE,0);

  hbox=gtk_hbox_new(FALSE,3);

  label=gtk_label_new(_("Track artist"));
  gtk_widget_set_usize(label,len,0);
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
  gtk_widget_show(label);

  uinfo->track_artist_edit_entry=gtk_entry_new_with_max_length(256);
  gtk_signal_connect(GTK_OBJECT(uinfo->track_artist_edit_entry),"changed",
		     GTK_SIGNAL_FUNC(TrackEditChanged),(gpointer)ginfo);
  gtk_box_pack_start(GTK_BOX(hbox),uinfo->track_artist_edit_entry,
		     TRUE,TRUE,0);
  gtk_widget_show(uinfo->track_artist_edit_entry);

  gtk_box_pack_start(GTK_BOX(uinfo->multi_artist_box),hbox,FALSE,FALSE,0);
  gtk_widget_show(hbox);

  hbox=gtk_hbox_new(FALSE,3);

  label=gtk_label_new(_("Split:"));
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
  gtk_widget_show(label);

  button=gtk_button_new_with_label(_("Title/Artist"));
  gtk_object_set_user_data(GTK_OBJECT(button),(gpointer)0);
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
		     GTK_SIGNAL_FUNC(SplitTitleArtist),(gpointer)ginfo);
  gtk_box_pack_start(GTK_BOX(hbox),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  button=gtk_button_new_with_label(_("Artist/Title"));
  gtk_object_set_user_data(GTK_OBJECT(button),(gpointer)1);
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
		     GTK_SIGNAL_FUNC(SplitTitleArtist),(gpointer)ginfo);
  gtk_box_pack_start(GTK_BOX(hbox),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  entry=MakeStrEntry(&uinfo->split_chars_entry,ginfo->title_split_chars,
		     _("Split chars"),5,TRUE);

  gtk_widget_set_usize(uinfo->split_chars_entry,
		       5*dub_size,0);



  gtk_box_pack_end(GTK_BOX(hbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  gtk_box_pack_start(GTK_BOX(uinfo->multi_artist_box),hbox,FALSE,FALSE,2);
  gtk_widget_show(hbox);

  gtk_box_pack_start(GTK_BOX(vbox),uinfo->multi_artist_box,FALSE,FALSE,0);
  gtk_widget_hide(uinfo->multi_artist_box);

  hbox=gtk_hbox_new(FALSE,0);

  check=MakeCheckButton(&uinfo->multi_artist_button,
			&ginfo->Disc.instance->data.multi_artist,
			_("Multi-artist"));
  gtk_signal_connect(GTK_OBJECT(uinfo->multi_artist_button),"clicked",
		     GTK_SIGNAL_FUNC(UpdateMultiArtist),(gpointer)ginfo);
  gtk_box_pack_start(GTK_BOX(hbox),check,TRUE,TRUE,0);
  gtk_widget_show(check);

  check = MakeCheckButton(&uinfo->vtrack_edit_button,
			&uinfo->vtrack_edit_visible,
			_("Edit vtracks"));
  gtk_signal_connect(GTK_OBJECT(uinfo->vtrack_edit_button), "clicked",
			 GTK_SIGNAL_FUNC(UpdateVTrackEdit), (gpointer)ginfo);
  gtk_box_pack_start(GTK_BOX(hbox), check, TRUE, TRUE, 0);
  gtk_widget_show(check);

  button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->save_image);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
      GTK_SIGNAL_FUNC(SaveDiscInfo),(gpointer)ginfo);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Save disc info"),NULL);
  gtk_box_pack_start(GTK_BOX(hbox),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  button=ImageButton(GTK_WIDGET(uinfo->app),uinfo->mail_image);
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
      GTK_SIGNAL_FUNC(SubmitEntryCB),(gpointer)ginfo);
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Submit disc info"),NULL);
  gtk_box_pack_start(GTK_BOX(hbox),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  gtk_widget_show(hbox);

  /* Begin vtrack edit section */

  uinfo->vtracktooltips = gtk_tooltips_new();
  uinfo->vtrack_buttons_sensitive = 0;

  vtbox = gtk_vbox_new(FALSE, 0);
  uinfo->vtrack_edit_box = vtbox;
  gtk_box_pack_start(GTK_BOX(vbox), vtbox, FALSE, FALSE, 0);

  sep = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vtbox), sep, TRUE, TRUE, 0);
  gtk_widget_show(sep);

  label = gtk_label_new(_("Vtrack editing"));
  gtk_box_pack_start(GTK_BOX(vtbox), label, TRUE, TRUE, 0);
  gtk_widget_show(label);

  /* vtrackset section */

  vthbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vtbox), vthbox, FALSE, FALSE, 0);
  gtk_widget_show(vthbox);

  ttt = "New vtrackset with all of the current set's tracks";
  uinfo->newset_all_button =
      VTEditImageButton(ginfo, NewVTracksetClicked,
			uinfo->newset_all_image, ttt, vthbox);

  ttt = "New vtrackset with the current track and all preceding tracks";
  uinfo->newset_preceding_button =
      VTEditImageButton(ginfo, NewVTracksetPrecedingClicked,
			uinfo->newset_preceding_image, ttt, vthbox);

  ttt = "New vtrackset with the current track and all following tracks";
  uinfo->newset_following_button =
      VTEditImageButton(ginfo, NewVTracksetFollowingClicked,
			uinfo->newset_following_image, ttt, vthbox);


  ttt = "New vtrackset with all tracks preceding the current track";
  uinfo->newset_preceding_strictly_button =
      VTEditImageButton(ginfo, NewVTracksetStrictlyPrecedingClicked,
			uinfo->newset_preceding_strictly_image , ttt, vthbox);


  ttt = "New vtrackset with all tracks following the current track";
  uinfo->newset_following_strictly_button =
      VTEditImageButton(ginfo, NewVTracksetStrictlyFollowingClicked,
			uinfo->newset_following_strictly_image, ttt, vthbox);

  /* Move the "remove vtrackset" button to the right side of the hbox */
  button = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vthbox), button, TRUE, FALSE, 0);
  gtk_widget_show(button);


  ttt = "Remove the current vtrackset";
  uinfo->removeset_button =
      VTEditImageButton(ginfo, RemoveVTracksetClicked,
			uinfo->removeset_image , ttt, vthbox);
  /* end vtrackset section */

  /* vtrack section */

  vthbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vtbox), vthbox, FALSE, FALSE, 0);
  gtk_widget_show(vthbox);

  ttt = "Split the current track into two tracks at the current track position";
  uinfo->split_track_button =
      VTEditImageButton(ginfo, SplitTrackHereClicked,
			uinfo->split_track_image , ttt, vthbox);

  ttt = "Append this track to the previous track";
  uinfo->join_to_prev_button = 
      VTEditImageButton(ginfo, JoinToPrevClicked,
			uinfo->join_to_prev_image, ttt, vthbox);

  ttt = "Prepend this track to the next track";
  uinfo->join_to_next_button =
      VTEditImageButton(ginfo, JoinToNextClicked,
			uinfo->join_to_next_image, ttt, vthbox);

  /* Move the "remove track" button to the right side of the hbox */
  button = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vthbox), button, TRUE, FALSE, 0);
  gtk_widget_show(button);

  ttt = "Remove this track";
  uinfo->remove_track_button =
      VTEditImageButton(ginfo, RemoveTrackClicked,
			uinfo->remove_track_image, ttt, vthbox);


  /* Move End section */

  vthbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vtbox), vthbox, FALSE, FALSE, 0);
  gtk_widget_show(vthbox);

  ttt = "Move the end of this track to here, without adjusting the next track";
  uinfo->move_end_here_noadjust_button =
      VTEditImageButton(ginfo, MoveEndToHereNoAdjustClicked,
			uinfo->move_end_here_noadjust_image, ttt, vthbox);

  ttt = "Move the end of this track to here, adjusting the beginning of the "
	"next track to match if it is adjacent to the end of this one";
  uinfo->move_end_here_adjust_button =
      VTEditImageButton(ginfo, MoveEndToHereAdjustClicked,
			uinfo->move_end_here_adjust_image, ttt, vthbox);

  ttt = "Move the end of this track backward by the amount selected below, "
	"adjusting the beginning of the next track to match if it is adjacent "
	"to the end of this one";
  uinfo->move_end_back_adjust_button =
      VTEditImageButton(ginfo, MoveEndBackwardAdjustClicked,
			uinfo->move_end_back_adjust_image, ttt, vthbox);

  ttt = "Move the end of this track backward by the amount selected below, "
	"without adjusting the next track";
  uinfo->move_end_back_noadjust_button =
      VTEditImageButton(ginfo, MoveEndBackwardNoAdjustClicked,
			uinfo->move_end_back_noadjust_image, ttt, vthbox);

  ttt = "Move the end of this track forward by the amount selected below, "
	"adjusting the beginning of the next track forward if they would "
	"overlap";
  uinfo->move_end_forward_adjust_button =
      VTEditImageButton(ginfo, MoveEndForwardClicked,
			uinfo->move_end_forward_adjust_image, ttt, vthbox);

  /* End move end section */
  /* Move start section */

  vthbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vtbox), vthbox, FALSE, FALSE, 0);
  gtk_widget_show(vthbox);


  ttt = "Move the beginning of this track to the current track position "
	"without adjusting the previous track";
  uinfo->move_beginning_here_noadjust_button =
      VTEditImageButton(ginfo, MoveBeginningToHereNoAdjustClicked,
			uinfo->move_beginning_here_noadjust_image, ttt, vthbox);

  ttt = "Move the beginning of this track to the current track position, "
	"adjusting the end of the previous track to match if it is adjacent to "
	"the beginning of this one";
  uinfo->move_beginning_here_adjust_button =
      VTEditImageButton(ginfo, MoveBeginningToHereAdjustClicked,
			uinfo->move_beginning_here_adjust_image, ttt, vthbox);

  ttt = "Move the beginning of this track forward by the amount selected below "
	"without adjusting the previous track";
  uinfo->move_beginning_forward_noadjust_button =
      VTEditImageButton(ginfo, MoveBeginningForwardNoAdjustClicked,
			uinfo->move_beginning_forward_noadjust_image, ttt, vthbox);

  ttt = "Move the beginning of this track forward by the amount selected below,"
	" adjusting the end of the previous track to match if it is adjacent to"
	" the beginning of this one";
  uinfo->move_beginning_forward_adjust_button =
      VTEditImageButton(ginfo, MoveBeginningForwardAdjustClicked,
			uinfo->move_beginning_forward_adjust_image, ttt,
			vthbox);

  ttt = "Move the beginning of this track backward by the amount selected "
	"below, adjusting the end of the previous track backward if they would "
	"overlap";
  uinfo->move_beginning_back_adjust_button =
      VTEditImageButton(ginfo, MoveBeginningBackwardClicked,
			uinfo->move_beginning_back_adjust_image, ttt, vthbox);

  /* End move start section */

  /* Begin adjustment amount section */

  vthbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vtbox), vthbox, FALSE, FALSE, 0);
  gtk_widget_show(vthbox);

  adj = gtk_adjustment_new(1, 5, 100, 1, 1, 0);

  button = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 0);
  gtk_box_pack_start(GTK_BOX(vthbox), button, FALSE, FALSE, 0);
  uinfo->adjustment_count_spin_button = button;
  gtk_widget_show(button);

  list = g_list_append(NULL, _("frames"));
  list = g_list_append(list, _("seconds"));
  list = g_list_append(list, _("minutes"));

  button = gtk_combo_new();
  gtk_combo_set_popdown_strings(GTK_COMBO(button), list);
  gtk_editable_delete_text(GTK_EDITABLE(GTK_COMBO(button)->entry), 0, -1);
  position = 0;
  gtk_editable_insert_text(GTK_EDITABLE(GTK_COMBO(button)->entry), _("seconds"), strlen(_("seconds")), &position);
  gtk_box_pack_start(GTK_BOX(vthbox), button, FALSE, FALSE, 0);
  gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(button)->entry), FALSE);
  uinfo->adjustment_unit_combo = button;
  gtk_widget_show(button);

  /* End adjustment amount section */

  /* Begin vtracknum section */
  vthbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vtbox), vthbox, FALSE, FALSE, 0);
  gtk_widget_show(vthbox);

  label = gtk_label_new(_("Set VTracknum to "));
  gtk_box_pack_start(GTK_BOX(vthbox), label, FALSE, TRUE, 0);
  gtk_widget_show(label);

  adj = gtk_adjustment_new(1, 1, 1000, 1, 1, 0);

  button = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 0);
  gtk_box_pack_start(GTK_BOX(vthbox), button, FALSE, FALSE, 0);
  uinfo->set_vtracknum_spin_button = button;
  gtk_widget_show(button);

  gtk_signal_connect(GTK_OBJECT(adj), "value-changed",
		     GTK_SIGNAL_FUNC(VTracknumChanged), ginfo);

  button = gtk_check_button_new_with_label(_("Adjust following tracks"));
  gtk_box_pack_start(GTK_BOX(vthbox), button, FALSE, FALSE, 0);
  uinfo->set_vtracknum_adjust_check_button = button;
  gtk_widget_show(button);

  /* end vtrack section */

  /* End VTrack edit section */

  gtk_container_add(GTK_CONTAINER(frame),vbox);
  gtk_widget_show(vbox);

  return frame;
}

void UpdateMultiArtist(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;
  GripGUI *uinfo;
  gboolean multi_artist;
  

  ginfo=(GripInfo *)data;
  uinfo=&(ginfo->gui_info);

  multi_artist = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(uinfo->multi_artist_button));

  if (multi_artist)
	  gtk_widget_show(uinfo->multi_artist_box);
  else
	  gtk_widget_hide(uinfo->multi_artist_box);
}

void RetargetMultiArtistCheckButton(GripInfo *ginfo, gboolean *oldvar) {
  gboolean *var = &ginfo->Disc.instance->data.multi_artist;
  GtkWidget *widget = ginfo->gui_info.multi_artist_button;

  RetargetCheckButton(widget, var, oldvar);
  UpdateMultiArtist(NULL, ginfo);
}

void UpdateVTrackEdit(GtkWidget *widget, gpointer data) {
  GripInfo *ginfo = (GripInfo *)data;
  GripGUI *uinfo = &ginfo->gui_info;

  gboolean vtrack_edit;

  vtrack_edit = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(uinfo->vtrack_edit_button));

  if (vtrack_edit)
	gtk_widget_show(uinfo->vtrack_edit_box);
  else
	gtk_widget_hide(uinfo->vtrack_edit_box);
}

void ToggleTrackEdit(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;
  GripGUI *uinfo;

  ginfo=(GripInfo *)data;
  uinfo=&(ginfo->gui_info);

  if(uinfo->track_edit_visible) {
    gtk_window_resize(GTK_WINDOW(uinfo->app),
                      uinfo->win_width,
                      uinfo->win_height);

    gtk_widget_hide(uinfo->track_edit_box);
    UpdateGTK();
  }
  else {
    if(uinfo->minimized) MinMax(NULL,(gpointer)ginfo);

    gtk_widget_show(uinfo->track_edit_box);

    gtk_window_resize(GTK_WINDOW(uinfo->app),
                      uinfo->win_width,
                      uinfo->win_height_edit);
  }

  uinfo->track_edit_visible=!uinfo->track_edit_visible;
}

void UpdateTitleEdit(GripInfo *ginfo) {
  DiscInstance *ins = ginfo->Disc.instance;
  gchar *title = ins->data.title;

  g_signal_handlers_block_by_func(G_OBJECT(ginfo->gui_info.title_edit_entry),
                                  TitleEditChanged,(gpointer)ginfo);

  gtk_entry_set_text(GTK_ENTRY(ginfo->gui_info.title_edit_entry),title);
  gtk_entry_set_position(GTK_ENTRY(ginfo->gui_info.title_edit_entry), 0);

  g_signal_handlers_unblock_by_func(G_OBJECT(ginfo->gui_info.title_edit_entry),
                                    TitleEditChanged,(gpointer)ginfo);
}

void SetTitle(GripInfo *ginfo, DiscInstance *ins, DiscGuiInstance *gins, char *title)
{
  strcpy(ins->data.title, title);

  gtk_label_set(GTK_LABEL(gins->disc_name_label), title);
  gtk_tooltips_set_tip(gins->tabtooltips, gins->trackpage, ins->data.title, "");
  if (ins == ginfo->Disc.instance) {
	  UpdateTitleEdit(ginfo);
  }
}

void UpdateArtistEdit(GripInfo *ginfo) {
  DiscInstance *ins = ginfo->Disc.instance;
  gchar *artist = ins->data.artist;

  g_signal_handlers_block_by_func(G_OBJECT(ginfo->gui_info.artist_edit_entry),
                                   ArtistEditChanged,(gpointer)ginfo);

  gtk_entry_set_text(GTK_ENTRY(ginfo->gui_info.artist_edit_entry),artist);
  gtk_entry_set_position(GTK_ENTRY(ginfo->gui_info.artist_edit_entry), 0);

  g_signal_handlers_unblock_by_func(G_OBJECT(ginfo->gui_info.artist_edit_entry),
                                    ArtistEditChanged,(gpointer)ginfo);
}

void SetArtist(GripInfo *ginfo, DiscInstance *ins, DiscGuiInstance *gins, char *artist)
{
  strcpy(ins->data.artist, artist);

  gtk_label_set(GTK_LABEL(gins->disc_artist_label),artist);

  if (ins == ginfo->Disc.instance) {
	  UpdateArtistEdit(ginfo);
  }
}

void SetYear(GripInfo *ginfo,int year)
{
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(ginfo->gui_info.year_spin_button),
  			    (gfloat)year);
}

void SetID3Genre(GripInfo *ginfo,int id3_genre)
{
  GtkWidget *item;

  item=
    GTK_WIDGET(g_list_nth(ginfo->gui_info.id3_genre_item_list,
			  ID3GenrePos(id3_genre))->data);
  gtk_list_select_child(GTK_LIST(GTK_COMBO(ginfo->gui_info.id3_genre_combo)->
				 list),item);
}

static void SaveDiscInfo(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  if(ginfo->have_disc) {
	  if(DiscDBWriteDiscData(&ginfo->Disc, NULL, TRUE, FALSE,
				 "utf-8") < 0)
		gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
				    _("Error saving disc data"));
	if (DiscDBWriteVTrackData(&ginfo->Disc,"utf-8") != 0)
		gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
				  _("Error saving vtrack data"));
  }
  else 		gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
				  _("No disc present"));
}

static void TitleEditChanged(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;
  const gchar *title;

  ginfo=(GripInfo *)data;

  title = gtk_entry_get_text(GTK_ENTRY(ginfo->gui_info.title_edit_entry));

  strcpy(ginfo->Disc.instance->data.title, title);
  
  gtk_label_set(GTK_LABEL(ginfo->gui_info.instance->disc_name_label), title);
}

static void ArtistEditChanged(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;
  const gchar *artist;

  ginfo=(GripInfo *)data;

  artist = gtk_entry_get_text(GTK_ENTRY(ginfo->gui_info.artist_edit_entry));
  strcpy(ginfo->Disc.instance->data.artist,artist);
  gtk_label_set(GTK_LABEL(ginfo->gui_info.instance->disc_artist_label),artist);
}

static void YearEditChanged(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  ginfo->Disc.instance->data.year =
    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ginfo->gui_info.
  						     year_spin_button));
}

void TrackEditChanged(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo = (GripInfo *)data;
  DiscGuiInstance *gins = ginfo->gui_info.instance;
  DiscDataInstance *ins = &ginfo->Disc.instance->data;
  char newname[256];
  GtkTreeIter iter;
  gint i;



  strcpy(ins->tracks[CURRENT_TRACK].track_name,
	 gtk_entry_get_text(GTK_ENTRY(ginfo->gui_info.track_edit_entry)));

  strcpy(ins->tracks[CURRENT_TRACK].track_artist,
	 gtk_entry_get_text(GTK_ENTRY(ginfo->gui_info.track_artist_edit_entry)));

  
  if(*ins->tracks[CURRENT_TRACK].track_artist)
    g_snprintf(newname,256,"%02d  %s (%s)",CURRENT_TRACK+1,
	       ins->tracks[CURRENT_TRACK].track_name,
	       ins->tracks[CURRENT_TRACK].track_artist);

  else
    g_snprintf(newname,256,"%02d  %s",CURRENT_TRACK+1,
	       ins->tracks[CURRENT_TRACK].track_name);


  gtk_tree_model_get_iter_first(GTK_TREE_MODEL(gins->track_list_store),
                                &iter);
  for(i=0;i<CURRENT_TRACK;i++)
    gtk_tree_model_iter_next(GTK_TREE_MODEL(gins->track_list_store),
                             &iter);
  gtk_list_store_set(gins->track_list_store,&iter,
			 TRACKLIST_TRACK_COL,newname,-1);

  /*  gtk_clist_set_text(GTK_CLIST(ginfo->gui_info.instance->trackclist),
      CURRENT_TRACK,0,newname);*/
}

static void EditNextTrack(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  NextTrack(ginfo);
  /*  gtk_editable_select_region(GTK_EDITABLE(track_edit_entry),0,-1);*/
  gtk_widget_grab_focus(GTK_WIDGET(ginfo->gui_info.track_edit_entry));
}

static void ID3GenreChanged(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  ginfo->Disc.instance->data.id3genre=(int)gtk_object_get_user_data(GTK_OBJECT(widget));
  ginfo->Disc.instance->data.genre=ID32DiscDB(ginfo->Disc.instance->data.id3genre);
}

static void SeparateFields(char *buf,char *field1,char *field2,char *sep)
{
  char *tmp;
  char spare[80];

  tmp=strtok(buf,sep);

  if(!tmp) return;

  strncpy(spare,g_strstrip(tmp),80);

  tmp=strtok(NULL,"");

  if(tmp) {
    strncpy(field2,g_strstrip(tmp),80);
  }
  else *field2='\0';

  strcpy(field1,spare);
}

static void SplitTitleArtist(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo = (GripInfo *)data;
  DiscDataInstance *ins = &ginfo->Disc.p_instance.data;
  int track;
  int mode;

  mode=(int)gtk_object_get_user_data(GTK_OBJECT(widget));

  for(track = 0; track < ginfo->Disc.p_instance.info.num_tracks; track++) {
    if(mode==0)
      SeparateFields(ins->tracks[track].track_name,
		     ins->tracks[track].track_name,
		     ins->tracks[track].track_artist,
		     ginfo->title_split_chars);
    else 
      SeparateFields(ins->tracks[track].track_name,
		     ins->tracks[track].track_artist,
		     ins->tracks[track].track_name,
		     ginfo->title_split_chars);
  }

  UpdateTracks(ginfo);
}

static void SubmitEntryCB(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;
  int len;

  ginfo=(GripInfo *)data;

  if(!ginfo->have_disc) {
    gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
		      _("Cannot submit\nNo disc is present"));

    return;
  }

  if(!ginfo->Disc.instance->data.genre) {
    /*    gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
	  _("Submission requires a genre other than 'unknown'"));*/
    GetDiscDBGenre(ginfo);

    return;
  }

  if(!ginfo->Disc.instance->data.title[0]) {
    gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
		      _("You must enter a disc title"));

    return;
  }

  if(!ginfo->Disc.instance->data.artist[0]) {
    gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
		      _("You must enter a disc artist"));
    
    return;
  }

  len=strlen(ginfo->discdb_submit_email);

  if(!strncasecmp(ginfo->discdb_submit_email+(len-9),".cddb.com",9))
    gnome_app_ok_cancel_modal
      ((GnomeApp *)ginfo->gui_info.app,
       _("You are about to submit this disc information\n"
       "to a commercial CDDB server, which will then\n"
       "own the data that you submit. These servers make\n"
       "a profit out of your effort. We suggest that you\n"
       "support free servers instead.\n\nContinue?"),
       (GnomeReplyCallback)SubmitEntry,(gpointer)ginfo);
  else
    gnome_app_ok_cancel_modal
      ((GnomeApp *)ginfo->gui_info.app,
       _("You are about to submit this\ndisc information via email.\n\n"
       "Continue?"),(GnomeReplyCallback)SubmitEntry,(gpointer)ginfo);
}

/* Make the user pick a DiscDB genre on submit*/
static void GetDiscDBGenre(GripInfo *ginfo)
{
  GtkWidget *dialog;
  GtkWidget *label;
  GtkWidget *submit_button;
  GtkWidget *cancel_button;
  GtkWidget *hbox;
  GtkWidget *genre_combo;
  GtkWidget *item;
  int genre;

  dialog=gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog),_("Genre selection"));

  gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),5);

  label=gtk_label_new(_("Submission requires a genre other than 'unknown'\n"
		      "Please select a DiscDB genre below"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),label,TRUE,TRUE,0);
  gtk_widget_show(label);

  genre_combo=gtk_combo_new();
  gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(genre_combo)->entry),FALSE);

  hbox=gtk_hbox_new(FALSE,3);

  label=gtk_label_new(_("DiscDB genre"));
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
  gtk_widget_show(label);

  for(genre=0;genre<12;genre++) {
    item=gtk_list_item_new_with_label(DiscDBGenre(genre));
    gtk_object_set_user_data(GTK_OBJECT(item),
			     (gpointer)genre);
    gtk_signal_connect(GTK_OBJECT(item), "select",
		       GTK_SIGNAL_FUNC(DiscDBGenreChanged),(gpointer)ginfo);
    gtk_container_add(GTK_CONTAINER(GTK_COMBO(genre_combo)->list),item);
    gtk_widget_show(item);
  }

  gtk_box_pack_start(GTK_BOX(hbox),genre_combo,TRUE,TRUE,0);
  gtk_widget_show(genre_combo);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),hbox,TRUE,TRUE,0);
  gtk_widget_show(hbox);

  submit_button=gtk_button_new_with_label(_("Submit"));

  gtk_signal_connect(GTK_OBJECT(submit_button),"clicked",
		     (gpointer)SubmitEntryCB,(gpointer)ginfo);
  gtk_signal_connect_object(GTK_OBJECT(submit_button),"clicked",
			    GTK_SIGNAL_FUNC(gtk_widget_destroy),
			    GTK_OBJECT(dialog));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),submit_button,
		     TRUE,TRUE,0);
  gtk_widget_show(submit_button);

  cancel_button=gtk_button_new_with_label(_("Cancel"));

  gtk_signal_connect_object(GTK_OBJECT(cancel_button),"clicked",
			    GTK_SIGNAL_FUNC(gtk_widget_destroy),
			    GTK_OBJECT(dialog));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),cancel_button,
		     TRUE,TRUE,0);
  gtk_widget_show(cancel_button);

  gtk_widget_show(dialog);

  gtk_grab_add(dialog);
}

/* Set the DiscDB genre when a combo item is selected */
static void DiscDBGenreChanged(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  ginfo->Disc.instance->data.genre=(int)gtk_object_get_user_data(GTK_OBJECT(widget));
}
