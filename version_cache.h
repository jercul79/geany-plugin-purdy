#ifndef __SVN_CACHE_H__
#define __SVN_CACHE_H__

int svn_cache_has_updates ( );
gchar *svn_cache_cached_svnversion ( const gchar* path, gboolean unsafe );
void *svncache_refresh_function ( void *inp );
void svn_recheck_uri ( const char *uri );
void svn_cache_init ( );
void svn_cache_lock ( );
void svn_cache_unlock ( );
void svn_cache_remove ( gchar *prefix );

int svn_cache_has_updates ( );
void svn_cache_clear_updates ( );

#endif
