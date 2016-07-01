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

extern const gchar backupcopy_backup_dir[];
extern const gchar project_dir[];

static gchar *backup_create_dir_parts(const gchar *filename)
{
	gchar *parts = NULL;
	gchar *dirname = NULL;

	dirname = g_path_get_dirname(filename);
	if ( !dirname )
		return g_strdup ( "" );

	// if the file is in the project space already, skip the 
	if ( strncmp ( dirname, project_dir, strlen(project_dir) ) != 0 )
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

void backup_document_save_cb(GObject *obj, GeanyDocument *doc, gpointer user_data)
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

