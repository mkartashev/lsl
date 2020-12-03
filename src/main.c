#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>

#include "list.h"

static void		init_globals(void);
static bool		parse_args(int arg, char* argv[]);
static void		usage(void);

static void		error_args(const char *msg, ...);
static void		error_err_args(const char *msg, ...);

static bool		fill_entry(char * fname, entry_s * data);
static void		print_file_entry(const entry_s * data);
static void		print_dir_entry(const entry_s * data);

static list_t		list_upper_bound_name(list_t list, const char * fname);
static void 		list_insert_sorted(list_t* list, list_t new_item);

/**
 * Returns true if the command line argument given is an option.
 */
static inline bool	is_option(const char* opt)
{
	return (opt[0] == '-');
}

int	main(int argc, char *argv[])
{
	init_globals();

	if (! parse_args(argc, argv) )
	{
		return 2;	// to be compatible with ls(1)
	}

	// Collect all file's data in a list; we need to know everything about
	// them in advance in order to be able to format the output well.
	list_t files = NULL;
	bool has_files = false;	// were there any files/directories specified at all?
	for(int i = 1; i < argc; i++)
	{
		if (!is_option(argv[i]))
		{
			has_files = true;

			list_t next_file = list_create();
			// Files or directories that we weren't able to read are simply skipped.
			char * fname = argv[i];
			if (fill_entry(fname, &next_file->data))
			{
				list_insert_sorted(&files, next_file);
			}
			else
			{
				list_delete(next_file);
			}
		}
	}

	// By default, show the contents on the current directory.
	if (!has_files)
	{
		assert(!files);
		files = list_create();
		if (!fill_entry(".", &files->data))
		{
			list_delete(files);
			files = NULL;
		}
	}

	// First output all the files.
	FOREACH(f, files)
	{
		if (!S_ISDIR(f->data.mode))
		{
			print_file_entry(&f->data);
		}
	}
	
	// Then print directories and their files; prefix with the directory
	// name if there's a need to disambiguate the file list
	const bool show_prefix = (files && files->next);
	FOREACH(f, files)
	{
		if (S_ISDIR(f->data.mode))
		{
			if (show_prefix)
			{
				printf("%s:\n", f->data.fname);
			}

			print_dir_entry(&f->data);
		}
	}

	// Note: there's no point in freeing malloc'ed memory here since it will surely not be re-used
	// for anything.

	return EXIT_SUCCESS;
}

/**
 * A struct to group together info about the maximum number of characters needed for various listing columns.
 */
struct	globals_s
{
	size_t	max_nlinks_width;	// maximum number of characters needed for the number of file links
	size_t	max_size_width;		// maximum number of characters needed for the file size
	size_t	max_uid_width;		// maximum number of characters needed for the user id (text or digits)
	size_t	max_gid_width;		// maximum number of characters needed for the group id (text or digits)
}		g_globals = { 0 };

bool	g_test_run;				// when true, makes output more deterministic for testing purposes
bool 	g_blocksize_512;		// should blocksize be reported in 512b blocks (instead of 1024)

/**
 * Returns the number of digits in the given number.
 */
static size_t	count_digits(unsigned long l)
{
	if ( l < 10 )
	{
		return 1;
	}

	size_t n = 0;
	while (l != 0)
	{
		l /= 10;
		n++;
	}

	return n;
}

/**
 * Updates the global "maximum columns width" with the new data according to the
 * given argument.
 */
static void	update_max_widths(const entry_s * data)
{
	size_t nlinks_width = count_digits(data->nlink);
	if (nlinks_width > g_globals.max_nlinks_width)
	{
		g_globals.max_nlinks_width = nlinks_width;
	}

	size_t size_width = count_digits((unsigned long)data->size);
	if (size_width > g_globals.max_size_width)
	{
		g_globals.max_size_width = size_width;
	}

	size_t uid_width = data->uid_str ? strlen(data->uid_str) : count_digits(data->uid);
	if (uid_width > g_globals.max_uid_width)
	{
		g_globals.max_uid_width = uid_width;
	}

	size_t gid_width = data->gid_str ? strlen(data->gid_str) : count_digits(data->gid);
	if (gid_width > g_globals.max_gid_width)
	{
		g_globals.max_gid_width = gid_width;
	}
}

/**
 * Fills in 'data' with the information about the file 'fname'.
 */
static void	fill_file_entry(char * fname, char * full_fname, entry_s * data, const struct stat * sb, bool toplevel)
{
	data->fname  = fname;
	data->mode   = sb->st_mode;
	data->nlink  = sb->st_nlink;
	data->uid    = sb->st_uid;
	data->gid    = sb->st_gid;
	data->size   = sb->st_size;
	data->blocks = sb->st_blocks;
	data->mtime  = sb->st_mtime;

	if (g_test_run)
	{	
		// Those will differ between hosts, blank them out for a testrun.
		data->uid = 42;
		data->gid = 42;
		data->uid_str = "tester";
		data->gid_str = "testgrp";
	}
	else
	{
		struct group *  grp = getgrgid(sb->st_gid);
		struct passwd * pswd = getpwuid(sb->st_uid);
		if (pswd)
		{
			data->uid_str = strdup(pswd->pw_name);
			assert(data->uid_str);
		}
		if (grp)
		{
			data->gid_str = strdup(grp->gr_name);
			assert(data->gid_str);
		}
	}

	if (!toplevel || !S_ISDIR(sb->st_mode))
	{
		// We will not output this information for top-level directories, so they need not
		// participate in calculating the maximums.
		update_max_widths(data);
	}

	if (S_ISLNK(sb->st_mode))
	{
		size_t bufsz = (size_t)(sb->st_size + 1);
		if (bufsz == 0)
		{
			bufsz = PATH_MAX+1;
		}
		char * target_name = malloc(bufsz);
		assert(target_name);

		ssize_t nbytes = readlink(full_fname, target_name, bufsz);
		if (nbytes == -1)
		{
			error_err_args("cannot read link '%s'", fname);
			free(target_name);
			target_name = NULL;
		}
		else if ((size_t)nbytes == bufsz)
		{
			target_name[bufsz-1] = 0;	// the name may have been truncated, if changed during the readlink() call
		}
		else
		{
			target_name[nbytes] = 0;
		}
		data->link_tgt = target_name;
	}
}

/**
 * Returns true if the file with the name given should be shown in the directory listing.
 */
static inline bool	should_show_file(const char* fname)
{
	return fname[0] != '.';
}

/**
 * Finds and returns the position in the sorted list where the new entry with the given
 * name should be inserted.
 */
static list_t	list_upper_bound_name(list_t list, const char * fname)
{
	list_t last = NULL;

	for(list_t l = list; l; l = l->next)
	{
		if ( strcasecmp(l->data.fname, fname) > 0 )
		{
			break;
		}
		last = l;
	}

	return last;
}

/**
 * Adds a new item to the files list, keeping it sorted by the name of the file.
 */
static void		list_insert_sorted(list_t* list, list_t new_item)
{
	const char * fname = new_item->data.fname;

	list_t insert_point = list_upper_bound_name(*list, fname);
	if (insert_point)
	{
		list_insert(insert_point, new_item);
	}
	else
	{
		*list = list_add(*list, new_item);
	}
}

/**
 * Fills in 'data' with the information about the directory named 'dname'.
 */
static bool		fill_dir_entry(const char * dname, entry_s * data)
{
	blksize_t blksize = 0;	// number of blocks occupied by the contents of this directory

	DIR * dir = opendir(dname);
	if (!dir)
	{
		error_err_args("cannot access '%s'", dname);
		return false;
	}

	errno = 0;	// resetting errno is necessary for readdir()

	while (true)
	{
		struct dirent * f = readdir(dir);
		if (!f && errno != 0)
		{
			error_err_args("cannot read directory '%s'", dname);
			closedir(dir);
			return false;
		}
		else if (!f)
		{
			break;	// Done, no error
		}

		if (!should_show_file(f->d_name))
		{
			// Note: we do not include the skipped files into blksize.
			continue;
		}

		size_t len = strlen(dname) + strlen(f->d_name) + 2;
		char * full_fname = malloc(len);
		assert(full_fname);
		strncpy(full_fname, dname, len);
		strncat(full_fname, "/", len);
		strncat(full_fname, f->d_name, len);
		
		struct stat	sb_f;
		if (lstat(full_fname, &sb_f) == -1)
		{
			error_err_args("cannot access '%s'", f->d_name);
			free(full_fname);
			continue;
		}

		blksize += sb_f.st_blocks;

		list_t next_file = list_create();
		char * fname = strdup(f->d_name);
		assert(fname);

		const bool top_level = false;
		fill_file_entry(fname, full_fname, &next_file->data, &sb_f, top_level);
		list_insert_sorted(&data->files, next_file);
	}

	data->blocks = blksize;

	return true;
}

/**
 * Fills in 'data' with the information about the file or directory 'fname'.
 */
static bool	fill_entry(char * fname, entry_s * data)
{
	struct stat	sb;

	if ((lstat(fname, &sb) == -1))
	{
		error_err_args("cannot access '%s'", fname);
		return false;
	}

	const bool	is_dir	= ((sb.st_mode & S_IFMT) == S_IFDIR);

	fill_file_entry(fname, fname, data, &sb, true);
	bool rc = true;
	if (is_dir)
	{
		rc = fill_dir_entry(fname, data);
	}

	return rc;
}

/**
 * Prints out file mode.
 */
static void	print_mode(mode_t mode)
{
	char first = '-';
	if (S_ISCHR(mode))
	{
		first = 'c';
	}
	else if (S_ISDIR(mode))
	{
		first = 'd';
	}
	else if (S_ISBLK(mode))
	{
		first = 'b';
	}
	else if (S_ISFIFO(mode))
	{
		first = 'p';
	}
	else if (S_ISLNK(mode))
	{
		first = 'l';
	}
	else if (S_ISSOCK(mode))
	{
		first = 's';
	}

	char owner_r = (mode & S_IRUSR) ? 'r' : '-';
	char owner_w = (mode & S_IWUSR) ? 'w' : '-';
	char owner_x = (mode & S_IXUSR) ? 'x' : '-';

	char grp_r = (mode & S_IRGRP) ? 'r' : '-';
	char grp_w = (mode & S_IWGRP) ? 'w' : '-';
	char grp_x = (mode & S_IXGRP) ? 'x' : '-';

	char oth_r = (mode & S_IROTH) ? 'r' : '-';
	char oth_w = (mode & S_IWOTH) ? 'w' : '-';
	char oth_x = (mode & S_IXOTH) ? 'x' : '-';

	printf("%c%c%c%c%c%c%c%c%c%c", first, owner_r, owner_w, owner_x,
				       grp_r, grp_w, grp_x,
				       oth_r, oth_w, oth_x);
	
}

/**
 * Prints out the time argument given.
 */
static void	print_time(const time_t* timep)
{
	char time_buf[32];
	if (g_test_run)
	{
		// Use this instead of the actual modification time to ease testing
		strcpy(time_buf, "Jan  3 11:12:42");
	}
	else
	{
		// Get the current year
		struct tm tm_time;
		time_t t = time(NULL);
		(void)localtime_r(&t, &tm_time);
		int cur_year = tm_time.tm_year;

		(void)localtime_r(timep, &tm_time);

		const bool same_year = (cur_year == tm_time.tm_year);
		const char *fmt = same_year
				? "%b %e %H:%M"
				: "%b %e  %Y";

		(void)strftime(time_buf, sizeof(time_buf), fmt, &tm_time);
		time_buf[sizeof(time_buf)-1] = 0; // just in case
	}

	printf("%s", time_buf);
}

/**
 * Prints out 's' padded to the given length 'n'.
 */
static void	print_padding_str(size_t n, const char * s)
{
	size_t len = strlen(s);
	while (n > len)
	{
		printf(" ");
		n--;
	}
}

/**
 * Prints out 'i' padded to the given length 'n'.
 */
static void	print_padding_num(size_t n, unsigned long i)
{
	size_t len = count_digits(i);
	while (n > len)
	{
		printf(" ");
		n--;
	}
}

/**
 * Prints out file's owner user id information.
 */
static void	print_uid(const entry_s * data)
{
	if (data->uid_str)
	{
		printf("%s", data->uid_str);
		print_padding_str(g_globals.max_uid_width, data->uid_str);
	}
	else
	{
		printf("%d", data->uid);
		print_padding_num(g_globals.max_uid_width, data->uid);
	}
}

/**
 * Prints out file's owner group id information.
 */
static void	print_gid(const entry_s * data)
{
	if (data->gid_str)
	{
		printf("%s", data->gid_str);
		print_padding_str(g_globals.max_gid_width, data->gid_str);
	}
	else
	{
		printf("%d", data->gid);
		print_padding_num(g_globals.max_gid_width, data->gid);
	}
}

/**
 * Prints out the size of a file.
 */
static void	print_size(off_t size)
{
	print_padding_num(g_globals.max_size_width, (unsigned long)size);
	printf("%ld", size);
}

/**
 * Prints out the number of hard links to a file.
 */
static void	print_nlinks(nlink_t n)
{
	print_padding_num(g_globals.max_nlinks_width, n);
	printf("%ld", n);
}

/**
 * Prints out information about one file.
 */
static void	print_file_entry(const entry_s * data)
{
	print_mode(data->mode);
	
	printf(" ");

	print_nlinks(data->nlink);

	printf(" ");

	print_uid(data);

	printf(" ");

	print_gid(data);

	printf(" ");

	print_size(data->size);

	printf(" ");

	print_time(&data->mtime);

	printf(" ");

	printf("%s", data->fname);

	printf(" ");

	const bool is_link = S_ISLNK(data->mode);
	if (is_link)
	{
		if ( data->link_tgt )
		{
			printf("-> %s", data->link_tgt);
		}
		else
		{
			printf("-> (error)");
		}
		
	}

	printf("\n");
}

/**
 * Prints out information about one directory with all its contents.
 */
static void	print_dir_entry(const entry_s * data)
{
	printf("total %ld\n", g_blocksize_512 ? data->blocks/2 : data->blocks);

	FOREACH(f, data->files)
	{
		print_file_entry(&f->data);
	}
}

/**
 * Prints out the program's usage info.
 */
static void	usage(void)
{
	printf("Usage: ls OPTION [FILE]...\n");
	printf("Lists information about the FILEs (the current directory by default)).\n");
	printf("\n");
	printf("Options:\n");
	printf("  -l	use a long listing format\n");
}

/**
 * Parses the program's arguments, returning true on success and false in case of argument-related errors.
 */
static bool	parse_args(int arg, char* argv[])
{
	bool long_fmt = false;

	for(int i = 1; i < arg; i++)
	{
		char *	opt = argv[i];

		if (strcmp(opt, "--help") == 0)
		{
			usage();
			exit(EXIT_SUCCESS);
		}

		if (opt[0] == '-')
		{
			const char *	s = &opt[0];
			while ( *(++s) )
			{
				if (*s == 'l')
				{
					long_fmt = true;
				}
				else
				{
					error_args("invalid option -- '%c'", *s);
					fprintf(stderr, "Try 'ls --help' for more information.\n");
					return false;
				}
			}
		}
	}

	if (!long_fmt)
	{
		error_args("ls: option '-l' must be specified");
		return false;
	}
	return true;
}

/**
 * Initializes all the programs' global variables.
 */
static void		init_globals(void)
{
	g_test_run = getenv("_LS_TESTRUN");
	g_blocksize_512 = getenv("POSIXLY_CORRECT");
}

/**
 * Prints out (to stderr) the given message, decorated with the program's name.
 */
static void		error_args(const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	fprintf(stderr, "ls: ");
	vfprintf(stderr, msg, ap);
	fprintf(stderr, "\n");
}

/**
 * Prints out (to stderr) the given message with program's name and current errno description added.
 */
static void		error_err_args(const char *msg, ...)
{
	const char * err_descr = strerror(errno);

	va_list ap;
	va_start(ap, msg);
	fprintf(stderr, "ls: ");
	vfprintf(stderr, msg, ap);
	fprintf(stderr, ": %s\n", err_descr);
}
