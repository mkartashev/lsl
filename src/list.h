#ifndef _LIST_H_
#define _LIST_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct 			list_s;
typedef struct list_s *	list_t;

struct entry_s
{
	char *		fname;		// file name, may or may not be malloc'ed

	mode_t		mode;		// file mode; see stat(2)
	nlink_t		nlink;		// number of hard links pointing to file; see stat(2)
	uid_t		uid;		// uid of owner; see stat(2)
	gid_t		gid;		// gid of owner; see stat(2)	
	off_t		size;		// file size; see stat(2)
	blkcnt_t	blocks;		// directory size in blocks; see stat(2)
	time_t		mtime;		// time of modification; see stat(2)

	char *		uid_str;	// malloc'ed owner user id string
	char *		gid_str;	// malloc'ed owner group id string

	char *		link_tgt;	// malloc'ed name of the link target, if symlink

	list_t		files;		// list of files, if mode indicates directory; NULL otherwise
};

typedef struct entry_s	entry_s;

struct list_s
{
	struct list_s *	next;
	entry_s		data;
};

extern list_t	list_add(list_t list, list_t item);
extern list_t	list_insert(list_t list, list_t item);
extern list_t	list_create();
extern list_t	list_delete(list_t list);

#define FOREACH(i, l)	for(list_t i = l; i; i = i->next)

#endif
