#include <stdlib.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include <geany.h>
#include <geanyplugin.h>
#include "plugindata.h"
#include "locale.h"
#include "xpms.h"

#ifdef HAVE_GIO
# include <gio/gio.h>
#endif

#ifdef G_OS_WIN32
# include <windows.h>
#endif

extern gint purdy_terminate_requested;
extern const gchar project_dir[];

static GHashTable *svn_cache = NULL;
static GHashTable *svn_cache_pending = NULL;
static pthread_mutex_t svn_cache_mutex = PTHREAD_MUTEX_INITIALIZER;
static gint svn_cache_update_flag = 0;

void svn_cache_lock ( )
{
	pthread_mutex_lock ( &svn_cache_mutex );
}

void svn_cache_unlock ( )
{
	pthread_mutex_unlock ( &svn_cache_mutex );
}

int svn_cache_has_updates  ( )
{
	return svn_cache_update_flag;
}

void svn_cache_clear_updates ( )
{
	svn_cache_update_flag = 0;
}

// return must be g_free'd
gchar *svn_cache_cached_svnversion ( const gchar* path, gboolean unsafe )
{
	gchar *result = NULL;

	if ( !unsafe )
		pthread_mutex_lock ( &svn_cache_mutex );

	// Check the cache
	if ( g_hash_table_contains ( svn_cache, path ) )
	{
		result = g_strdup ( g_hash_table_lookup ( svn_cache, path ) );
	}

	// If not cached, insert into pending list
	if ( result == NULL )
	{
		// cache the result
		//g_print ( "Asking for %s", path );
		g_hash_table_replace ( svn_cache_pending, g_strdup(path), g_strdup("") );
		result = g_strdup ( "..." );
	}

	if ( !unsafe )
		pthread_mutex_unlock ( &svn_cache_mutex );

	return result;
}

static gchar *svnversion ( const gchar* path )
{
	gchar *safe_path = g_shell_quote ( path );

	gchar *cmd = g_strjoin ( " ", "svnversion", "-nc", path, NULL );

	gchar *output;
	g_spawn_command_line_sync ( cmd, &output, NULL, NULL, NULL );

	g_free ( safe_path );
	g_free ( cmd );

	gchar *ver = g_strdup ( output ? output : "" );
	if ( output )
		g_free ( output );

	return ver;
}

static void strip_trailing_slashes ( gchar *s )
{
	int len = strlen ( s );
	for ( ; len>0 ; len-- )
	{
		if ( s[len-1] == '/' )
			s[len-1] = 0;
		else
			break;
	}
}


// ===========================
// 
// FINAL
// ===========================
void *svncache_refresh_function ( void *inp )
{
	while ( !purdy_terminate_requested ) {
		gchar *uri = NULL, *key = NULL, *val = NULL;

		pthread_mutex_lock ( &svn_cache_mutex );

		GHashTableIter iter;
		g_hash_table_iter_init (&iter, svn_cache_pending);
		while ( g_hash_table_iter_next ( &iter, (gpointer)&key, (gpointer)&val ) ) {
			if ( key ) {
				uri = g_strdup ( key );
				break;
			}
		}
		pthread_mutex_unlock ( &svn_cache_mutex );

		if ( uri ) {
			// this is the costly operation. Keep out of protected block.
			gchar *ver = svnversion ( uri );

			pthread_mutex_lock ( &svn_cache_mutex );
			g_hash_table_replace ( svn_cache, uri, ver );
			g_hash_table_remove ( svn_cache_pending, uri );
			svn_cache_update_flag = 1;
			pthread_mutex_unlock ( &svn_cache_mutex );
		} else {
			// TODO: Use thread conds instead of sleep/poll
			struct timespec req, rem;
			req.tv_sec = 0;
			req.tv_nsec = 100000000;
			nanosleep ( &req, &rem );
		}
	}

	g_print ( "svn_cache terminate requested\n" );

	return NULL;
}

void svn_recheck_uri ( const char *uri )
{
	gchar *cur = g_strdup ( uri );

	while ( TRUE )
	{
		g_print ( "Rechecking: (%s)", cur );

		if ( strncmp ( cur, project_dir, strlen(project_dir) ) != 0 )
		{
			g_print ( "Not in the projects. Skipping svn_recheck\n" );
			break;
		}

		// remove from svn_cache and insert to svn_cache_pending. The rest happens automatically
		pthread_mutex_lock ( &svn_cache_mutex );
		g_hash_table_remove ( svn_cache, cur );
		g_hash_table_replace ( svn_cache_pending, g_strdup(cur), g_strdup("") );
		pthread_mutex_unlock ( &svn_cache_mutex );

		gchar *ncur = g_path_get_dirname(cur);
		if ( !ncur )
			break;

		strip_trailing_slashes ( ncur );

		g_free ( cur );
		cur = ncur;
	}

	g_free ( cur );
}


void *inotify_watcher_function ( void *inp )
{
	int fd = inotify_init ();
	fd_set watch_set;
	if ( fd < 0 )
	{
		g_print ( "inotify() disabled" );
		return NULL;
	}

#define EVENT_SIZE          ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN       ( 1024 * ( EVENT_SIZE + NAME_MAX + 1) )

	int WATCH_FLAGS = ( IN_CREATE | IN_DELETE | IN_MODIFY );
	FD_ZERO( &watch_set );
	FD_SET ( fd, &watch_set );
//	int wd = 
	inotify_add_watch( fd, project_dir, WATCH_FLAGS );
	char buffer[ EVENT_BUF_LEN ];

	while ( 1 )
	{
		select( fd+1, &watch_set, NULL, NULL, NULL );
		// Read event(s) from non-blocking inotify fd (non-blocking specified in inotify_init1 above).
		int length = read( fd, buffer, EVENT_BUF_LEN ); 
		if ( length < 0 ) {
			g_print ( "inotify read error" );
			break;
		}

		for ( int i=0; i<length; ) {
			struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
			if ( event->wd == -1 )
			{
				g_print ( "inotify overflow" );
				continue;
			}
	
			g_print ( "event WD: %d", event->wd );
			if ( event->len ) {
				if ( event->mask & IN_IGNORED ) {
					g_print( "IN_IGNORED\n" );
				}
				if ( event->mask & IN_CREATE ) 
				{
					if ( event->mask & IN_ISDIR ) {
						g_print( "New directory %s created.", event->name );
					} else {
						g_print( "New file %s created.", event->name );
					}
				}
				if ( event->mask & IN_DELETE ) 
				{
					if ( event->mask & IN_ISDIR ) 
					{
						g_print( "Directory %s deleted.", event->name );
					}
					else
					{
						g_print( "File %s deleted.", event->name );
					}
				}
				if ( event->mask & IN_MODIFY ) 
				{
					if ( event->mask & IN_ISDIR ) 
					{
						g_print( "Directory %s modified.", event->name );
					}
					else
					{
						g_print( "File %s modified.", event->name );
					}
				}
			}
			i += EVENT_SIZE + event->len;
		}
	}
	return NULL;
}

void svn_cache_remove ( gchar *prefix )
{
	pthread_mutex_lock ( &svn_cache_mutex );

	GList *rem_list = NULL;
	GHashTableIter iter;
	g_hash_table_iter_init (&iter, svn_cache);
	gchar *key, *val;
	while ( g_hash_table_iter_next ( &iter, (gpointer)&key, (gpointer)&val ) )
	{
		if ( key && strstr ( key, prefix ) == key )
		{
			rem_list = g_list_append ( rem_list, g_strdup(key) );
		}
	}

	for ( GList *l = rem_list; l != NULL; l = l->next)
	{
		g_hash_table_remove ( svn_cache, l->data );
	}
	pthread_mutex_unlock ( &svn_cache_mutex );

	// free up glist
	for ( GList *l = rem_list; l != NULL; l = l->next)
	{
		// Becuase we strdup'ed above
		g_free ( l->data );
	}
	g_list_free ( rem_list );	
}

void svn_cache_init ( )
{
	svn_cache = g_hash_table_new ( g_str_hash, g_str_equal );
	svn_cache_pending = g_hash_table_new ( g_str_hash, g_str_equal );

	pthread_t svn_watcher;
	pthread_create ( &svn_watcher, NULL, svncache_refresh_function, NULL );

	pthread_t inotify_watcher;
	pthread_create ( &inotify_watcher, NULL, inotify_watcher_function, NULL );

}

