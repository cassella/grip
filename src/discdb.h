/* discdb.h
 *
 * Based on code from libcdaudio 0.5.0 (Copyright (C)1998 Tony Arcieri)
 *
 * All changes Copyright (c) 1998-2002  Mike Oliphant <oliphant@gtk.org>
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

#ifndef GRIP_DISCDB_H
#define GRIP_DISCDB_H

/* HTTP proxy server structure */

typedef struct _proxy_server {
  char name[256];
  int port;
  char username[80];
  char pswd[80];
} ProxyServer;

/* DiscDB server structure */

typedef struct _discdb_server {
  char name[256];
  char cgi_prog[256];
  int port;
  int use_proxy;
  ProxyServer *proxy;
} DiscDBServer;

typedef struct _discdb_hello {
  /* Program */
  char hello_program[256];
  /* Program version */
  char hello_version[256];
  int proto_version;
} DiscDBHello;

/* DiscDB entry */

typedef struct _discdb_entry {
   unsigned int entry_id;
   int entry_genre;
} DiscDBEntry;

/* An entry in the query list */
struct query_list_entry {
   int list_genre;
   int list_id;
   char list_title[256];
   char list_artist[256];
};

#define MAX_INEXACT_MATCHES			64

/* DiscDB query structure */

typedef struct _discdb_query {
   int query_match;
   int query_matches;
   struct query_list_entry query_list[MAX_INEXACT_MATCHES];
} DiscDBQuery;

/* Match values returned by a query */

#define MATCH_NOMATCH	 0
#define MATCH_EXACT	 1
#define MATCH_INEXACT	 2

/* Track database structure */

typedef struct _track_data {
  char track_name[256];
  char track_artist[256];
  char track_extended[4096];	/* Track-specific extended information */
  int  vtracknum;		/* Track number for display and tagging */
} TrackData;

/* Disc database structure */

typedef struct _disc_data {
  unsigned int data_id;		/* CD id */
  int revision;			/* Database revision */
} DiscData;

typedef struct _disc_data_instance {
  TrackData tracks[MAX_TRACKS];
  char title[256];		/* Disc title */
  char artist[256];		/* Disc artist */
  char extended[4096];		/* Disc extended information */
  int genre;			/* Discdb genre */
  int id3genre;			/* ID3 genre */
  int year;			/* Disc year */
  char playlist[256];		/* Playlist info */
  gboolean multi_artist;	/* Is CD multi-artist? */
} DiscDataInstance;

unsigned int DiscDBDiscid(Disc *Disc);
char *DiscDBGenre(int genre);
int DiscDBGenreValue(char *genre);
gboolean DiscDBDoQuery(Disc *Disc,DiscDBServer *server,
		     DiscDBHello *hello,DiscDBQuery *query);
gboolean DiscDBRead(Disc *Disc, DiscDBServer *server,
		    DiscDBHello *hello,DiscDBEntry *entry,
		    char *encoding);
gboolean DiscDBStatDiscData(Disc *Disc);
int DiscDBReadDiscData(Disc *Disc, const char *encoding);
int DiscDBWriteDiscData(Disc *Disc, FILE *outfile,
                        gboolean gripext,gboolean freedbext,char *encoding);
void DiscDBParseTitle(char *buf,char *title,char *artist,char *sep);
char *ChopWhite(char *buf);
int DiscDBWriteVTrackData(Disc *Disc, char *encoding);
void ReinitTrack(TrackInfo *track);

#endif /* GRIP_DISCDB_H */
