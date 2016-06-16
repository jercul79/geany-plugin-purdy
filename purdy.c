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

#ifdef HAVE_GIO
# include <gio/gio.h>
#endif

#ifdef G_OS_WIN32
# include <windows.h>
#endif

struct t_svnitem
{
	gchar *uri;
	gchar *ver;
};

struct t_treenode
{
	gchar *path;
	gchar *uri;
};

/* These items are set by Geany before plugin_init() is called. */
GeanyPlugin 				*geany_plugin;
GeanyData 					*geany_data;

static gint there_are_updates = 0;
static gint 				page_number 				= 0;
static GtkTreeStore 		*treestore = NULL;
static GtkWidget 			*treeview;
static GtkWidget 			*sidebar_vbox;
static GtkWidget 			*sidebar_vbox_bars;
static gchar 				*addressbar_last_address 	= NULL;
static GHashTable 				*svn_cache = NULL;
static GHashTable 				*svn_cache_pending = NULL;
static int delete_no = 0;

static GtkTreeViewColumn 	*treeview_column_text;
static GtkCellRenderer 		*render_icon, *render_text;
pthread_mutex_t svn_cache_mutex = PTHREAD_MUTEX_INITIALIZER;
const gchar backupcopy_backup_dir[] = "/home/harmanci/backups";

/* ------------------
 * FLAGS
 * ------------------ */

static gboolean 			flag_on_expand_refresh 		= FALSE;

/* ------------------
 *  CONFIG VARS
 * ------------------ */

#ifndef G_OS_WIN32
# define CONFIG_OPEN_EXTERNAL_CMD_DEFAULT "nautilus '%d'"
# define CONFIG_OPEN_TERMINAL_DEFAULT "xterm"
#else
# define CONFIG_OPEN_EXTERNAL_CMD_DEFAULT "explorer '%d'"
# define CONFIG_OPEN_TERMINAL_DEFAULT "cmd"
#endif

static gchar 				*CONFIG_FILE 				= NULL;
static gchar 				*CONFIG_OPEN_EXTERNAL_CMD 	= NULL;
static gchar 				*CONFIG_OPEN_TERMINAL 	= NULL;
static gboolean 			CONFIG_ONE_CLICK_CHDOC 		= FALSE;
static gboolean 			CONFIG_SHOW_HIDDEN_FILES 	= FALSE;
static gboolean 			CONFIG_HIDE_OBJECT_FILES 	= FALSE;
static gint 				CONFIG_SHOW_BARS			= 1;
static gboolean 			CONFIG_CHROOT_ON_DCLICK		= FALSE;
static gboolean 			CONFIG_FOLLOW_CURRENT_DOC 	= TRUE;
static gboolean 			CONFIG_ON_DELETE_CLOSE_FILE = TRUE;
static gboolean 			CONFIG_ON_OPEN_FOCUS_EDITOR = FALSE;
static gboolean 			CONFIG_SHOW_TREE_LINES 		= TRUE;
static gint 				CONFIG_SHOW_ICONS 			= 2;
static gboolean				CONFIG_OPEN_NEW_FILES 		= TRUE;

static const char * added_xpm[] = {
"7 7 32 1",
" 	c None",
".	c #388C51",
"+	c #378D4F",
"@	c #388E52",
"#	c #379151",
"$	c #D6D93D",
"%	c #369054",
"&	c #2F7C45",
"*	c #2F7945",
"=	c #2F7C47",
"-	c #C6D63A",
";	c #2D7A47",
">	c #2E7A45",
",	c #2E7947",
"'	c #286A3D",
")	c #89AC33",
"!	c #89AC34",
"~	c #8CAB34",
"{	c #8AAD35",
"]	c #276A40",
"^	c #24623E",
"/	c #25623A",
"(	c #23633B",
"_	c #629230",
":	c #26613B",
"<	c #256238",
"[	c #1D5032",
"}	c #62942D",
"|	c #1D4E31",
"1	c #1B4B2F",
"2	c #1B4B2E",
"3	c #1B4A2E",
"  .+@  ",
"  #$%  ",
"&*=-;>,",
"')!)~{]",
"^/(_:</",
"  [}|  ",
"  123  "};

/*
static const char * conflict_xpm[] = {
"11 9 26 1",
" 	c None",
".	c #AA3227",
"+	c #A92C25",
"@	c #E85B5A",
"#	c #A72D25",
"$	c #A72521",
"%	c #E34749",
"&	c #E53B3C",
"*	c #A61F1E",
"=	c #DD2B32",
"-	c #A51919",
";	c #D22121",
">	c #DA1822",
",	c #DD1F2A",
"'	c #DA1823",
")	c #DE2733",
"!	c #A31416",
"~	c #E32A30",
"{	c #E4262C",
"]	c #A30F13",
"^	c #E53135",
"/	c #A20F13",
"(	c #A10B10",
"_	c #A20B10",
":	c #A10B11",
"<	c #A1080F",
"    . .    ",
"   +@#@#   ",
"  $%%&%%$  ",
" *=======* ",
"-;>,'>>)>;-",
" !;~~{~~;! ",
"  ];^^^;/  ",
"   (;_;:   ",
"    < <    "};

static const char * removed_xpm[] = {
"7 7 22 1",
" 	c None",
".	c #717171",
"+	c #626262",
"@	c #727272",
"#	c #5C5C5C",
"$	c #5D5D5D",
"%	c #878787",
"&	c #737373",
"*	c #868686",
"=	c #585858",
"-	c #7D7D7D",
";	c #545454",
">	c #636363",
",	c #525252",
"'	c #515151",
")	c #6D6D6D",
"!	c #4F4F4F",
"~	c #505050",
"{	c #6E6E6E",
"]	c #4E4E4E",
"^	c #4D4D4D",
"/	c #707070",
".+@ .#.",
"$%& @*$",
".=%$-=.",
" ;>.>, ",
".')!)~.",
"!{. .)]",
".^@ /!/"};
*/

/* XPM */
static const char * staged_xpm[] = {
"7 7 15 1",
" 	c None",
".	c #50381C",
"+	c #483219",
"@	c #53372F",
"#	c #FAFACE",
"$	c #483319",
"%	c #3E2B15",
"&	c #3D2B15",
"*	c #322311",
"=	c #332310",
"-	c #281C0C",
";	c #291C0C",
">	c #201609",
",	c #211609",
"'	c #1C1306",
".......",
"+@@#@@$",
"%#@#@#&",
"*@###@=",
"-#@#@#;",
">@@#@@,",
"'''''''"};
/* XPM */
static const char *tracked_xpm[] = {
"6 7 9 1",
" 	c None",
".	c #FFFEF9",
"+	c #FFE263",
"@	c #D48B14",
"#	c #FFC63E",
"$	c #FAECD5",
"%	c #FFC623",
"&	c #FFA814",
"*	c #B96C14",
".+@@@#",
".@$%&*",
".@$%&*",
".@$%&*",
".@$%&*",
".@$%&*",
".#***#"};
/* XPM */
static const char * untracked_xpm[] = {
"6 7 7 1",
" 	c None",
".	c #FFFFFF",
"+	c #1283D5",
"@	c #00008D",
"#	c #00139B",
"$	c #0B6ECE",
"%	c #0016A1",
"......",
".+@@+.",
".@+.@.",
"...#$.",
"..@+..",
"......",
"..@%.."};

/* ------------------
 * TREEVIEW STRUCT
 * ------------------ */

enum
{
	TREEBROWSER_COLUMNC 								= 5,

	TREEBROWSER_COLUMN_ICON 							= 0,
	TREEBROWSER_COLUMN_NAME 							= 1,
	TREEBROWSER_COLUMN_URI 								= 2,
	TREEBROWSER_COLUMN_FLAG 							= 3,
	TREEBROWSER_COLUMN_NAME_CLEAN						= 4,

	TREEBROWSER_RENDER_ICON 							= 0,
	TREEBROWSER_RENDER_TEXT 							= 1,

	TREEBROWSER_FLAGS_SEPARATOR 						= -1
};


/* Keybinding(s) */
enum
{
	KB_FOCUS_FILE_LIST,
	KB_FOCUS_PATH_ENTRY,
	KB_RENAME_OBJECT,
	KB_CREATE_FILE,
	KB_CREATE_DIR,
	KB_REFRESH,
	KB_COUNT
};


/* ------------------
 * PLUGIN INFO
 * ------------------ */

PLUGIN_VERSION_CHECK(224)

PLUGIN_SET_TRANSLATABLE_INFO(
	"/usr/local/share/locale/",
	GETTEXT_PACKAGE,
	_("Purdy"),
	_("This plugin adds a streamlined browser to Geany"),
	"0.9" ,
	"Jeremy Culver (jeremy.culver79@gmail.com)")


/* ------------------
 * PREDEFINES
 * ------------------ */

#define foreach_slist_free(node, list) for (node = list, list = NULL; g_slist_free_1(list), node != NULL; list = node, node = node->next)

static GList*
_gtk_cell_layout_get_cells(GtkTreeViewColumn *column)
{
#if GTK_CHECK_VERSION(2, 12, 0)
	return gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(column));
#else
	return gtk_tree_view_column_get_cell_renderers(column);
#endif
}


/* ------------------
 * PROTOTYPES
 * ------------------ */

static void 	treebrowser_browse2(gchar *directory, gpointer parent);
static void 	gtk_tree_store_iter_clear_nodes(gpointer iter, gboolean delete_root);
static void 	treebrowser_rename_current(void);
static void 	load_settings(void);
static gboolean save_settings(void);
gchar *dialogs_show_text ( const gchar *title, const gchar *label_text, const gchar *default_value );
static void refresh_selected ( );
static void svn_recheck_uri ( const char *uri );

static gchar *backup_create_dir_parts(const gchar *filename)
{
	gchar *parts = NULL;
	gchar *dirname = NULL;

	dirname = g_path_get_dirname(filename);
	if ( !dirname )
		return g_strdup ( "" );

	// if the file is in the project space already, skip the 
	if ( strncmp ( dirname, "/home/harmanci/projects/", 24 ) != 0 )
	{
		parts = g_strdup ( "__non_project__" );
	}
	else
	{
		parts = g_strdup ( dirname+24 );
	}

	gchar *target_dir = g_strconcat ( 
		backupcopy_backup_dir, G_DIR_SEPARATOR_S,
		parts, G_DIR_SEPARATOR_S,
		NULL );

	utils_mkdir(target_dir, TRUE);
	g_free ( target_dir );
	g_free ( dirname );

	return parts;
}

static void document_save_cb(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	FILE *src, *dst;
	gchar *locale_filename_src;
	gchar *locale_filename_dst;
	gchar *basename_src;
	gchar *dir_parts_src;
	gchar *stamp;
	gchar buf[512];
	gint fd_dst = -1;
	gchar backupcopy_time_fmt[] = "%Y-%m-%d-%H-%M-%S";

	locale_filename_src = utils_get_locale_from_utf8(doc->file_name);

	svn_recheck_uri ( locale_filename_src );

	if ((src = g_fopen(locale_filename_src, "r")) == NULL)
	{
		/* it's unlikely that this happens */
		ui_set_statusbar ( FALSE, _("Backup Copy: File could not be read.") );
		g_free(locale_filename_src);
		return;
	}

	stamp = utils_get_date_time(backupcopy_time_fmt, NULL);
	basename_src = g_path_get_basename(locale_filename_src);
	dir_parts_src = backup_create_dir_parts(locale_filename_src);
	locale_filename_dst = g_strconcat(
		backupcopy_backup_dir, G_DIR_SEPARATOR_S,
		dir_parts_src, G_DIR_SEPARATOR_S,
		basename_src, ".", stamp, NULL);
	g_free(basename_src);
	g_free(dir_parts_src);

	msgwin_status_add ( "Backup into %s", locale_filename_dst );

	/* Use g_open() on non-Windows to set file permissions to 600 atomically.
	 * On Windows, seting file permissions would require specific Windows API. */
	fd_dst = g_open(locale_filename_dst, O_CREAT | O_WRONLY, S_IWUSR | S_IRUSR);
	if (fd_dst == -1 || (dst = fdopen(fd_dst, "w")) == NULL)
	{
		ui_set_statusbar(FALSE, _("Backup Copy: File could not be saved.") );
		g_free(locale_filename_src);
		g_free(locale_filename_dst);
		g_free(stamp);
		fclose(src);
		if (fd_dst != -1)
			close(fd_dst);
		return;
	}

	while (fgets(buf, sizeof(buf), src) != NULL)
	{
		fputs(buf, dst);
	}

	//ui_set_statusbar ( FALSE, _("Backup Copy Saved into %s."), local_filename_dst );
	msgwin_status_add ( "Backup created in %s", locale_filename_dst );

	fclose(src);
	fclose(dst);
	if (fd_dst != -1)
		close(fd_dst);
	g_free(locale_filename_src);
	g_free(locale_filename_dst);
	g_free(stamp);
}

/* ------------------
 * PLUGIN CALLBACKS
 * ------------------ */

PluginCallback plugin_callbacks[] =
{
	{ "document-save", (GCallback) &document_save_cb, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};


/* ------------------
 * TREEBROWSER CORE FUNCTIONS
 * ------------------ */

/*
static gboolean
tree_view_row_expanded_iter(GtkTreeView *tree_view, GtkTreeIter *iter)
{
	GtkTreePath *tree_path;
	gboolean expanded;

	tree_path = gtk_tree_model_get_path(gtk_tree_view_get_model(tree_view), iter);
	expanded = gtk_tree_view_row_expanded(tree_view, tree_path);
	gtk_tree_path_free(tree_path);

	return expanded;
}
*/

static GdkPixbuf *
utils_pixbuf_from_stock(const gchar *stock_id)
{
	GtkIconSet *icon_set;

	icon_set = gtk_icon_factory_lookup_default(stock_id);

	if (icon_set)
		return gtk_icon_set_render_icon(icon_set, gtk_widget_get_default_style(),
										gtk_widget_get_default_direction(),
										GTK_STATE_NORMAL, GTK_ICON_SIZE_MENU, NULL, NULL);
	return NULL;
}

/*
static GdkPixbuf *
utils_pixbuf_from_path(gchar *path)
{
#if defined(HAVE_GIO) && GTK_CHECK_VERSION(2, 14, 0)
	GIcon 		*icon;
	GdkPixbuf 	*ret = NULL;
	GtkIconInfo *info;
	gchar 		*ctype;
	gint 		width;

	ctype = g_content_type_guess(path, NULL, 0, NULL);
	icon = g_content_type_get_icon(ctype);
	g_free(ctype);

	if (icon != NULL)
	{
		gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, NULL);
		info = gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(), icon, width, GTK_ICON_LOOKUP_USE_BUILTIN);
		g_object_unref(icon);
		if (!info)
			return NULL;
		ret = gtk_icon_info_load_icon (info, NULL);
		gtk_icon_info_free(info);
	}
	return ret;
#else
	return utils_pixbuf_from_stock(g_file_test(path, G_FILE_TEST_IS_DIR)
									? GTK_STOCK_DIRECTORY
									: GTK_STOCK_FILE);
#endif
}
*/

// return must be g_free'd
gchar *cached_svnversion ( const gchar* path, gboolean unsafe )
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

gchar *svnversion ( const gchar* path )
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

static GList *get_expanded_urls_iter ( GList *uris, GtkTreeIter *iter )
{
	/*
			if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
				gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), path);
			else
				gtk_tree_view_expand_row(GTK_TREE_VIEW(widget), path, FALSE);
	*/
	GtkTreeModel *model = gtk_tree_view_get_model ( GTK_TREE_VIEW(treeview) );
	GtkTreePath	*path = gtk_tree_model_get_path ( model, iter );

	gchar *uri; 
	gtk_tree_model_get(GTK_TREE_MODEL(treestore), iter, TREEBROWSER_COLUMN_URI, &uri, -1);
	if ( !uri )
		return uris;
	if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(treeview), path))
		uris = g_list_append ( uris, g_strdup ( uri ) );

	gint childcnt = gtk_tree_model_iter_n_children ( GTK_TREE_MODEL(treestore), iter );
	for ( int i=0;i<childcnt;i++ )
	{
		GtkTreeIter child_iter;
		if ( gtk_tree_model_iter_nth_child ( GTK_TREE_MODEL(treestore), &child_iter, iter, i ) )
			get_expanded_urls_iter ( uris, &child_iter );
	}

	return uris;
}

static GList *get_expanded_uris ( )
{
	GList *uris = NULL;

	GtkTreeIter iter;
	if ( gtk_tree_model_get_iter_first ( GTK_TREE_MODEL(treestore), &iter ) )
	{
		do 
		{
			uris = get_expanded_urls_iter ( uris, &iter );
		}while ( gtk_tree_model_iter_next ( GTK_TREE_MODEL(treestore), &iter ) );
	}

	return uris;
}

static GdkPixbuf *get_pixbuf ( gchar *uri, const gchar *state, int unsafe )
{
	gboolean is_dir = g_file_test ( uri, G_FILE_TEST_IS_DIR );

	GdkPixbuf *pb = NULL;
	if ( is_dir )
	{
		if ( strcmp ( state, "expanded" ) == 0 )
			pb = utils_pixbuf_from_stock ( GTK_STOCK_OPEN );
		else
			pb = utils_pixbuf_from_stock ( GTK_STOCK_DIRECTORY );
	}
	else
		pb = utils_pixbuf_from_stock ( GTK_STOCK_FILE );
	if ( !pb )
		return NULL;

	GdkPixbuf *pbc = gdk_pixbuf_copy ( pb );
	if ( !pbc )
		return NULL;
	
	gchar *rev = cached_svnversion ( uri, unsafe );
	GdkPixbuf *olay = NULL;
	if ( strcmp ( rev, "..." ) == 0 )
		olay = NULL;
	else if ( strstr ( rev, "Uncommitted local addition" ) )
		olay = gdk_pixbuf_new_from_xpm_data ( added_xpm );
	else if ( strstr ( rev, "M" ) )
		olay = gdk_pixbuf_new_from_xpm_data ( staged_xpm );
	else if ( strstr ( rev, "Unversioned" ) )
		olay = gdk_pixbuf_new_from_xpm_data ( untracked_xpm );
	else
		olay = gdk_pixbuf_new_from_xpm_data ( tracked_xpm );

	if ( olay )
	{
		int olay_w = gdk_pixbuf_get_width ( olay );
		int olay_h = gdk_pixbuf_get_height ( olay );
		int pbc_w = gdk_pixbuf_get_width ( pbc );
		int pbc_h = gdk_pixbuf_get_height ( pbc );

		gdk_pixbuf_composite ( olay, pbc, pbc_w-olay_w, pbc_h-olay_h, olay_w, olay_h, pbc_w-olay_w, pbc_h-olay_h, 1, 1, GDK_INTERP_NEAREST, 255 );
		
		g_object_unref ( olay );
	}

	return pbc;
}

// return must be g_free'd
gchar *make_tree_name ( const gchar *path, const gchar* name, gboolean unsafe )
{
	gchar *ver = cached_svnversion ( path, unsafe );
	gchar *res = NULL;
	if ( strstr ( ver, "Unversioned" ) )
		res = g_strdup ( name );
	else if ( strstr ( ver, "Uncommitted local addition" ) )
		res = g_strdup ( name );
	else
		res = g_strjoin ( " ", name, ver, NULL );
	g_free ( ver );
	return res;
}

static void decorator_refresh_iter ( GtkTreeIter *iter )
{
	gchar *uri = NULL, *fname = NULL, *orig_name = NULL;
	gtk_tree_model_get ( GTK_TREE_MODEL(treestore), iter, TREEBROWSER_COLUMN_URI, &uri, -1 );
	gtk_tree_model_get ( GTK_TREE_MODEL(treestore), iter, TREEBROWSER_COLUMN_NAME_CLEAN, &fname, -1 );
	gtk_tree_model_get ( GTK_TREE_MODEL(treestore), iter, TREEBROWSER_COLUMN_NAME, &orig_name, -1 );
	if ( uri && fname && orig_name )
	{
		gchar *display_name = make_tree_name ( uri, fname, TRUE );

		if ( g_strcmp0 ( orig_name, display_name ) != 0 )
		{
			gtk_tree_store_set ( treestore, iter, TREEBROWSER_COLUMN_NAME, display_name, -1 );

			// NULL or not, set it.
			GdkPixbuf *newpb = get_pixbuf ( uri, "", 1 );
			gtk_tree_store_set ( treestore, iter, TREEBROWSER_COLUMN_ICON, newpb, -1 );
			if ( newpb )
				g_object_unref ( newpb );
		}
	}

	g_free ( uri );
	g_free ( fname );
	g_free ( orig_name );

	gint childcnt = gtk_tree_model_iter_n_children ( GTK_TREE_MODEL(treestore), iter );
	for ( int i=0;i<childcnt;i++ )
	{
		GtkTreeIter child_iter;
		if ( gtk_tree_model_iter_nth_child ( GTK_TREE_MODEL(treestore), &child_iter, iter, i ) )
			decorator_refresh_iter ( &child_iter );
	}
}

static gboolean decorator_refresh_function ( gpointer data )
{
	if ( !treestore || !there_are_updates )
		return TRUE;

	//g_print ( "Decorators need updating..." );
	pthread_mutex_lock ( &svn_cache_mutex );

	// go over the treeview and relabel
	GtkTreeIter iter;
	if ( gtk_tree_model_get_iter_first ( GTK_TREE_MODEL(treestore), &iter ) )
	{
		do 
		{
			decorator_refresh_iter ( &iter );
		}while ( gtk_tree_model_iter_next ( GTK_TREE_MODEL(treestore), &iter ) );
	}

	// clear the update flag
	there_are_updates = 0;
	pthread_mutex_unlock ( &svn_cache_mutex );

	return TRUE;
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

static void svn_recheck_uri ( const char *uri )
{
	gchar *cur = g_strdup ( uri );

	while ( TRUE )
	{
		g_print ( "Rechecking: (%s)", cur );

		if ( strncmp ( cur, "/home/harmanci/projects", strlen("/home/harmanci/projects") ) != 0 )
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

static void *svncache_refresh_function ( void *inp )
{
	//g_print ( "In svnrefresh" );
   
	do
	{
		// TODO: check the cache.
		gchar *uri = NULL, *key = NULL, *val = NULL;

		pthread_mutex_lock ( &svn_cache_mutex );

		GHashTableIter iter;

		g_hash_table_iter_init (&iter, svn_cache_pending);
		while ( g_hash_table_iter_next ( &iter, (gpointer)&key, (gpointer)&val ) )
		{
			if ( key )
			{
				uri = g_strdup ( key );
				break;
			}
		}
		pthread_mutex_unlock ( &svn_cache_mutex );

		if ( uri )
		{
			// this should be out of the locking block
			gchar *ver = svnversion ( uri );

			// locking block:
			//g_print ( "Caching: %s -> %s", uri, ver );
			pthread_mutex_lock ( &svn_cache_mutex );

			g_hash_table_replace ( svn_cache, uri, ver );
			g_hash_table_remove ( svn_cache_pending, uri );

			there_are_updates = 1;

			pthread_mutex_unlock ( &svn_cache_mutex );
		}
		else
			sleep ( 1 );
	}while ( 1 );

	return NULL;
}


/* result must be freed */
static gchar*
path_is_in_dir(gchar* src, gchar* find)
{
	guint i = 0;

	gchar *diffed_path = NULL, *tmp = NULL;
	gchar **src_segments = NULL, **find_segments = NULL;
	guint src_segments_n = 0, find_segments_n = 0, n = 0;

	src_segments = g_strsplit(src, G_DIR_SEPARATOR_S, 0);
	find_segments = g_strsplit(find, G_DIR_SEPARATOR_S, 0);

	src_segments_n = g_strv_length(src_segments);
	find_segments_n = g_strv_length(find_segments);

	n = src_segments_n;
	if (find_segments_n < n)
		n = find_segments_n;

	for(i = 1; i<n; i++)
		if (utils_str_equal(find_segments[i], src_segments[i]) != TRUE)
			break;
		else
		{
			tmp = g_strconcat(diffed_path == NULL ? "" : diffed_path,
								G_DIR_SEPARATOR_S, find_segments[i], NULL);
			g_free(diffed_path);
			diffed_path = tmp;
		}

	g_strfreev(src_segments);
	g_strfreev(find_segments);

	return diffed_path;
}

/* Return: FALSE - if file is filtered and not shown, and TRUE - if file isn`t filtered, and have to be shown */
static gboolean
check_filtered(const gchar *base_name)
{
	const gchar *exts[] 			= {".o", ".obj", ".so", ".dll", ".a", ".lib", ".la", ".lo", ".pyc", ".exe"};
	int exts_len = G_N_ELEMENTS(exts);
	for ( int i = 0; i < exts_len; i++)
	{
		if (g_str_has_suffix(base_name, exts[i]))
			return FALSE;
	}

	return TRUE;
/*
	gchar		**filters;
	guint 		i;
	gboolean 	temporary_reverse 	= FALSE;
	const gchar *exts[] 			= {".o", ".obj", ".so", ".dll", ".a", ".lib", ".la", ".lo", ".pyc", ".exe"};
	guint exts_len;
	const gchar *ext;
	gboolean	filtered;

	if (CONFIG_HIDE_OBJECT_FILES)
	{
		exts_len = G_N_ELEMENTS(exts);
		for (i = 0; i < exts_len; i++)
		{
			ext = exts[i];
			if (g_str_has_suffix(base_name, ext))
				return FALSE;
		}
	}

	if (EMPTY(gtk_entry_get_text(GTK_ENTRY(filter))))
		return TRUE;

	filters = g_strsplit(gtk_entry_get_text(GTK_ENTRY(filter)), ";", 0);

	if (utils_str_equal(filters[0], "!") == TRUE)
	{
		temporary_reverse = TRUE;
		i = 1;
	}
	else
		i = 0;

	filtered = CONFIG_REVERSE_FILTER || temporary_reverse ? TRUE : FALSE;
	for (; filters[i]; i++)
	{
		if (utils_str_equal(base_name, "*") || g_pattern_match_simple(filters[i], base_name))
		{
			filtered = CONFIG_REVERSE_FILTER || temporary_reverse ? FALSE : TRUE;
			break;
		}
	}
	g_strfreev(filters);

	return filtered;
*/
}

#ifdef G_OS_WIN32
static gboolean
win32_check_hidden(const gchar *filename)
{
	DWORD attrs;
	static wchar_t w_filename[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, w_filename, sizeof(w_filename));
	attrs = GetFileAttributesW(w_filename);
	if (attrs != INVALID_FILE_ATTRIBUTES && attrs & FILE_ATTRIBUTE_HIDDEN)
		return TRUE;
	return FALSE;
}
#endif

/* Returns: whether name should be hidden. */
static gboolean
check_hidden(const gchar *filename)
{
	gsize len;
	const gchar *base_name = NULL;
	base_name = g_path_get_basename(filename);

	if (EMPTY(base_name))
		return FALSE;

	if (CONFIG_SHOW_HIDDEN_FILES)
		return FALSE;

#ifdef G_OS_WIN32
	if (win32_check_hidden(filename))
		return TRUE;
#else
	if (base_name[0] == '.')
		return TRUE;
#endif

	len = strlen(base_name);
	if (base_name[len - 1] == '~')
		return TRUE;

	return FALSE;
}

static gchar*
get_default_dir(void)
{
	//static gchar path[] = "/home/harmanci/projects";
	//return path;

	gchar 			*dir;
	GeanyProject 	*project 	= geany->app->project;
	GeanyDocument	*doc 		= document_get_current();

	if (doc != NULL && doc->file_name != NULL && g_path_is_absolute(doc->file_name))
	{
		gchar *dir_name;
		gchar *ret;

		dir_name = g_path_get_dirname(doc->file_name);
		ret = utils_get_locale_from_utf8(dir_name);
		g_free (dir_name);

		return ret;
	}

	if (project)
		dir = project->base_path;
	else
		dir = geany->prefs->default_open_path;

	if (! EMPTY(dir))
		return utils_get_locale_from_utf8(dir);

	return g_get_current_dir();
}

/*
static gchar *
get_terminal(void)
{
	gchar 		*terminal;
#ifdef G_OS_WIN32
	terminal = g_strdup("cmd");
#else
	terminal = g_strdup(CONFIG_OPEN_TERMINAL);
#endif
	return terminal;
}
*/

static gboolean
treebrowser_checkdir(gchar *directory)
{
	gboolean is_dir;
	//static const GdkColor red 	= {0, 0xffff, 0x6666, 0x6666};
	//static const GdkColor white = {0, 0xffff, 0xffff, 0xffff};
	static gboolean old_value = TRUE;

	is_dir = g_file_test(directory, G_FILE_TEST_IS_DIR);

	if (old_value != is_dir)
	{
		//gtk_widget_modify_base(GTK_WIDGET(addressbar), GTK_STATE_NORMAL, is_dir ? NULL : &red);
		//gtk_widget_modify_text(GTK_WIDGET(addressbar), GTK_STATE_NORMAL, is_dir ? NULL : &white);
		old_value = is_dir;
	}

	if (!is_dir)
	{
		if (CONFIG_SHOW_BARS == 0)
			dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("%s: no such directory."), directory);

		return FALSE;
	}

	return is_dir;
}

static void
treebrowser_chroot(const gchar *dir)
{
	dir = g_strdup ( _("/home/harmanci/projects/") );

	gchar *directory;

	if (g_str_has_suffix(dir, G_DIR_SEPARATOR_S))
		directory = g_strndup(dir, strlen(dir)-1);
	else
		directory = g_strdup(dir);

	if (!directory || strlen(directory) == 0)
		setptr(directory, g_strdup(G_DIR_SEPARATOR_S));

	if (! treebrowser_checkdir(directory))
	{
		g_free(directory);
		return;
	}

	gtk_tree_store_clear(treestore);
	setptr(addressbar_last_address, directory);

	treebrowser_browse2(addressbar_last_address, NULL);
}

static void treebrowser_browse ( gchar *in_directory, gpointer parent, GList *expanded_uris )
{
	GtkTreeIter 	iter, iter_empty, *last_dir_iter = NULL;
	gboolean 		is_dir;
	gboolean 		has_parent;
	gchar 			*utf8_name;
	GSList 			*list, *node;
	gchar 			*fname;
	gchar 			*uri;
	GtkTreeModel 	*model = gtk_tree_view_get_model ( GTK_TREE_VIEW(treeview) );
	GtkTreePath 	*parent_path = NULL;
	GList		*expand_paths = NULL;
	gchar 			*directory = g_strconcat(in_directory, G_DIR_SEPARATOR_S, NULL);

	g_print ( "Browsing %s", in_directory );

	has_parent = parent ? gtk_tree_store_iter_is_valid(treestore, parent) : FALSE;
	if ( has_parent )
	{
		parent_path = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), parent);
	}
	else
		parent = NULL;

	if (parent)
		gtk_tree_store_iter_clear_nodes(parent, FALSE);
	else
		gtk_tree_store_clear ( treestore );

	list = utils_get_file_list(directory, NULL, NULL);
	if (list != NULL)
	{
		foreach_slist_free(node, list)
		{
			fname 		= node->data;
			uri 		= g_strconcat(directory, fname, NULL);
			is_dir 		= g_file_test (uri, G_FILE_TEST_IS_DIR);
			utf8_name 	= utils_get_utf8_from_locale(fname);
			gchar *display_name = make_tree_name ( uri, fname, FALSE );

			if (!check_hidden(uri))
			{
				GdkPixbuf *icon = NULL;

				if (is_dir)
				{
					if (last_dir_iter == NULL)
						gtk_tree_store_prepend(treestore, &iter, parent);
					else
					{
						gtk_tree_store_insert_after(treestore, &iter, parent, last_dir_iter);
						gtk_tree_iter_free(last_dir_iter);
					}

					for ( GList *lnode = expanded_uris;lnode;lnode = lnode->next )
					{
						if ( lnode->data && uri && strcmp ( lnode->data, uri ) == 0 )
						{
							g_print ( "We need to browse into this path too." );

							// Create the path and put it in a list.
							GtkTreePath *path = gtk_tree_model_get_path ( model, &iter );
							struct t_treenode *tn = (struct t_treenode*)calloc ( sizeof(struct t_treenode), 1 );
							tn->path = gtk_tree_path_to_string ( path );
							tn->uri = g_strdup ( uri );
							gtk_tree_path_free ( path );

							expand_paths = g_list_append ( expand_paths, (gpointer)tn );
						}
					}


					last_dir_iter = gtk_tree_iter_copy(&iter);
					icon = get_pixbuf ( uri, "", 0 );
					gtk_tree_store_set ( treestore, &iter,
										TREEBROWSER_COLUMN_ICON, icon,
										TREEBROWSER_COLUMN_NAME, display_name,
										TREEBROWSER_COLUMN_URI, uri,
										TREEBROWSER_COLUMN_NAME_CLEAN, fname, -1);

					if ( icon )
						g_object_unref ( icon );
					gtk_tree_store_prepend(treestore, &iter_empty, &iter);
					gtk_tree_store_set(treestore, &iter_empty,
									TREEBROWSER_COLUMN_ICON, NULL,
									TREEBROWSER_COLUMN_NAME, _("(Empty)"),
									TREEBROWSER_COLUMN_NAME_CLEAN, _("(Empty)"),
									TREEBROWSER_COLUMN_URI, NULL, -1);
				}
				else
				{
					if (check_filtered(utf8_name))
					{
						icon = get_pixbuf ( uri, "", 0 );
						gtk_tree_store_append(treestore, &iter, parent);
						gtk_tree_store_set(treestore, &iter,
										TREEBROWSER_COLUMN_ICON, icon,
										TREEBROWSER_COLUMN_NAME, display_name,
										TREEBROWSER_COLUMN_NAME_CLEAN, fname,
										TREEBROWSER_COLUMN_URI, uri, -1);
						if ( icon )
							g_object_unref ( icon );
					}
				}
			}
			g_free ( display_name );
			g_free(utf8_name);
			g_free(uri);
			g_free(fname);
		}
	}
	else
	{
		gtk_tree_store_prepend(treestore, &iter_empty, parent);
		gtk_tree_store_set(treestore, &iter_empty,
						TREEBROWSER_COLUMN_ICON, 	NULL,
						TREEBROWSER_COLUMN_NAME, 	_("(Empty)"),
						TREEBROWSER_COLUMN_NAME_CLEAN, 	_("(Empty)"),
						TREEBROWSER_COLUMN_URI, 	NULL,
						-1);
	}

	for ( GList *lnode = expanded_uris;lnode;lnode = lnode->next )
	{
		if ( parent_path && lnode->data && in_directory && strcmp ( lnode->data, in_directory ) == 0 )
		{
			gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), parent_path, FALSE);
			GtkTreeIter parent_iter;
			if ( gtk_tree_model_get_iter ( model, &parent_iter, parent_path) )
			{
				g_print ( "Set expand icon %s", in_directory );
				GdkPixbuf *icon = get_pixbuf ( in_directory, "expanded", 0 );
				gtk_tree_store_set ( treestore, &parent_iter, TREEBROWSER_COLUMN_ICON, icon, -1);
				if ( icon )
					g_object_unref ( icon );
			}
			break;
		}
	}

	for ( GList *lnode = expand_paths;lnode;lnode = lnode->next )
	{
		struct t_treenode *tn = (struct t_treenode*)lnode->data;
		
/*
		GtkTreePath *path = gtk_tree_path_new_from_string ( tn->path );
		if ( path )
		{
			GtkTreeIter iter2;
			if ( gtk_tree_model_get_iter ( model, &iter2, path) )
			{
				treebrowser_browse ( tn->uri, &iter2, expanded_uris );
			}
			gtk_tree_path_free ( path );
		}
*/
		g_free ( tn->path );
		g_free ( tn->uri );
	}

	gtk_tree_path_free ( parent_path );
	g_free ( directory );
}

static void treebrowser_browse2 ( gchar *directory, gpointer parent )
{
	GList *expanded_uris = get_expanded_uris ( );
	treebrowser_browse ( directory, parent, expanded_uris );

/*
	// Iterate the table and browse
	GtkTreeIter iter;
	if ( gtk_tree_model_get_iter_first ( GTK_TREE_MODEL(treestore), &iter ) )
	{
		do 
		{
			uris = get_expanded_urls_iter ( uris, &iter );
		}while ( gtk_tree_model_iter_next ( GTK_TREE_MODEL(treestore), &iter ) );
	}
*/
	for ( GList *lnode = expanded_uris;lnode;lnode = lnode->next )
	{
		g_free ( lnode->data );
	}
	g_list_free ( expanded_uris );
}

static gboolean treebrowser_search(gchar *uri, gpointer parent)
{
	GtkTreeIter 	iter;
	GtkTreePath 	*path;
	gchar 			*uri_current;

	if (gtk_tree_model_iter_children(GTK_TREE_MODEL(treestore), &iter, parent))
	{
		do
		{
			if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(treestore), &iter))
				if (treebrowser_search(uri, &iter))
					return TRUE;

			gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter, TREEBROWSER_COLUMN_URI, &uri_current, -1);

			if (utils_str_equal(uri, uri_current) == TRUE)
			{
				path = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), &iter);
				gtk_tree_view_expand_to_path(GTK_TREE_VIEW(treeview), path);
				gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, NULL, FALSE, 0, 0);
				gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview), path, treeview_column_text, FALSE);
				gtk_tree_path_free(path);
				g_free(uri_current);
				return TRUE;
			}
			else
				g_free(uri_current);

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(treestore), &iter));
	}

	return FALSE;
}

static void
showbars(gboolean state)
{
	if (state)
	{
		gtk_widget_show(sidebar_vbox_bars);
		if (!CONFIG_SHOW_BARS)
			CONFIG_SHOW_BARS = 1;
	}
	else
	{
		gtk_widget_hide(sidebar_vbox_bars);
		CONFIG_SHOW_BARS = 0;
	}

	save_settings();
}

static void
gtk_tree_store_iter_clear_nodes(gpointer iter, gboolean delete_root)
{
	GtkTreeIter i;

	if (gtk_tree_model_iter_children(GTK_TREE_MODEL(treestore), &i, iter))
	{
		while (gtk_tree_store_remove(GTK_TREE_STORE(treestore), &i))
			/* do nothing */;
	}
	if (delete_root)
		gtk_tree_store_remove(GTK_TREE_STORE(treestore), iter);
}

static gboolean
treebrowser_expand_to_path(gchar* root, gchar* find)
{
	guint i = 0;
	gboolean founded = FALSE, global_founded = FALSE;
	gchar *new = NULL;
	gchar **root_segments = NULL, **find_segments = NULL;
	guint find_segments_n = 0;

	root_segments = g_strsplit(root, G_DIR_SEPARATOR_S, 0);
	find_segments = g_strsplit(find, G_DIR_SEPARATOR_S, 0);

	find_segments_n = g_strv_length(find_segments)-1;


	for (i = 1; i<=find_segments_n; i++)
	{
		new = g_strconcat(new ? new : "", G_DIR_SEPARATOR_S, find_segments[i], NULL);

		if (founded)
		{
			if (treebrowser_search(new, NULL))
				global_founded = TRUE;
		}
		else
			if (utils_str_equal(root, new) == TRUE)
				founded = TRUE;
	}

	g_free(new);
	g_strfreev(root_segments);
	g_strfreev(find_segments);

	return global_founded;
}

static gboolean
treebrowser_track_current(void)
{

	GeanyDocument	*doc 		= document_get_current();
	gchar 			*path_current;
	gchar			**path_segments = NULL;
	gchar 			*froot = NULL;

	if (doc != NULL && doc->file_name != NULL && g_path_is_absolute(doc->file_name))
	{
		path_current = utils_get_locale_from_utf8(doc->file_name);

		/*
		 * Checking if the document is in the expanded or collapsed files
		 */
		if (! treebrowser_search(path_current, NULL))
		{
			/*
			 * Else we have to chroting to the document`s nearles path
			 */

			froot = path_is_in_dir(addressbar_last_address, g_path_get_dirname(path_current));

			if (froot == NULL)
				froot = g_strdup(G_DIR_SEPARATOR_S);

			if (utils_str_equal(froot, addressbar_last_address) != TRUE)
				treebrowser_chroot(froot);

			treebrowser_expand_to_path(froot, path_current);
		}

		g_strfreev(path_segments);
		g_free(froot);
		g_free(path_current);

		return FALSE;
	}
	return FALSE;
}

/*
static gboolean treebrowser_iter_rename(gpointer iter)
{
	GtkTreeViewColumn 	*column;
	GtkCellRenderer 	*renderer;
	GtkTreePath 		*path;
	GList 				*renderers;

	if (gtk_tree_store_iter_is_valid(treestore, iter))
	{
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), iter);
		if (G_LIKELY(path != NULL))
		{
			column 		= gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 0);
			renderers 	= _gtk_cell_layout_get_cells(column);
			renderer 	= g_list_nth_data(renderers, TREEBROWSER_RENDER_TEXT);

			g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);
			gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(treeview), path, column, renderer, TRUE);

			gtk_tree_path_free(path);
			g_list_free(renderers);
			return TRUE;
		}
	}
	return FALSE;
}
*/

static void
treebrowser_rename_current(void)
{
/*
	GtkTreeSelection 	*selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeIter 		iter;
	GtkTreeModel 		*model;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		treebrowser_iter_rename(&iter);
	}
*/
}


/* ------------------
 * RIGHTCLICK MENU EVENTS
 * ------------------*/

static void
on_menu_open_externally(GtkMenuItem *menuitem, gchar *uri)
{
	gchar *safe = g_shell_quote ( uri );
	gchar *cmd = g_strjoin ( " ", "xdg-open", safe, NULL );

	g_spawn_command_line_async ( cmd, NULL );

	g_free(cmd);
	g_free(safe);
}

static void
on_menu_find_in_files(GtkMenuItem *menuitem, gchar *uri)
{
	search_show_find_in_files_dialog(uri);
}

/*
static void
on_menu_rename(GtkMenuItem *menuitem, gpointer *user_data)
{
	treebrowser_rename_current();
}
*/

static void remove_uri ( gchar *uri )
{
	gchar fn[PATH_MAX];
	snprintf ( fn, sizeof(fn)-1, "/tmp/geany-deleted-%ld-%d-%d", time(NULL), getpid(), delete_no );

	gchar *safe = g_shell_quote ( uri );

	gchar *cmd = g_strjoin ( " ", "mv", "-f", safe, fn, NULL );

	g_spawn_command_line_sync ( cmd, NULL, NULL, NULL, NULL );
	//fprintf ( stderr, "%s\n", cmd );
	delete_no++;
}

static GList *get_selected_uris ( )
{
	GList *uris = NULL;
	GtkTreeSelection *selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(treeview) );
	GtkTreeModel *model = gtk_tree_view_get_model ( GTK_TREE_VIEW(treeview) );
	GList *list = gtk_tree_selection_get_selected_rows ( selection, &model );
	for ( GList *node = list;node;node = node->next )
	{
		GtkTreeIter iter;
		gtk_tree_model_get_iter ( model, &iter, node->data );

		gchar *uri; 
		gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
		if ( uri )
		{
			uris = g_list_append ( uris, g_strdup ( uri ) );
		}
	}

	return uris;
}

//Record and re-apply this after regular refreshes

static void on_menu_delete(GtkMenuItem *menuitem, gpointer *user_data)
{
	GList *uris = get_selected_uris ( );

	for ( GList *l = uris; l != NULL; l = l->next)
	{
		gchar *uri = (gchar*)l->data;

		if ( dialogs_show_question ( _("Do you really want to delete '%s' ?"), uri ) )
		{
			g_print ( "Delete %s", uri );
			gchar *uri_parent = g_path_get_dirname(uri);
			remove_uri ( uri );					

			/*
			GtkTreeIter iter_parent;
			if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(treestore), &iter_parent, &iter))
				treebrowser_browse(uri_parent, &iter_parent);
			else
				treebrowser_browse(uri_parent, NULL);
			*/
			g_free(uri_parent);
		}
		g_free ( uri );
	}

	treebrowser_browse2 ( _("/home/harmanci/projects/"), NULL);
	g_list_free ( uris );
}

static void on_menu_rename ( GtkMenuItem *menuitem, gpointer *user_data )
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(treeview) );
	if ( gtk_tree_selection_count_selected_rows ( selection ) != 1 )
		return;

	GList *uris = get_selected_uris ( );

	for ( GList *l = uris; l != NULL; l = l->next)
	{
		gchar *uri = (gchar*)l->data;
		if ( !uri )
			break;

		gchar *fname = g_path_get_basename ( uri );
		gchar *root_name = g_path_get_dirname ( uri );

		gchar *title = g_strconcat ( "New name for ", fname, NULL );

		gchar *new_name = dialogs_show_input ( "Rename", NULL, title, fname );
		if ( new_name && new_name[0] )
		{
			gchar *new_uri = g_strconcat ( root_name, "/", new_name, NULL );
			if ( dialogs_show_question ( _("Do you really want to rename '%s' to '%s'?"), uri, new_uri ) )
			{
				msgwin_status_add ( "Rename %s to %s/%s", uri, root_name, new_name );

				gchar *safe_uri = g_shell_quote ( uri );
				gchar *safe_new_uri = g_shell_quote ( new_uri );

				gchar *cmd = g_strjoin ( " ", "mv", "-n", safe_uri, safe_new_uri, NULL );
				g_free ( safe_uri );
				g_free ( safe_new_uri );
				if ( dialogs_show_question ( _("%s?"), cmd ) )
				{
					gint status;
					gchar *output, *error;
					g_spawn_command_line_sync ( cmd, &output, &error, &status, NULL );
					if ( status != 0 )
					{
						dialogs_show_msgbox ( GTK_MESSAGE_ERROR,_("Rename failed.") );
					}

					svn_recheck_uri ( new_uri );
					svn_recheck_uri ( uri );
				}
				g_free ( cmd );
			}
			g_free ( new_uri );
		}
		g_free ( uri );
		g_free ( title );

		break;
	}

	treebrowser_browse2 ( _("/home/harmanci/projects/"), NULL);
	g_list_free ( uris );
}

static void remove_from_svn_cache ( gchar *prefix )
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

static void refresh_selected ( )
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(treeview) );
	GtkTreeModel *model = gtk_tree_view_get_model ( GTK_TREE_VIEW(treeview) );

	GList *list = gtk_tree_selection_get_selected_rows ( selection, &model );
	for ( GList *node = list;node;node = node->next )
	{
		GtkTreeIter iter;
		gtk_tree_model_get_iter ( model, &iter, node->data );

		gchar *uri, *clean_name; 
		gtk_tree_model_get ( GTK_TREE_MODEL(treestore), &iter, TREEBROWSER_COLUMN_URI, &uri, 
			TREEBROWSER_COLUMN_NAME_CLEAN, &clean_name, -1 );

		// remove from svn cache so we can trigger an update
		remove_from_svn_cache ( uri );

		// Rename to pending
		gchar *display_name = make_tree_name ( uri, clean_name, TRUE );
		gtk_tree_store_set ( treestore, &iter, TREEBROWSER_COLUMN_NAME, display_name, -1 );

		if ( g_file_test ( uri, G_FILE_TEST_IS_DIR ) )
		{
			treebrowser_browse2 ( uri, &iter );
		}

		g_free(uri);
		g_free(display_name);
		g_free(clean_name);
	}
}

static void on_menu_refresh ( GtkMenuItem *menuitem, gpointer *user_data)
{
	refresh_selected ( );
}

/*
static void
on_menu_expand_all(GtkMenuItem *menuitem, gpointer *user_data)
{
	gtk_tree_view_expand_all(GTK_TREE_VIEW(treeview));
}
*/

static void
on_menu_collapse_all(GtkMenuItem *menuitem, gpointer *user_data)
{
	gtk_tree_view_collapse_all(GTK_TREE_VIEW(treeview));
}

/*
static void
on_menu_close(GtkMenuItem *menuitem, gchar *uri)
{
	if (g_file_test(uri, G_FILE_TEST_EXISTS))
		document_close(document_find_by_filename(uri));
}
static void
on_menu_close_children(GtkMenuItem *menuitem, gchar *uri)
{
	guint i;
	size_t uri_len = strlen(uri);
	for (i=0; i < GEANY(documents_array)->len; i++)
	{
		if (documents[i]->is_valid)
		{
			// the document filename should always be longer than the uri when closing children
			// Compare the beginning of the filename string to see if it matchs the uri
			if (strlen(documents[i]->file_name) > uri_len)
			{
				if (strncmp(uri, documents[i]->file_name, uri_len)==0)
					document_close(documents[i]);
			}
		}
	}
}
*/

static void
on_menu_copy_uri(GtkMenuItem *menuitem, gchar *uri_in)
{
	GList *uris = get_expanded_uris ( );
	for ( GList *l = uris; l != NULL; l = l->next)
	{
		gchar *uri = (gchar*)l->data;
		g_print ( "Expanded: %s", uri );
	}

	GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(cb, uri_in, -1);
}

static void
on_menu_show_hidden_files(GtkMenuItem *menuitem, gpointer *user_data)
{
	CONFIG_SHOW_HIDDEN_FILES = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem));
	save_settings();
	treebrowser_browse2(addressbar_last_address, NULL);
}

static void
on_menu_show_bars(GtkMenuItem *menuitem, gpointer *user_data)
{
	showbars(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)));
}

static void run_svn_action ( const gchar *action, const gchar *uri )
{
	if ( !dialogs_show_question ( _("Do you really want to svn %s '%s'?"), action, uri ) )
		return;

	// Get a commit message;
	gchar *msg = NULL;

	if ( strcmp ( action, "commit" ) == 0 )
	{
		gchar *label = g_strconcat ( "Commit message for ", uri, NULL );
		msg = dialogs_show_text ( "SVN Commit", label, "" );
		if ( !msg )
			return;
		if ( !msg[0] )
		{
			g_free ( msg );
			return;
		}
	}
	
	gchar *safe_msg = NULL;
	gchar *safe = g_shell_quote ( uri );
	if ( msg )
	{
		safe_msg = g_shell_quote ( msg );
		g_free ( msg );
	}
	gchar *cmd = NULL;
	if ( safe_msg )
		cmd = g_strjoin ( " ", "svn", "-m", safe_msg, action, safe, NULL );
	else
		cmd = g_strjoin ( " ", "svn", action, safe, NULL );

	g_free ( safe );
	g_free ( safe_msg );

	gint status;
	gchar *output, *error;
	g_spawn_command_line_sync ( cmd, &output, &error, &status, NULL );
	g_free ( cmd );

	msgwin_status_add ( "Svn %s output for %s:\nSTDOUT:%s\nSTDERR:%s\nSTATUS:%d\n", action, uri, output, error, status );
	if ( status )
	{
		dialogs_show_msgbox	( GTK_MESSAGE_ERROR, "SVN %s error:\n%s\n", action, error );
	}
	g_free ( output );
	g_free ( error );

	svn_recheck_uri ( uri );
}

static void on_menu_svn(GtkMenuItem *menuitem, gchar *action)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(treeview) );
	GtkTreeModel *model = gtk_tree_view_get_model ( GTK_TREE_VIEW(treeview) );
	GtkTreeIter iter;
	int cnt = 0;

	char meld_cmd[PATH_MAX*3 + 2] = "meld";

	g_print ( "SVN %s on:", action );
	GList *list = gtk_tree_selection_get_selected_rows ( selection, &model );
	for ( GList *node = list;node;node = node->next )
	{
		gchar *uri; 
		gtk_tree_model_get_iter ( model, &iter, node->data );

		gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
							TREEBROWSER_COLUMN_URI, &uri,
							-1);

		if ( strcmp ( action, "add" ) == 0 || 
			strcmp ( action, "delete" ) == 0 || 
			strcmp ( action, "revert" ) == 0 ||
			strcmp ( action, "commit" ) == 0 ||
			strcmp ( action, "update" ) == 0 )
		{
			run_svn_action ( action, uri );
		}

		g_print ( "... %s", uri );
		if ( cnt < 2 && strcmp ( action, "meld" ) == 0 )
		{
			gchar *safe = g_shell_quote ( uri );

			g_strlcat ( meld_cmd, " ", sizeof(meld_cmd)-1 );
			g_strlcat ( meld_cmd, safe, sizeof(meld_cmd)-1 );

			g_free ( safe );
		}
		cnt ++;
	}

	if ( cnt <= 2 && cnt >= 1 && strcmp ( action, "meld" ) == 0 )
	{
		g_print ( "%s", meld_cmd );
		//g_spawn_command_line_sync ( meld_cmd, NULL, NULL, NULL, NULL );
		g_spawn_command_line_async ( meld_cmd, NULL );
	}

	// refresh selected
	refresh_selected ( );
}


static gboolean get_selected_iter ( GtkTreeIter *sel_iter )
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(treeview) );
	GtkTreeModel *model = gtk_tree_view_get_model ( GTK_TREE_VIEW(treeview) );
	if ( gtk_tree_selection_count_selected_rows ( selection ) != 1 )
		return FALSE;

	gboolean found = FALSE;
	GList *list = gtk_tree_selection_get_selected_rows ( selection, &model );
	for ( GList *node = list;node;node = node->next )
	{
		found = TRUE;
		gtk_tree_model_get_iter ( model, sel_iter, node->data );
		break;
	}

	// TODO: Do this everywhere
	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);

	return found;
}

static void on_menu_create_new_object(GtkMenuItem *menuitem, const gchar *type)
{
	GtkTreeModel *model = gtk_tree_view_get_model ( GTK_TREE_VIEW(treeview) );
	GtkTreeIter 		iter;

	gchar 				*uri = NULL;

	if ( get_selected_iter ( &iter ) )
	{
		gtk_tree_model_get ( model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1 );

		if ( uri && !g_file_test ( uri, G_FILE_TEST_IS_DIR ) )
		{
			dialogs_show_msgbox ( GTK_MESSAGE_ERROR,_("You must select a folder.") );
			return;
		}
	}
	else
	{
		uri = g_strdup ( "/home/harmanci/projects/" );
	}

	gchar *new_name = dialogs_show_input ( "Enter name", NULL, "New name", "" );
	if ( !new_name )
		return;

	gchar *uri_new = NULL;
	if (utils_str_equal(type, "directory"))
		uri_new = g_strconcat ( uri, "/", new_name, NULL );
	else
		uri_new = g_strconcat ( uri, "/", new_name, NULL );
	g_free ( uri );
	if ( g_file_test(uri_new, G_FILE_TEST_EXISTS) )
	{
		dialogs_show_msgbox ( GTK_MESSAGE_ERROR, _("%s already exits."), uri_new );
		g_free ( uri_new );
		return;
	}

	if ( dialogs_show_question(_("Create '%s'?"), uri_new) )
	{
		gboolean creation_success = FALSE;
		if (utils_str_equal(type, "directory"))
			creation_success = (g_mkdir(uri_new, 0755) == 0);
		else
			creation_success = (g_creat(uri_new, 0644) != -1);

		if ( !creation_success )
			dialogs_show_msgbox ( GTK_MESSAGE_ERROR, _("Failed to create %s."), uri_new );
		else
		{
			document_open_file ( uri_new, FALSE, NULL, NULL );

			svn_recheck_uri ( uri_new );

			refresh_selected ( );
		}
	}
	g_free(uri_new);
}

GtkWidget *ui_dialog_vbox_new(GtkDialog *dialog)
{
	const gint BUTTON_BOX_BORDER = 5;

	GtkWidget *vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), BUTTON_BOX_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), vbox);
	return vbox;
}

gchar *dialogs_show_text ( const gchar *title, const gchar *label_text, const gchar *default_value )
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons(title, NULL,
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL );

	GtkWidget *vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_widget_set_name(dialog, "GeanyDialog");

	GtkWidget *label = gtk_label_new(label_text);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	//gtk_widget_set_size_request(label, 800, 40);

    GtkWidget* scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	GtkWidget *text = gtk_text_view_new();

	gtk_container_add ( GTK_CONTAINER(scrolledwindow), text );

	gtk_container_add(GTK_CONTAINER(vbox), label);
	gtk_container_add(GTK_CONTAINER(vbox), scrolledwindow);

	gtk_widget_show_all(vbox);

	//gtk_widget_set_size_request(dialog, 800, 600);

	gchar *msg = NULL;
	if ( gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT )
	{
		GtkTextBuffer *buffer = gtk_text_view_get_buffer ( (GtkTextView*)text );

		GtkTextIter start_iter, end_iter;
		gtk_text_buffer_get_start_iter ( buffer, &start_iter );
		gtk_text_buffer_get_end_iter ( buffer, &end_iter );

		msg = gtk_text_buffer_get_text ( buffer, &start_iter, &end_iter, TRUE );
	}
	gtk_widget_destroy(dialog);

	return msg;
}
static void on_test (GtkMenuItem *menuitem, gchar *action)
{
	//GtkWindow *parent = NULL;
	//gchar *res = dialogs_show_input ( "Testing", parent, "label_text", "default_text" );
	//g_print ( "GOT: (%s)", res );

	dialogs_show_text ( "Testing", "Enter this and that:", "Default is this\nand this\n" );
}

static GtkWidget *create_popup_menu(const gchar *name, const gchar *all_uri, int sel_count)
{
	GtkWidget *item, *menu = gtk_menu_new();

	gboolean is_exists 		= g_file_test(all_uri, G_FILE_TEST_EXISTS);
	gboolean is_dir 		= is_exists ? g_file_test(all_uri, G_FILE_TEST_IS_DIR) : FALSE;
	//gboolean is_document 	= document_find_by_filename(all_uri) != NULL ? TRUE : FALSE;

	// To Disable/enable use:
	//gtk_widget_set_sensitive(item, is_exists);

	item = ui_image_menu_item_new(GTK_STOCK_REFRESH, _("Refresh"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_refresh), NULL);

	item = ui_image_menu_item_new(GTK_STOCK_GO_BACK, _("Meld"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_svn), (gpointer)"meld" );

	if ( sel_count == 1 && is_dir )
	{
		// Find - separator
		item = gtk_separator_menu_item_new();
		gtk_container_add(GTK_CONTAINER(menu), item);
		gtk_widget_show(item);

		item = ui_image_menu_item_new(GTK_STOCK_FIND, _("_Find in Files"));
		gtk_container_add(GTK_CONTAINER(menu), item);
		g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_find_in_files), g_strdup(all_uri), (GClosureNotify)g_free, 0);
	}

	if ( sel_count == 1 )
	{
		// New item create - separator
		item = gtk_separator_menu_item_new();
		gtk_container_add(GTK_CONTAINER(menu), item);
		gtk_widget_show(item);

		item = ui_image_menu_item_new(GTK_STOCK_NEW, _("Rename"));
		gtk_container_add(GTK_CONTAINER(menu), item);
		g_signal_connect(item, "activate", G_CALLBACK(on_menu_rename), (gpointer)"");

		item = ui_image_menu_item_new(GTK_STOCK_ADD, _("Create new directory"));
		gtk_container_add(GTK_CONTAINER(menu), item);
		g_signal_connect(item, "activate", G_CALLBACK(on_menu_create_new_object), (gpointer)"directory");

		item = ui_image_menu_item_new(GTK_STOCK_NEW, _("Create new file"));
		gtk_container_add(GTK_CONTAINER(menu), item);
		g_signal_connect(item, "activate", G_CALLBACK(on_menu_create_new_object), (gpointer)"file");
	}

	// SVN - separator
	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_widget_show(item);

	item = ui_image_menu_item_new(GTK_STOCK_GO_FORWARD, _("SVN Update"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_svn), (gpointer)"update" );

	item = ui_image_menu_item_new(GTK_STOCK_GO_BACK, _("SVN Commit"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_svn), (gpointer)"commit" );

	item = ui_image_menu_item_new(GTK_STOCK_GO_BACK, _("SVN Add"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_svn), (gpointer)"add" );

	item = ui_image_menu_item_new(GTK_STOCK_GO_BACK, _("SVN Delete"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_svn), (gpointer)"delete" );

	item = ui_image_menu_item_new(GTK_STOCK_GO_BACK, _("SVN Revert"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_svn), (gpointer)"revert" );

	//item = ui_image_menu_item_new(GTK_STOCK_GO_BACK, _("SVN TEST"));
	//gtk_container_add(GTK_CONTAINER(menu), item);
	//g_signal_connect(item, "activate", G_CALLBACK(on_test), (gpointer)"test" );

	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_widget_show(item);

	/* CREATE */
	/*
	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = ui_image_menu_item_new(GTK_STOCK_ADD, _("Create new directory"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_create_new_object), (gpointer)"directory");

	item = ui_image_menu_item_new(GTK_STOCK_NEW, _("Create new file"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_create_new_object), (gpointer)"file");
	*/
	if ( sel_count == 1 )
	{
		item = ui_image_menu_item_new(GTK_STOCK_OPEN, _("Default open"));
		gtk_container_add(GTK_CONTAINER(menu), item);
		g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_open_externally), g_strdup(all_uri), (GClosureNotify)g_free, 0);
	}
	
	/*
	if ( sel_count == 1 && is_exists )
	{
		item = ui_image_menu_item_new(GTK_STOCK_SAVE_AS, _("Rename"));
		gtk_container_add(GTK_CONTAINER(menu), item);
		g_signal_connect(item, "activate", G_CALLBACK(on_menu_rename), NULL);
	}
	*/

	item = ui_image_menu_item_new(GTK_STOCK_DELETE, _("Delete"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_delete), NULL);

	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = ui_image_menu_item_new(GTK_STOCK_COPY, _("_Copy full path to clipboard"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_copy_uri), g_strdup(all_uri), (GClosureNotify)g_free, 0);

	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_widget_show(item);

	item = ui_image_menu_item_new(GTK_STOCK_GO_BACK, _("Collapse all"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_collapse_all), NULL);

	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_check_menu_item_new_with_mnemonic(_("Show hidden files"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), CONFIG_SHOW_HIDDEN_FILES);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_show_hidden_files), NULL);

	item = gtk_check_menu_item_new_with_mnemonic(_("Show toolbars"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), CONFIG_SHOW_BARS ? TRUE : FALSE);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_show_bars), NULL);

	gtk_widget_show_all(menu);

	return menu;
}


/* ------------------
 * TOOLBAR`S EVENTS
 * ------------------ */

static void
on_button_refresh(void)
{
	treebrowser_chroot(addressbar_last_address);
}

static void on_button_refresh_svn ( void )
{
}

/* ------------------
 * TREEVIEW EVENTS
 * ------------------ */

static gboolean
on_treeview_mouseclick(GtkWidget *widget, GdkEventButton *event, GtkTreeSelection *selection)
{
	if (event->button == 3)
	{
		GtkTreeIter 	iter;
		GtkTreeModel 	*model;
		GtkTreePath *path;
		GtkWidget *menu;
		gchar *name = NULL, *uri = NULL, *clean_name = NULL, *all_uri = NULL;
		GList *list;
		GList *node;

		/* Get tree path for row that was clicked */
		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview),
																		 (gint) event->x,
																		 (gint) event->y,
																		 &path, NULL, NULL, NULL))
		{
			/* Unselect current selection; unless CTRL */
			if ( !(event->state & GDK_CONTROL_MASK) )
			{
				// Control is not down but this was already selected.
				if ( !gtk_tree_selection_path_is_selected ( selection, path ) )
					gtk_tree_selection_unselect_all(selection);
			}
			gtk_tree_selection_select_path(selection, path);
			gtk_tree_path_free(path);
		}

		int sel_count = 0;
		list = gtk_tree_selection_get_selected_rows ( selection, &model );
		for ( node = list;node;node = node->next )
		{
			sel_count++;
			gtk_tree_model_get_iter ( model, &iter, node->data );

			gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
								TREEBROWSER_COLUMN_NAME, &name,
								TREEBROWSER_COLUMN_NAME_CLEAN, &clean_name,
								TREEBROWSER_COLUMN_URI, &uri,
								-1);

			if ( all_uri == NULL )
				all_uri = g_strdup ( uri );
			else
			{
				gchar *tmp = g_strjoin ( ", ", all_uri, uri, NULL );
				g_free ( all_uri );
				all_uri = tmp;
			}
		}

		/*
		if (gtk_tree_selection_get_selected(selection, &model, &iter))
			gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
								TREEBROWSER_COLUMN_NAME, &name,
								TREEBROWSER_COLUMN_URI, &uri,
								-1);
		*/

		menu = create_popup_menu ( name != NULL ? name : "", all_uri != NULL ? all_uri : "", sel_count );
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);

		g_free ( all_uri );
		g_free(name);
		g_free(uri);
		return TRUE;
	}

	return FALSE;
}

static gboolean
on_treeview_keypress(GtkWidget *widget, GdkEventKey *event)
{
	if (event->keyval == GDK_BackSpace)
	{
		return TRUE;
	}

	return FALSE;
}

static void on_treeview_changed(GtkWidget *widget, gpointer user_data)
{
	g_print ( "Changed" );
	/*
	GtkTreeIter 	iter;
	GtkTreeModel 	*model;
	gchar 			*uri;

	if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(widget), &model, &iter))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
							TREEBROWSER_COLUMN_URI, &uri,
							-1);
		if (uri == NULL)
			return;

		if (g_file_test(uri, G_FILE_TEST_EXISTS)) {
			if (!g_file_test(uri, G_FILE_TEST_IS_DIR) && CONFIG_ONE_CLICK_CHDOC)
				document_open_file(uri, FALSE, NULL, NULL);
		} else
			gtk_tree_store_iter_clear_nodes(&iter, TRUE);

		g_free(uri);
	}
	*/
}

static void
on_treeview_row_activated(GtkWidget *widget, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	GtkTreeIter 	iter;
	gchar 			*uri;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(treestore), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter, TREEBROWSER_COLUMN_URI, &uri, -1);

	if (uri == NULL)
		return;

	if (g_file_test (uri, G_FILE_TEST_IS_DIR))
		if (CONFIG_CHROOT_ON_DCLICK)
			treebrowser_chroot(uri);
		else
			if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
				gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), path);
			else {
				treebrowser_browse2(uri, &iter);
				gtk_tree_view_expand_row(GTK_TREE_VIEW(widget), path, FALSE);
			}
	else {
		document_open_file(uri, FALSE, NULL, NULL);
		if (CONFIG_ON_OPEN_FOCUS_EDITOR)
			keybindings_send_command(GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_EDITOR);
	}

	g_free(uri);
}

static void
on_treeview_row_expanded(GtkWidget *widget, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data)
{
	gchar *uri;

	gtk_tree_model_get(GTK_TREE_MODEL(treestore), iter, TREEBROWSER_COLUMN_URI, &uri, -1);
	if (uri == NULL)
		return;

	if (flag_on_expand_refresh == FALSE)
	{
		flag_on_expand_refresh = TRUE;
		treebrowser_browse2(uri, iter);
		gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), path, FALSE);
		flag_on_expand_refresh = FALSE;
	}
	if (CONFIG_SHOW_ICONS)
	{
		GdkPixbuf *icon = get_pixbuf ( uri, "expanded", 0 );
		gtk_tree_store_set(treestore, iter, TREEBROWSER_COLUMN_ICON, icon, -1);
		if ( icon )
			g_object_unref(icon);
	}

	g_free(uri);
}

static void
on_treeview_row_collapsed(GtkWidget *widget, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data)
{
	gchar *uri;
	gtk_tree_model_get(GTK_TREE_MODEL(treestore), iter, TREEBROWSER_COLUMN_URI, &uri, -1);
	if (uri == NULL)
		return;
	if (CONFIG_SHOW_ICONS)
	{
		GdkPixbuf *icon = get_pixbuf ( uri, "collapsed", 0 );
		gtk_tree_store_set(treestore, iter, TREEBROWSER_COLUMN_ICON, icon, -1);
		if ( icon )
			g_object_unref(icon);
	}
	g_free(uri);
}

static void
on_treeview_renamed(GtkCellRenderer *renderer, const gchar *path_string, const gchar *name_new, gpointer user_data)
{

	GtkTreeViewColumn 	*column;
	GList 				*renderers;
	GtkTreeIter 		iter, iter_parent;
	gchar 				*uri, *uri_new, *dirname;

	column 		= gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 0);
	renderers 	= _gtk_cell_layout_get_cells(column);
	renderer 	= g_list_nth_data(renderers, TREEBROWSER_RENDER_TEXT);

	g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL);

	if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(treestore), &iter, path_string))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
		if (uri)
		{
			dirname = g_path_get_dirname(uri);
			uri_new = g_strconcat(dirname, G_DIR_SEPARATOR_S, name_new, NULL);
			g_free(dirname);
			if (!(g_file_test(uri_new, G_FILE_TEST_EXISTS) &&
				strcmp(uri, uri_new) != 0 &&
				!dialogs_show_question(_("Target file '%s' exists, do you really want to replace it?"), uri_new)))
			{
				if (g_rename(uri, uri_new) == 0)
				{
					dirname = g_path_get_dirname(uri_new);
					gtk_tree_store_set(treestore, &iter,
									TREEBROWSER_COLUMN_NAME, name_new,
									TREEBROWSER_COLUMN_URI, uri_new,
									-1);
					if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(treestore), &iter_parent, &iter))
						treebrowser_browse2(dirname, &iter_parent);
					else
						treebrowser_browse2(dirname, NULL);
					g_free(dirname);

					if (!g_file_test(uri, G_FILE_TEST_IS_DIR))
					{
						GeanyDocument *doc = document_find_by_filename(uri);
						if (doc && document_close(doc))
							document_open_file(uri_new, FALSE, NULL, NULL);
					}
				}
			}
			g_free(uri_new);
			g_free(uri);
		}
	}
}

static void
treebrowser_track_current_cb(void)
{
	if (CONFIG_FOLLOW_CURRENT_DOC)
		treebrowser_track_current();
}


/* ------------------
 * TREEBROWSER INITIAL FUNCTIONS
 * ------------------ */

static gboolean
treeview_separator_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gint flag;
	gtk_tree_model_get(model, iter, TREEBROWSER_COLUMN_FLAG, &flag, -1);
	return (flag == TREEBROWSER_FLAGS_SEPARATOR);
}

static GtkWidget*
create_view_and_model(void)
{

	GtkWidget 			*view;

	view 					= gtk_tree_view_new();
	treeview_column_text	= gtk_tree_view_column_new();
	render_icon 			= gtk_cell_renderer_pixbuf_new();
	render_text 			= gtk_cell_renderer_text_new();

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), treeview_column_text);

	gtk_tree_view_column_pack_start(treeview_column_text, render_icon, FALSE);
	gtk_tree_view_column_set_attributes(treeview_column_text, render_icon, "pixbuf", TREEBROWSER_RENDER_ICON, NULL);

	gtk_tree_view_column_pack_start(treeview_column_text, render_text, TRUE);
	gtk_tree_view_column_add_attribute(treeview_column_text, render_text, "text", TREEBROWSER_RENDER_TEXT);

	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(view), TREEBROWSER_COLUMN_NAME);

	gtk_tree_view_set_row_separator_func(GTK_TREE_VIEW(view), treeview_separator_func, NULL, NULL);

	ui_widget_modify_font_from_string(view, geany->interface_prefs->tagbar_font);

#if GTK_CHECK_VERSION(2, 10, 0)
	g_object_set(view, "has-tooltip", TRUE, "tooltip-column", TREEBROWSER_COLUMN_URI, NULL);
#endif

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), GTK_SELECTION_MULTIPLE);

#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(view), CONFIG_SHOW_TREE_LINES);
#endif

	treestore = gtk_tree_store_new(TREEBROWSER_COLUMNC, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING);

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(treestore));
	g_signal_connect(G_OBJECT(render_text), "edited", G_CALLBACK(on_treeview_renamed), view);

	return view;
}

static void
create_sidebar(void)
{
	GtkWidget 			*scrollwin;
	GtkWidget 			*toolbar;
	GtkWidget 			*wid;
	GtkTreeSelection 	*selection;

	treeview 				= create_view_and_model();
	sidebar_vbox 			= gtk_vbox_new(FALSE, 0);
	sidebar_vbox_bars 		= gtk_vbox_new(FALSE, 0);
	selection 				= gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	scrollwin 				= gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_MENU);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH));
	gtk_widget_set_tooltip_text(wid, _("Refresh"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_refresh), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_DISCONNECT));
	gtk_widget_set_tooltip_text(wid, _("Refresh Versioning"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_refresh_svn), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

	gtk_container_add(GTK_CONTAINER(scrollwin), 			treeview);
	gtk_box_pack_start(GTK_BOX(sidebar_vbox_bars), 			toolbar, 			FALSE, TRUE,  1);

	if (CONFIG_SHOW_BARS == 2)
	{
		gtk_box_pack_start(GTK_BOX(sidebar_vbox), 				scrollwin, 			TRUE,  TRUE,  1);
		gtk_box_pack_start(GTK_BOX(sidebar_vbox), 				sidebar_vbox_bars, 	FALSE, TRUE,  1);
	}
	else
	{
		gtk_box_pack_start(GTK_BOX(sidebar_vbox), 				sidebar_vbox_bars, 	FALSE, TRUE,  1);
		gtk_box_pack_start(GTK_BOX(sidebar_vbox), 				scrollwin, 			TRUE,  TRUE,  1);
	}

	g_signal_connect(selection, 		"changed", 				G_CALLBACK(on_treeview_changed), 				NULL);
	g_signal_connect(treeview, 			"button-press-event", 	G_CALLBACK(on_treeview_mouseclick), 			selection);
	g_signal_connect(treeview, 			"row-activated", 		G_CALLBACK(on_treeview_row_activated), 			NULL);
	g_signal_connect(treeview, 			"row-collapsed", 		G_CALLBACK(on_treeview_row_collapsed), 			NULL);
	g_signal_connect(treeview, 			"row-expanded", 		G_CALLBACK(on_treeview_row_expanded), 			NULL);
	g_signal_connect(treeview, 			"key-press-event", 		G_CALLBACK(on_treeview_keypress), 			NULL);

	gtk_widget_show_all(sidebar_vbox);

	page_number = gtk_notebook_append_page(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
							sidebar_vbox, gtk_label_new(_("Tree Browser")));

	showbars(CONFIG_SHOW_BARS);
}


/* ------------------
 * CONFIG DIALOG
 * ------------------ */

static struct
{
	GtkWidget *OPEN_EXTERNAL_CMD;
	GtkWidget *OPEN_TERMINAL;
	GtkWidget *ONE_CLICK_CHDOC;
	GtkWidget *SHOW_HIDDEN_FILES;
	GtkWidget *HIDE_OBJECT_FILES;
	GtkWidget *SHOW_BARS;
	GtkWidget *CHROOT_ON_DCLICK;
	GtkWidget *FOLLOW_CURRENT_DOC;
	GtkWidget *ON_DELETE_CLOSE_FILE;
	GtkWidget *ON_OPEN_FOCUS_EDITOR;
	GtkWidget *SHOW_TREE_LINES;
	GtkWidget *SHOW_ICONS;
	GtkWidget *OPEN_NEW_FILES;
} configure_widgets;

static void
load_settings(void)
{
	GKeyFile *config 	= g_key_file_new();

	g_key_file_load_from_file(config, CONFIG_FILE, G_KEY_FILE_NONE, NULL);

	CONFIG_OPEN_EXTERNAL_CMD 		=  utils_get_setting_string(config, "treebrowser", "open_external_cmd", 	CONFIG_OPEN_EXTERNAL_CMD_DEFAULT);
	CONFIG_OPEN_TERMINAL 				= utils_get_setting_string(config, "treebrowser", "open_terminal", CONFIG_OPEN_TERMINAL_DEFAULT);
	CONFIG_ONE_CLICK_CHDOC 			= utils_get_setting_boolean(config, "treebrowser", "one_click_chdoc", 		CONFIG_ONE_CLICK_CHDOC);
	CONFIG_SHOW_HIDDEN_FILES 		= utils_get_setting_boolean(config, "treebrowser", "show_hidden_files", 	CONFIG_SHOW_HIDDEN_FILES);
	CONFIG_HIDE_OBJECT_FILES 		= utils_get_setting_boolean(config, "treebrowser", "hide_object_files", 	CONFIG_HIDE_OBJECT_FILES);
	CONFIG_SHOW_BARS 				= utils_get_setting_integer(config, "treebrowser", "show_bars", 			CONFIG_SHOW_BARS);
	CONFIG_CHROOT_ON_DCLICK 		= utils_get_setting_boolean(config, "treebrowser", "chroot_on_dclick", 		CONFIG_CHROOT_ON_DCLICK);
	CONFIG_FOLLOW_CURRENT_DOC 		= utils_get_setting_boolean(config, "treebrowser", "follow_current_doc", 	CONFIG_FOLLOW_CURRENT_DOC);
	CONFIG_ON_DELETE_CLOSE_FILE 	= utils_get_setting_boolean(config, "treebrowser", "on_delete_close_file", 	CONFIG_ON_DELETE_CLOSE_FILE);
	CONFIG_ON_OPEN_FOCUS_EDITOR		= utils_get_setting_boolean(config, "treebrowser", "on_open_focus_editor", 	CONFIG_ON_OPEN_FOCUS_EDITOR);
	CONFIG_SHOW_TREE_LINES 			= utils_get_setting_boolean(config, "treebrowser", "show_tree_lines", 		CONFIG_SHOW_TREE_LINES);
	CONFIG_SHOW_ICONS 				= utils_get_setting_integer(config, "treebrowser", "show_icons", 			CONFIG_SHOW_ICONS);
	CONFIG_OPEN_NEW_FILES			= utils_get_setting_boolean(config, "treebrowser", "open_new_files",		CONFIG_OPEN_NEW_FILES);

	g_key_file_free(config);
}

static gboolean
save_settings(void)
{
	GKeyFile 	*config 		= g_key_file_new();
	gchar 		*config_dir 	= g_path_get_dirname(CONFIG_FILE);
	gchar 		*data;

	g_key_file_load_from_file(config, CONFIG_FILE, G_KEY_FILE_NONE, NULL);
	if (! g_file_test(config_dir, G_FILE_TEST_IS_DIR) && utils_mkdir(config_dir, TRUE) != 0)
	{
		g_free(config_dir);
		g_key_file_free(config);
		return FALSE;
	}

	g_key_file_set_string(config, 	"treebrowser", "open_external_cmd", 	CONFIG_OPEN_EXTERNAL_CMD);
	g_key_file_set_string(config, 	"treebrowser", "open_terminal", 	CONFIG_OPEN_TERMINAL);
	g_key_file_set_boolean(config, 	"treebrowser", "one_click_chdoc", 		CONFIG_ONE_CLICK_CHDOC);
	g_key_file_set_boolean(config, 	"treebrowser", "show_hidden_files", 	CONFIG_SHOW_HIDDEN_FILES);
	g_key_file_set_boolean(config, 	"treebrowser", "hide_object_files", 	CONFIG_HIDE_OBJECT_FILES);
	g_key_file_set_integer(config, 	"treebrowser", "show_bars", 			CONFIG_SHOW_BARS);
	g_key_file_set_boolean(config, 	"treebrowser", "chroot_on_dclick", 		CONFIG_CHROOT_ON_DCLICK);
	g_key_file_set_boolean(config, 	"treebrowser", "follow_current_doc", 	CONFIG_FOLLOW_CURRENT_DOC);
	g_key_file_set_boolean(config, 	"treebrowser", "on_delete_close_file", 	CONFIG_ON_DELETE_CLOSE_FILE);
	g_key_file_set_boolean(config, 	"treebrowser", "on_open_focus_editor", 	CONFIG_ON_OPEN_FOCUS_EDITOR);
	g_key_file_set_boolean(config, 	"treebrowser", "show_tree_lines", 		CONFIG_SHOW_TREE_LINES);
	g_key_file_set_integer(config, 	"treebrowser", "show_icons", 			CONFIG_SHOW_ICONS);
	g_key_file_set_boolean(config,	"treebrowser", "open_new_file",			CONFIG_OPEN_NEW_FILES);

	data = g_key_file_to_data(config, NULL, NULL);
	utils_write_file(CONFIG_FILE, data);
	g_free(data);

	g_free(config_dir);
	g_key_file_free(config);

	return TRUE;
}

static void
on_configure_response(GtkDialog *dialog, gint response, gpointer user_data)
{

	if (! (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY))
		return;

	CONFIG_OPEN_EXTERNAL_CMD 	= gtk_editable_get_chars(GTK_EDITABLE(configure_widgets.OPEN_EXTERNAL_CMD), 0, -1);
	CONFIG_OPEN_TERMINAL     	= gtk_editable_get_chars(GTK_EDITABLE(configure_widgets.OPEN_TERMINAL), 0, -1);
	CONFIG_ONE_CLICK_CHDOC 		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.ONE_CLICK_CHDOC));
	CONFIG_SHOW_HIDDEN_FILES 	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_HIDDEN_FILES));
	CONFIG_HIDE_OBJECT_FILES 	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.HIDE_OBJECT_FILES));
	CONFIG_SHOW_BARS 			= gtk_combo_box_get_active(GTK_COMBO_BOX(configure_widgets.SHOW_BARS));
	CONFIG_CHROOT_ON_DCLICK 	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.CHROOT_ON_DCLICK));
	CONFIG_FOLLOW_CURRENT_DOC 	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.FOLLOW_CURRENT_DOC));
	CONFIG_ON_DELETE_CLOSE_FILE = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.ON_DELETE_CLOSE_FILE));
	CONFIG_ON_OPEN_FOCUS_EDITOR	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.ON_OPEN_FOCUS_EDITOR));
	CONFIG_SHOW_TREE_LINES 		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_TREE_LINES));
	CONFIG_SHOW_ICONS 			= gtk_combo_box_get_active(GTK_COMBO_BOX(configure_widgets.SHOW_ICONS));
	CONFIG_OPEN_NEW_FILES		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.OPEN_NEW_FILES));

	if (save_settings() == TRUE)
	{
#if GTK_CHECK_VERSION(2, 10, 0)
		gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(treeview), CONFIG_SHOW_TREE_LINES);
#endif
		treebrowser_chroot(addressbar_last_address);
		showbars(CONFIG_SHOW_BARS);
	}
	else
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
			_("Plugin configuration directory could not be created."));
}

GtkWidget*
plugin_configure(GtkDialog *dialog)
{
	GtkWidget 		*label;
	GtkWidget 		*vbox, *hbox;

	vbox 	= gtk_vbox_new(FALSE, 0);
	hbox 	= gtk_hbox_new(FALSE, 0);

	label 	= gtk_label_new(_("External open command"));
	configure_widgets.OPEN_EXTERNAL_CMD = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(configure_widgets.OPEN_EXTERNAL_CMD), CONFIG_OPEN_EXTERNAL_CMD);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_widget_set_tooltip_text(configure_widgets.OPEN_EXTERNAL_CMD,
		_("The command to execute when using \"Open with\". You can use %f and %d wildcards.\n"
		  "%f will be replaced with the filename including full path\n"
		  "%d will be replaced with the path name of the selected file without the filename"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), configure_widgets.OPEN_EXTERNAL_CMD, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Terminal"));
	configure_widgets.OPEN_TERMINAL = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(configure_widgets.OPEN_TERMINAL), CONFIG_OPEN_TERMINAL);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_widget_set_tooltip_text(configure_widgets.OPEN_TERMINAL,
		_("The terminal to use with the command \"Open Terminal\""));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), configure_widgets.OPEN_TERMINAL, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Toolbar"));
	configure_widgets.SHOW_BARS = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(configure_widgets.SHOW_BARS), _("Hidden"));
	gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(configure_widgets.SHOW_BARS), _("Top"));
	gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(configure_widgets.SHOW_BARS), _("Bottom"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), configure_widgets.SHOW_BARS, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);
	gtk_widget_set_tooltip_text(configure_widgets.SHOW_BARS,
		_("If position is changed, the option require plugin restart."));
	gtk_combo_box_set_active(GTK_COMBO_BOX(configure_widgets.SHOW_BARS), CONFIG_SHOW_BARS);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Show icons"));
	configure_widgets.SHOW_ICONS = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(configure_widgets.SHOW_ICONS), _("None"));
	gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(configure_widgets.SHOW_ICONS), _("Base"));
	gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(configure_widgets.SHOW_ICONS), _("Content-type"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), configure_widgets.SHOW_ICONS, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);
	gtk_combo_box_set_active(GTK_COMBO_BOX(configure_widgets.SHOW_ICONS), CONFIG_SHOW_ICONS);

	configure_widgets.SHOW_HIDDEN_FILES = gtk_check_button_new_with_label(_("Show hidden files"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.SHOW_HIDDEN_FILES), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_HIDDEN_FILES), CONFIG_SHOW_HIDDEN_FILES);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.SHOW_HIDDEN_FILES, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(configure_widgets.SHOW_HIDDEN_FILES,
		_("On Windows, this just hide files that are prefixed with '.' (dot)"));

	configure_widgets.HIDE_OBJECT_FILES = gtk_check_button_new_with_label(_("Hide object files"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.HIDE_OBJECT_FILES), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.HIDE_OBJECT_FILES), CONFIG_HIDE_OBJECT_FILES);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.HIDE_OBJECT_FILES, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(configure_widgets.HIDE_OBJECT_FILES,
		_("Don't show generated object files in the file browser, this includes *.o, *.obj. *.so, *.dll, *.a, *.lib"));

	configure_widgets.FOLLOW_CURRENT_DOC = gtk_check_button_new_with_label(_("Follow current document"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.FOLLOW_CURRENT_DOC), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.FOLLOW_CURRENT_DOC), CONFIG_FOLLOW_CURRENT_DOC);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.FOLLOW_CURRENT_DOC, FALSE, FALSE, 0);

	configure_widgets.ONE_CLICK_CHDOC = gtk_check_button_new_with_label(_("Single click, open document and focus it"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.ONE_CLICK_CHDOC), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.ONE_CLICK_CHDOC), CONFIG_ONE_CLICK_CHDOC);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.ONE_CLICK_CHDOC, FALSE, FALSE, 0);

	configure_widgets.CHROOT_ON_DCLICK = gtk_check_button_new_with_label(_("Double click open directory"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.CHROOT_ON_DCLICK), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.CHROOT_ON_DCLICK), CONFIG_CHROOT_ON_DCLICK);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.CHROOT_ON_DCLICK, FALSE, FALSE, 0);

	configure_widgets.ON_DELETE_CLOSE_FILE = gtk_check_button_new_with_label(_("On delete file, close it if is opened"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.ON_DELETE_CLOSE_FILE), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.ON_DELETE_CLOSE_FILE), CONFIG_ON_DELETE_CLOSE_FILE);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.ON_DELETE_CLOSE_FILE, FALSE, FALSE, 0);

	configure_widgets.ON_OPEN_FOCUS_EDITOR = gtk_check_button_new_with_label(_("Focus editor on file open"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.ON_OPEN_FOCUS_EDITOR), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.ON_OPEN_FOCUS_EDITOR), CONFIG_ON_OPEN_FOCUS_EDITOR);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.ON_OPEN_FOCUS_EDITOR, FALSE, FALSE, 0);

	configure_widgets.SHOW_TREE_LINES = gtk_check_button_new_with_label(_("Show tree lines"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.SHOW_TREE_LINES), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_TREE_LINES), CONFIG_SHOW_TREE_LINES);
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.SHOW_TREE_LINES, FALSE, FALSE, 0);
#endif

	configure_widgets.OPEN_NEW_FILES = gtk_check_button_new_with_label(_("Open new files"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.OPEN_NEW_FILES ), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.OPEN_NEW_FILES ), CONFIG_OPEN_NEW_FILES);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.OPEN_NEW_FILES , FALSE, FALSE, 0);

	gtk_widget_show_all(vbox);

	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);

	return vbox;
}


/* ------------------
 * GEANY HOOKS
 * ------------------ */

static void kb_activate(guint key_id)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook), page_number);
	switch (key_id)
	{
		case KB_FOCUS_FILE_LIST:
			gtk_widget_grab_focus(treeview);
			break;

		case KB_RENAME_OBJECT:
			treebrowser_rename_current();
			break;

		case KB_REFRESH:
			on_menu_refresh(NULL, NULL);
			break;
	}
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
	inotify_add_watch( fd, "/home/harmanci/projects", WATCH_FLAGS );
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

void plugin_init(GeanyData *data)
{
	GeanyKeyGroup *key_group;

	svn_cache = g_hash_table_new ( g_str_hash, g_str_equal );
	svn_cache_pending = g_hash_table_new ( g_str_hash, g_str_equal );

	CONFIG_FILE = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
		"treebrowser", G_DIR_SEPARATOR_S, "treebrowser.conf", NULL);

	flag_on_expand_refresh = FALSE;

	load_settings();
	create_sidebar();
	treebrowser_chroot(get_default_dir());

	/* setup keybindings */
	key_group = plugin_set_key_group(geany_plugin, "file_browser", KB_COUNT, NULL);

	keybindings_set_item(key_group, KB_FOCUS_FILE_LIST, kb_activate,
		0, 0, "focus_file_list", _("Focus File List"), NULL);
	keybindings_set_item(key_group, KB_FOCUS_PATH_ENTRY, kb_activate,
		0, 0, "focus_path_entry", _("Focus Path Entry"), NULL);
	keybindings_set_item(key_group, KB_RENAME_OBJECT, kb_activate,
		0, 0, "rename_object", _("Rename Object"), NULL);
	keybindings_set_item(key_group, KB_CREATE_FILE, kb_activate,
		0, 0, "create_file", _("Create New File"), NULL);
	keybindings_set_item(key_group, KB_CREATE_DIR, kb_activate,
		0, 0, "create_dir", _("Create New Directory"), NULL);
	keybindings_set_item(key_group, KB_REFRESH, kb_activate,
		0, 0, "rename_refresh", _("Refresh"), NULL);

	plugin_signal_connect(geany_plugin, NULL, "document-activate", TRUE,
		(GCallback)&treebrowser_track_current_cb, NULL);

	pthread_t svn_watcher;
	pthread_create ( &svn_watcher, NULL, svncache_refresh_function, NULL );

	pthread_t inotify_watcher;
	pthread_create ( &inotify_watcher, NULL, inotify_watcher_function, NULL );

	gdk_threads_add_timeout ( 100, decorator_refresh_function, NULL );

	// setup inotify at the root
}

void
plugin_cleanup(void)
{
	g_free(addressbar_last_address);
	g_free(CONFIG_FILE);
	g_free(CONFIG_OPEN_EXTERNAL_CMD);
	g_free(CONFIG_OPEN_TERMINAL);
	gtk_widget_destroy(sidebar_vbox);
}


