/********************************************************/
/* Gsh Copyright (C) 1999 Vaughn Cato                   */
/* See the file "LICENSE", included with this software, */
/* for license information.                             */
/********************************************************/

/************************************************************************/
/* This code was taken largely from the book                            */
/* "Advanced Programming in the UNIX Environment" by W. Richard Stevens */
/* ISBN 0-201-56317-7                                                   */
/************************************************************************/

#include <unistd.h>
#include <stdio.h>

#ifdef __svr4__

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stropts.h>

extern char *ptsname(int);

static int ptym_open(char *pts_name)
{
	char *ptr;
	int fdm;

	strcpy(pts_name,"/dev/ptmx");
	if ((fdm=open(pts_name,O_RDWR))<0)
		return -1;

	if (grantpt(fdm) < 0) {
		close(fdm);
		return -2;
	}

	if (unlockpt(fdm) < 0) {
		close(fdm);
		return -3;
	}

	if ((ptr=ptsname(fdm))==NULL) {
		close(fdm);
		return -4;
	}

	strcpy(pts_name,ptr);
	return fdm;
}

static int ptys_open(int fdm, char *pts_name)
{
	int fds;

	if ((fds=open(pts_name,O_RDWR))<0) {
		close(fdm);
		return -5;
	}

	if (ioctl(fds,I_PUSH,"ptem")<0) {
		close(fdm);
		close(fds);
		return -6;
	}

	if (ioctl(fds,I_PUSH,"ldterm")<0) {
		close(fdm);
		close(fds);
		return -7;
	}

	if (ioctl(fds,I_PUSH,"ttcompat")<0) {
		close(fdm);
		close(fds);
		return -8;
	}

	return fds;
}

#else /* __svr4 */

#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <string.h>

int ptym_open(char *pts_name,int pts_name_size)
{
	int  fdm;
	const char *ptr1, *ptr2;

	fdm = open("/dev/ptmx",O_RDWR|O_NOCTTY);

	if (fdm!=-1) {
		if (grantpt(fdm)==-1) {
			fprintf(stderr,"Couldn't grantpt\n");
			close(fdm);
			return -1;
		}
		if (unlockpt(fdm)==-1) {
			fprintf(stderr,"Couldn't unlockpt\n");
			close(fdm);
			return -1;
		}
		{
			char *ptsn = ptsname(fdm);
			if ((int)strlen(ptsn)>=pts_name_size) {
				fprintf(stderr,"Bad pts name\n");
				close(fdm);
				return -1;
			}
			strcpy(pts_name,ptsn);
		}
		return fdm;
	}
	else {
		strcpy(pts_name, "/dev/ptyXY");
		/* array index: 0123456789 (for references in following code) */
		for (ptr1 = "pqrstuvwxyzPQRST"; *ptr1 != 0; ptr1++) {
			pts_name[8] = *ptr1;
			for (ptr2 = "0123456789abcdef"; *ptr2 != 0; ptr2++) {
				pts_name[9] = *ptr2;

				/* try to open master */
				if ( (fdm = open(pts_name, O_RDWR)) < 0) {
					if (errno == ENOENT) {  /* different from EIO */
						fprintf(stderr,"No free ptys\n");
						return -1;            /* out of pty devices */
					}
				}
				else {
					pts_name[5] = 't';      /* change "pty" to "tty" */
					if (access(pts_name,R_OK|W_OK)==0) {
						return fdm;            /* got it, return fd of master */
					}
					pts_name[5] = 'p';      /* change "tty" back to "pty" */
				}
			}
		}
		fprintf(stderr,"No free ptys\n");
		return -1;             /* out of pty devices */
	}
}

int ptys_open(int fdm, char *pts_name)
{
	struct group    *grptr;
	int                             gid, fds;

	if ( (grptr = getgrnam("tty")) != NULL)
		gid = grptr->gr_gid;
	else
		gid = -1;               /* group tty is not in the group file */

	/* The following two function do not work unless we are root */
	int rc = chown(pts_name, getuid(), gid);
	(void)rc;
	/*
	  the above line was added to avoid the "warning: ignoring return value of int chown(const char*, __uid_t, __gid_t), declared with attribute warn_unused_result"
	  when compiling with -Wall -O2

	Note: (void)chown(pts_name, getuid(), gid); also complained.
	 */
	chmod(pts_name, S_IRUSR | S_IWUSR | S_IWGRP);

	if ((fds = open(pts_name, O_RDWR)) < 0) {
		close(fdm);
		return(-1);
	}
	return(fds);
}
#endif

int open_pty_pair(int *master_fd,int *slave_fd)
{
	char pts_name[20];

	*master_fd = ptym_open(pts_name,20);
	if (*master_fd < 0) {
		perror("error opening master pty");
		return -1;
	}
	*slave_fd = ptys_open(*master_fd,pts_name);
	if (*slave_fd < 0) {
		perror("error opening slave tty");
		return -1;
	}
	return 0;
}
