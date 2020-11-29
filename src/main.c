#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
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

static void	init_globals(void);
static bool	parse_args(int arg, char* argv[]);
static void	usage(void);

static int	list_one(bool show_prefix, bool files_only, const char* fname);
static inline bool	is_option(const char* opt)
{
	return (opt[0] == '-');
}

bool	test_run;	// when true, makes output more deterministic for testing purposes

int	main(int argc, char *argv[])
{
	init_globals();

	if (! parse_args(argc, argv) )
	{
		usage();
		return 2;	// to be compatible with ls(1)
	}

	for(int i = 1; i < argc; i++)
	{
	}

	int		nfiles = 0;
	for(int i = 1; i < argc; i++)
	{
		if (!is_option(argv[i]))
		{
			nfiles++;
			list_one(false, true, argv[i]);
		}
	}

	if (nfiles > 0)
	{
		printf("\n");
	}

	const bool	show_prefix = (nfiles > 1);
	for(int i = 1; i < argc; i++)
	{
		if (!is_option(argv[i]))
		{
			list_one(show_prefix, false, argv[i]);
		}
	}

	return EXIT_SUCCESS;
}

static int	print_link(const char* fname, size_t bufsz)
{
	if (bufsz == 0)
	{
		bufsz = PATH_MAX;
	}
	char *	target_name = malloc(bufsz);
	assert(target_name);

	ssize_t	nbytes = readlink(fname, target_name, bufsz);
	if (nbytes == -1)
	{
		char*	errmsg = strerror(errno);
		fprintf(stderr, "ls: cannot read link '%s': %s\n", fname, errmsg);
		free(target_name);
		return EXIT_FAILURE;
	}
	else if ((size_t)nbytes == bufsz)
	{
		target_name[bufsz-1] = 0;	// the name may have been truncated
	}
	else
	{
		target_name[nbytes] = 0;
	}

	printf(" -> %s", target_name);

	free(target_name);

	return EXIT_SUCCESS;
}

static void	print_mode(mode_t mode)
{
	char	first = '-';
	if (S_ISCHR(mode))
	{
		first = 'c';
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

	char	owner_r = (mode & S_IRUSR) ? 'r' : '-';
	char	owner_w = (mode & S_IWUSR) ? 'w' : '-';
	char	owner_x = (mode & S_IXUSR) ? 'x' : '-';

	char	grp_r = (mode & S_IRGRP) ? 'r' : '-';
	char	grp_w = (mode & S_IWGRP) ? 'w' : '-';
	char	grp_x = (mode & S_IXGRP) ? 'x' : '-';

	char	oth_r = (mode & S_IROTH) ? 'r' : '-';
	char	oth_w = (mode & S_IWOTH) ? 'w' : '-';
	char	oth_x = (mode & S_IXOTH) ? 'x' : '-';

	printf("%c%c%c%c%c%c%c%c%c%c", first, owner_r, owner_w, owner_x,
					      grp_r, grp_w, grp_x,
					      oth_r, oth_w, oth_x);
	
}

static void	print_time(const time_t* timep)
{
	struct tm	tm_time;
	(void)localtime_r(timep, &tm_time);

	char	time_buf[256];

	strftime(time_buf, sizeof(time_buf), "%b %H:%M:%S", &tm_time);
	if (test_run)
	{
		// overwrite the actual modification time to ease testing
		strcpy(time_buf, "XXX 11:11:42");
	}

	printf("%s", time_buf);
}

static void	print_uid(uid_t uid)
{
	struct passwd *	pswd = getpwuid(uid);
	if (test_run)
	{
		printf("42");
	}
	else if (pswd)
	{
		printf("%s", pswd->pw_name);
	}
	else
	{
		printf("%d", uid);
	}
}

static void	print_gid(gid_t gid)
{
	struct group *	grp = getgrgid(gid);
	if (test_run)
	{
		printf("42");
	}
	if (grp)
	{
		printf("%s", grp->gr_name);
	}
	else
	{
		printf("%d", gid);
	}
}

static int	list_one_file(const char* fname, const struct stat* sb)
{
	int rc = EXIT_SUCCESS;

	print_mode(sb->st_mode);

	printf(" ");

	// Print the number of hard links
	printf(" %ld", sb->st_nlink );

	printf("\t");

	print_uid(sb->st_uid);
	
	printf("\t");
	
	print_gid(sb->st_uid);

	printf("\t");

	print_time(&sb->st_mtime);

	printf("\t");

	printf("%s", fname);

	const bool	is_link	= ((sb->st_mode & S_IFMT) == S_IFLNK);
	if (is_link)
	{
		const size_t	bufsz = (size_t)(sb->st_size + 1);
		rc = print_link(fname, bufsz);
		if (rc != EXIT_SUCCESS)
		{
			return rc;
		}
	}

	printf("\n");

	return EXIT_SUCCESS;
}

static inline bool	should_show_file(const char* fname)
{
	return fname[0] != '.';
}

static int	list_one_dir(const char* dname, const struct stat* sb)
{
	blksize_t	blksize = 0;
	(void)sb;

	DIR*	dir = opendir(dname);
	if (!dir)
	{
		char*	errmsg = strerror(errno);
		fprintf(stderr, "ls: cannot access '%s': %s\n", dname, errmsg);
		return EXIT_FAILURE;
	}

	errno = 0;	// necessary for readdir()

	while (true)
	{
		struct dirent*	f = readdir(dir);
		if (!f && errno != 0)
		{
			char*	errmsg = strerror(errno);
			fprintf(stderr, "ls: cannot read directory '%s': %s\n", dname, errmsg);
			closedir(dir);
			return EXIT_FAILURE;
		}
		else if (!f)
		{
			break;
		}

		if (!should_show_file(f->d_name))
		{
			continue;
		}

		// TODO: predictable ordering
		struct stat	sb_f;

		size_t	len = strlen(dname) + strlen(f->d_name) + 2;
		char *	fname = strdup(dname);
		strncat(fname, "/", len);
		strncat(fname, f->d_name, len);
		
		
		if (lstat(fname, &sb_f) == -1)
		{
			char*	errmsg = strerror(errno);
			fprintf(stderr, "ls: cannot access '%s': %s\n", f->d_name, errmsg);
			closedir(dir);
			return EXIT_FAILURE;
		}

		blksize += sb_f.st_blocks;

		list_one_file(f->d_name, &sb_f);
	}

	if (closedir(dir) == -1)
	{
		char*	errmsg = strerror(errno);
		fprintf(stderr, "ls: cannot access '%s': %s\n", dname, errmsg);
		return EXIT_FAILURE;
	}

	printf("total %ld\n", blksize);

	return EXIT_SUCCESS;
}

static int	list_one(bool show_prefix, bool files_only, const char* fname)
{
	struct stat	sb;

	if ((lstat(fname, &sb) == -1) && files_only)
	{
		char*	errmsg = strerror(errno);
		fprintf(stderr, "ls: cannot access '%s': %s\n", fname, errmsg);
		return EXIT_FAILURE;
	}

	const bool	is_dir	= ((sb.st_mode & S_IFMT) == S_IFDIR);

	if (!files_only && is_dir)
	{
		if (show_prefix)
		{
			printf("%s:\n", fname);
		}
		list_one_dir(fname, &sb);
	}
	else if (files_only && !is_dir)
	{
		int rc = list_one_file(fname, &sb);
		if (rc != EXIT_SUCCESS)
		{
			return rc;
		}
	}

	return EXIT_SUCCESS;
}

static void	usage(void)
{
	printf("Usage: ls OPTION [FILE]...\n");
	printf("Lists information about the FILEs (the current directory by default)).\n");
	printf("\n");
	printf("Options:\n");
	printf("  -l	use a long listing format\n");
}

static bool	parse_args(int arg, char* argv[])
{
	bool long_fmt = false;

	for(int i = 1; i < arg; i++)
	{
		char *	opt = argv[i];

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
					fprintf(stderr, "ls: invalid option -- '%c'\n", *s);
					return false;
				}
			}
		}
	}

	if (!long_fmt)
	{
		fprintf(stderr, "ls: option '-l' must be specified\n");
	}
	return true;
}

static void	init_globals(void)
{
	test_run = getenv("_LS_TEST_RUN");
}
