/********************************************************/
/* Gsh Copyright (C) 1999 Vaughn Cato                   */
/* See the file "LICENSE", included with this software, */
/* for license information.                             */
/********************************************************/
#include "spawn.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#ifdef sun
#  include <stropts.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <assert.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "spawndata.h"
#include "termstate.h"
#include "pty.h"

#define MONITOR 0
#define MONITOR_PAUSE 0
/* If MONITOR, display each operation received from TermState */

#define DEBUG_TERM 0
/* If DEBUG_TERM, display each term handler that is called */

#define USE_GRANDCHILD 0

static void child_exec(char **argv)
{
	execvp(argv[0],argv);
	perror(argv[0]);
	exit(EXIT_FAILURE);
}

static void (*orig_child_signal_handler)(int) = 0;
typedef void *ClientData;

static void set_fl(int fd,int flags)
{
	int val = fcntl(fd,F_GETFL,0);

	if (val<0) {
		fprintf(stderr,"fcntl F_GETFL error\n");
		val = 0;
	}
	val |= flags;
	if (fcntl(fd,F_SETFL,val)<0) {
		fprintf(stderr,"fcntl F_SETFL error\n");
	}
}

void Spawn_WaitMaster(SpawnData *spawn_data)
{
	for (;;) {
		fd_set rdfds;
		FD_ZERO(&rdfds);
		FD_SET(spawn_data->master_fd,&rdfds);
		select(spawn_data->master_fd+1,&rdfds,NULL,NULL,0);
		if (FD_ISSET(spawn_data->master_fd,&rdfds)) {
			break;
		}
		fprintf(stderr,"term: select return when not ready.\n");
	}
}

int Spawn_ReadMaster(SpawnData *spawn_data)
{
	assert(!spawn_data->reading_master);
	spawn_data->reading_master = 1;
	assert(spawn_data->master_fd!=-1);

	{
		unsigned char buffer[1024];
		int bufsize = sizeof buffer;
		int readsize = read(spawn_data->master_fd,buffer,bufsize);

		if (readsize<1) {
			if (errno==EAGAIN) {
				/* no data available for reading */
			}
			else {
				perror("read");
				fprintf(stderr,"Spawn_ReadMaster: readsize=%d\n",readsize);
				TermState_EOF(&spawn_data->term_state);
				assert(spawn_data->master_fd>=0);
				close(spawn_data->master_fd);
				spawn_data->master_fd = -1;
				/* It shouldn't be possible for the master_fd to get EOF if the */
				/* child_fd is still open. */
				assert(spawn_data->slave_fd==-1);
			}
		}
		else {
			int hit_eof = 0;

			while (readsize<bufsize) {
				{
					/* If this timeout isn't here, select will sometimes report */
					/* nothing in the buffer, even when something is. */
					struct timeval timeout = {0,1};
					fd_set rdfds;
					FD_ZERO(&rdfds);
					FD_SET(spawn_data->master_fd,&rdfds);
					select(spawn_data->master_fd+1,&rdfds,NULL,NULL,&timeout);
					if (!FD_ISSET(spawn_data->master_fd,&rdfds)) {
						break;
					}
				}
				{
					int count =
						read(spawn_data->master_fd,buffer+readsize,bufsize-readsize);
					if (count<=0) {
						hit_eof = 1;
						break;
					}
					readsize += count;
				}
			}
			TermState_AddChars(&spawn_data->term_state,(const char *)buffer,readsize);

			if (hit_eof) {
				fprintf(stderr,"Hit EOF\n");
			}
			if (!hit_eof) {
				TermState_BufferEmpty(&spawn_data->term_state);
			}
		}
	}
	assert(spawn_data->reading_master);
	spawn_data->reading_master = 0;

	if (spawn_data->master_fd == -1) {
		DeleteSpawnData(spawn_data);
		return 0;
	}
	Spawn_Send(spawn_data,0,0);
	return 1;
}

static void MakeRaw(struct termios *tios)
{
	tios->c_lflag &= ~ICANON;
	tios->c_cc[VMIN] = 1;
	tios->c_cc[VTIME] = 0;
}

static int ReadSlave(SpawnData *spawn_data,char *buffer,int bufsize)
{
	struct termios normattr;
	struct termios termattr;
	int readsize;

	tcgetattr(spawn_data->slave_fd,&normattr);
	termattr = normattr;
	MakeRaw(&termattr);
	termattr.c_cc[VEOF] = 0;
	tcsetattr(spawn_data->slave_fd,TCSANOW,&termattr);
	set_fl(spawn_data->slave_fd,O_NONBLOCK);
	readsize = read(spawn_data->slave_fd,buffer,bufsize-1);
	tcsetattr(spawn_data->slave_fd,TCSANOW,&normattr);
	if (readsize<0) {
		readsize = 0;
	}

	/* eof characters get turned into nulls.  Change them back. */
	/* I'm not sure if this is normal, or if it just happens under linux. */
	{
		int i = 0;
		for (;i<readsize;++i) {
			if (buffer[i]==0) {
				buffer[i] = normattr.c_cc[VEOF];
			}
		}
	}
	buffer[readsize] = '\0';
	return readsize;
}

static const struct {
	const char *name;
	int signum;
} signals[] = {
		{"HUP",   SIGHUP},
		{"INT",   SIGINT},
		{"QUIT",  SIGQUIT},
		{"ILL",   SIGILL},
		{"TRAP",  SIGTRAP},
		{"ABRT",  SIGABRT},
		{"BUS",   SIGBUS},
		{"FPE",   SIGFPE},
		{"KILL",  SIGKILL},
		{"USR1",  SIGUSR1},
		{"SEGV",  SIGSEGV},
		{"USR2",  SIGUSR2},
		{"PIPE",  SIGPIPE},
		{"ALRM",  SIGALRM},
		{"TERM",  SIGTERM},
		{"CHLD",  SIGCHLD},
		{"CONT",  SIGCONT},
		{"STOP",  SIGSTOP},
		{"TSTP",  SIGTSTP},
		{"TTIN",  SIGTTIN},
		{"TTOU",  SIGTTOU},
		{"URG",   SIGURG},
		{"XCPU",  SIGXCPU},
		{"XFSZ",  SIGXFSZ},
		{"VTALRM",SIGVTALRM},
		{"PROF",  SIGPROF},
		{"WINCH", SIGWINCH},
		{"IO",    SIGIO},
		{"PWR",   SIGPWR},
		{NULL,-1}
};

static void SpawnExit(SpawnData *spawn_data,int exit_signal,int exit_code)
{
	if (exit_signal!=SIGSTOP) {
		assert(spawn_data->slave_fd!=-1);
		assert(spawn_data->child_running);
		spawn_data->child_running = 0;
		spawn_data->child_pid = -1;
		spawn_data->child_exit_handled = 1;
	}
	spawn_data->exit_signal = exit_signal;
	spawn_data->exit_code = exit_code;
}

#if 0
static void SIGCHLDHandler(int signum);
#endif

void Spawn_CheckChildExit(SpawnData *spawn_data,int wait_for_exit)
{
	int exit_code = 0;
	int exit_signal = 0;
	{
		int status;
		int flags = WUNTRACED;
		if (!wait_for_exit) {
			flags |= WNOHANG;
		}
		pid_t exited_pid = waitpid(-1,&status,flags);

		if (exited_pid==-1) {
			/* Errors handled below */
		}
		else if (exited_pid==0) {
			perror("waitpid");
		}
		else if (WIFEXITED(status)) {
			fprintf(stderr,"Process %d exited\n",(int)exited_pid);
			exit_code=WEXITSTATUS(status);
		}
		else if (WIFSTOPPED(status)) {
			fprintf(stderr,"Process %d stopped\n",(int)exited_pid);
			exit_signal = SIGSTOP;
		}
		else if (WIFSIGNALED(status)) {
			exit_signal = WTERMSIG(status);
			fprintf(
					stderr,
					"Process %d was signalled with %d\n",(int)exited_pid,exit_signal
			);
		}
		else {
			fprintf(stderr,"Process %d exited abnormally\n",(int)exited_pid);
		}

		if (exited_pid>0) {
			spawn_data = FirstSpawnData();
			while (spawn_data) {
				if (spawn_data->child_pid==exited_pid) {
					break;
				}
				spawn_data = spawn_data->next;
			}
			if (!spawn_data) {
				fprintf(stderr,"Process %d is not in spawn list\n",(int)exited_pid);
			}
		}
		else if (exited_pid==0) {
			fprintf(stderr,"No child processes\n");
		}
		else if (exited_pid==-1 && errno!=EINTR) {
			perror("waitpid");
		}
		else {
			fprintf(stderr,"No child processes\n");
		}
	}
	if (spawn_data) {
		SpawnExit(spawn_data,exit_signal,exit_code);
	}
#if 0
#ifdef sgi
	signal(SIGCHLD,SIGCHLDHandler);
#endif
#endif
}

#if 0
static void SIGCHLDHandler(int signum)
{
	if (orig_child_signal_handler) {
		(*orig_child_signal_handler)(signum);
	}
}
#endif

static void OpenPty(SpawnData *spawn_data)
{
	assert(spawn_data->master_fd==-1);
	assert(spawn_data->slave_fd==-1);

#ifdef sun
	{
		spawn_data->master_fd = open("/dev/ptmx", O_RDWR);
		if (spawn_data->master_fd == -1) {
			fprintf(stderr,"Error opening pty\n");
			perror("open: /dev/ptmx");
			exit(EXIT_FAILURE);
		}
		grantpt(spawn_data->master_fd);
		unlockpt(spawn_data->master_fd);
		spawn_data->slave_fd = open(ptsname(spawn_data->master_fd), O_RDWR);
		if (spawn_data->slave_fd==-1) {
			fprintf(stderr,"Error opening slave\n");
			fprintf(stderr, "open: ");
			perror(ptsname(spawn_data->master_fd));
			exit(EXIT_FAILURE);
		}
		/* The following are "to get terminal semantics" */
		ioctl(spawn_data->slave_fd, I_PUSH, "ptem");
		ioctl(spawn_data->slave_fd, I_PUSH, "ldterm");
	}
#endif /* sun */
#ifdef sgi
	{
		char *slave_name =
			_getpty(&spawn_data->master_fd,O_RDWR,S_IREAD|S_IWRITE,0);
		if (!slave_name) {
			fprintf(stderr,"Error opening pty\n");
			perror("_getpty");
			exit(EXIT_FAILURE);
		}
		spawn_data->slave_fd = open(slave_name,O_RDWR);
		if (spawn_data->slave_fd==-1) {
			fprintf(stderr,"Error opening slave\n");
			exit(EXIT_FAILURE);
		}
	}
#endif
#if !defined(sun) && !defined(sgi)
	fprintf(stderr,"opening pty pair\n");
	if (open_pty_pair(&spawn_data->master_fd,&spawn_data->slave_fd)!=0) {
		fprintf(stderr,"Failed to open pty pair.\n");
		exit(EXIT_FAILURE);
	}
	fprintf(stderr,"pty pair open\n");
#endif
#ifdef TIOCNOTTY
	/* We don't want the slave to be the controlling terminal.  A tty can only */
	/* be used as the controlling terminal of one session.  If it is being */
	/* used as the controlling terminal for this session, it can't be used */
	/* as the controlling terminal for the new session we are going to create. */
	{
		int sid = getsid(0);
		int pid = getpid();
		if (sid!=pid) {
			fprintf(stderr,"removing tty from slave %d\n",spawn_data->slave_fd);
			ioctl(spawn_data->slave_fd,TIOCNOTTY,0);
		}
		else {
			/* we are the session leader, so we can't remove the controlling tty. */
		}
	}
#endif
	fprintf(stderr,"setting master non-blocking\n");
	set_fl(spawn_data->master_fd,O_NONBLOCK);
	fprintf(stderr,"setting tty settings\n");
	{
		struct termios termattr;

		tcgetattr(spawn_data->slave_fd,&termattr);
		termattr.c_cc[VINTR] = 3;
		termattr.c_cc[VERASE] = 8;
		tcsetattr(spawn_data->slave_fd,TCSANOW,&termattr);
	}

	/* If the close-on-exec flag is not set for these file descriptors, they */
	/* will remain open in the child processes.  If two child processes are */
	/* started, and the first one finishes.  The master won't get EOF until */
	/* the second one finishes because the second one will have the slave_fd */
	/* open also. */
	fprintf(stderr,"setting close-on-exec\n");
	fcntl(spawn_data->master_fd,F_SETFD,1);
	fcntl(spawn_data->slave_fd,F_SETFD,1);

	strcpy(spawn_data->slave_name,ttyname(spawn_data->slave_fd));
	fprintf(stderr,"Pty pair is open: master=%d, slave=%d\n",spawn_data->master_fd,spawn_data->slave_fd);
}



#if USE_GRANDCHILD
static int child_signal = -1;

static void ChildSignalHandler(int signum)
{
	child_signal = signum;
}

static int caught_sigint;

static void HandleSIGINT(int signum)
{
	caught_sigint = 1;
}
#endif

void Spawn_Exec(SpawnData *spawn_data,char **argv)
{
#if 0
	{
		/* re-install our SIGCHLD handler if it was removed */
		void (*old_signal_handler)(int) =
			signal(SIGCHLD,SIGCHLDHandler);
		if (old_signal_handler!=SIGCHLDHandler) {
			fprintf(stderr,"SIGCHLDHandler was removed, reinstalling\n");
			if (old_signal_handler==SIG_ERR) {
				fprintf(stderr,"Error installing signal handler\n");
				perror("signal");
			}
			orig_child_signal_handler = old_signal_handler;
		}
	}
#endif
	{
		int child_pid = fork();
		int session_id = 0;
		int setsid_errno = 0;

		if (child_pid==0) {
			fprintf(stderr,"term: in child\n");
			/* child process */

			/* Set signals back to their default behavior */
			/* signal(SIGINT,SIG_DFL); */
#if USE_GRANDCHILD
			signal(SIGINT,HandleSIGINT);
#else
			signal(SIGINT,SIG_DFL);
#endif
			signal(SIGQUIT,SIG_DFL);
#if 0
			signal(SIGCHLD,SIG_DFL);
#endif

			/* Close all open files so the child won't inherit them.  An example */
			/* problem is the X11 display socket.  If the child inheretid this, */
			/* then the display would be left open if a child was running even if */
			/* gsh exited. */
			{
				int fdlimit = sysconf(_SC_OPEN_MAX);
				int i = 0;

				for (;i<fdlimit;++i) {
					close(i);
				}
			}

			fprintf(stderr,"term: setting sid\n");
#if 1
			session_id = setsid();
			setsid_errno = errno;
#else
			setpgrp();
			{
				int fd = open("/dev/tty",O_RDWR);

				if (fd>=0) {
					ioctl(fd,TIOCNOTTY,0);
					close(fd);
				}
			}
#endif

			fprintf(stderr,"term: re-opening slave\n");
			{
				/* Open slave here after setsid so that it becomes the controlling */
				/* terminal for this process */
				int slave = open(spawn_data->slave_name,O_RDWR);

				if (slave<0) {
					perror("open slave");
					exit(EXIT_FAILURE);
				}

				fprintf(stderr,"term: slave re-opened\n");
				assert(slave==0);

				dup2(slave,1);
				dup2(slave,2);

				{
					if (session_id==-1) {
						errno = setsid_errno;
						perror("setsid");
					}
				}
				/* 4.3+BSD uses this ioctl to allocate the controlling terminal */
#ifdef TIOCSCTTY
				if (getuid()==0) {
					/* using 1 here forces it, but can only be done by root */
					if (ioctl(slave, TIOCSCTTY, 1)!=0) {
						perror("ioctl TIOCSCTTY");
					}
				}
				else {
					if (ioctl(slave, TIOCSCTTY, 0)!=0) {
						perror("ioctl TIOCSCTTY");
					}
				}
#endif

				ioctl(slave,TIOCSWINSZ,&spawn_data->window_size);
			}
			/* Turn off terminal i/o stop signals so that the tcsetpgrp will work */
			{
				signal(SIGTTIN,SIG_IGN);
				signal(SIGTTOU,SIG_IGN);
			}
			/* Turn on the SIGHUP signal so if gsh dies, the child process will */
			/* die too. */
			signal(SIGHUP,SIG_DFL);
			/* Fork the grandchild that will be the process group leader of a new */
			/* foreground process group */
			{
#if USE_GRANDCHILD
				int grandchild_pid = fork();

				/* This will catch continue signals so they can be redirected to */
				/* the grandchild.  The variable child_signal will be set. */
				if (grandchild_pid==0)
#endif
				{
					/* In the grandchild process */

					int pid = getpid();
#if USE_GRANDCHILD
					/* Make a new process group for the grandchild */
					{
						int result = setpgid(0,pid);
						if (result!=0) {
							fprintf(stderr,"grandchild setpgid: result=%d\n",result);
						}
					}
#endif
					/* Make the grandchild process group be the foreground process */
					/* group */
					{
						int result = tcsetpgrp(0,pid);
						if (result!=0) {
							perror("grandchild tcsetpgrp");
						}
					}
					/* Turn terminal i/o stop signals back on */
					{
						signal(SIGTTIN,SIG_DFL);
						signal(SIGTTOU,SIG_DFL);
					}
					child_exec(argv); /* doesn't return */
				}
#if USE_GRANDCHILD
				child_signal = -1;
				{
					struct sigaction act;

					act.sa_handler=ChildSignalHandler;
					sigemptyset(&act.sa_mask);
					act.sa_flags=0;
					sigaction(SIGCONT,&act,0);
				}
				{
					int exit_code = 0;
					int status;

					for (;;) {
						pid_t pid;

						pid = waitpid(grandchild_pid,&status,WUNTRACED);

						if (pid!=grandchild_pid) {
							if (child_signal==SIGCONT) {
								kill(grandchild_pid,SIGCONT);
							}
							else {
								fprintf(stderr,"waitpid returned %d\n",(int)pid);
								break;
							}
						}
						else {
							if (WIFSIGNALED(status)) {
#if 1
								fprintf(
										stderr,
										"process killed with signal %d\n",(int)WTERMSIG(status)
								);
#endif
								break;
							}
							else if (WIFEXITED(status)) {
								exit_code = WEXITSTATUS(status);
#if 1
								fprintf(stderr,"process exited with status %d\n",exit_code);
#endif
								break;
							}
							else {
								if (WSTOPSIG(status)) {
									{
										int pgrp = getpgrp();
										int result = tcsetpgrp(0,pgrp);
										if (result!=0) {
											perror("tcsetpgrp");
										}
									}
									fprintf(stderr,"(stopped)\n");
									kill(0,SIGSTOP);
									fprintf(stderr,"(continued)\n");
									{
										int result = tcsetpgrp(0,grandchild_pid);
										if (result!=0) {
											perror("tcsetpgrp");
										}
									}
									killpg(grandchild_pid,SIGCONT);
								}
								else {
#if 1
									fprintf(stderr,"process exited abnormally\n");
#endif
									break;
								}
							}
						}
					}
					/* Make the session leader be the foreground process group again. */
					/* This makes anything the child forked be in the background */
					/* process group, so it won't get SIGHUP when the session leader */
					/* dies. */
					{
						int pgrp = getpgrp();
						int result = tcsetpgrp(0,pgrp);
						if (result!=0) {
							perror("tcsetpgrp");
						}
					}
					signal(SIGINT,SIG_DFL);
					if (caught_sigint ||
							(WIFSIGNALED(status) && WTERMSIG(status)==SIGINT)) {
						/* Signal self so the parent can tell that the child was  */
						/* signalled */
						kill(getpid(),SIGINT);
					}
					/* Use _exit to prevent atexit cleanup operations */
					_exit(exit_code);
				}
#endif
			}
		}
		else {
			/* parent */
			spawn_data->child_running = 1;
			spawn_data->child_exit_handled = 0;
			spawn_data->child_pid = child_pid;
			spawn_data->exit_code = -1;
			spawn_data->exit_signal = -1;
			spawn_data->exit_reported = 0;
			fprintf(stderr,"Process %d started\n",child_pid);
		}
	}
}

void Spawn_Delete(SpawnData *spawn_data)
{
	if (spawn_data->master_fd!=-1) {
		close(spawn_data->master_fd);
		spawn_data->master_fd = -1;
	}
	if (spawn_data->slave_fd!=-1) {
		close(spawn_data->slave_fd);
		spawn_data->slave_fd = -1;
	}
	spawn_data->child_pid = -1;
	spawn_data->child_running = 0;
	spawn_data->child_exit_handled = 1;
	DeleteSpawnData(spawn_data);
}

void Spawn_Close(SpawnData *spawn_data)
{
	char buffer[1024];
	ReadSlave(spawn_data,buffer,sizeof buffer);

	assert(spawn_data->slave_fd!=-1);
	close(spawn_data->slave_fd);
	spawn_data->slave_fd = -1;
}

void Spawn_Send(SpawnData *spawn_data,const char *str,int len)
{
	if (spawn_data->output_buffer) {
		assert(spawn_data->output_buffer_size>0);

		if (len>0) {
			spawn_data->output_buffer =
				(char *)
				realloc(
						spawn_data->output_buffer,spawn_data->output_buffer_size+len
				);
			memcpy(spawn_data->output_buffer+spawn_data->output_buffer_size,str,len);
			spawn_data->output_buffer_size += len;
		}

		{
			int write_size =
				write(
						spawn_data->master_fd,
						spawn_data->output_buffer,
						spawn_data->output_buffer_size
				);

			fprintf(
					stderr,
					"Wrote %d of %d characters from the buffer\n",
					write_size,
					spawn_data->output_buffer_size
			);

			if (write_size<1) {
				return;
			}

			if (write_size<spawn_data->output_buffer_size) {
				memmove(
						spawn_data->output_buffer,
						spawn_data->output_buffer+write_size,
						spawn_data->output_buffer_size-write_size
				);
				spawn_data->output_buffer_size -= write_size;
				spawn_data->output_buffer =
					(char *)
					realloc(
							spawn_data->output_buffer,spawn_data->output_buffer_size
					);
			}
			else {
				free(spawn_data->output_buffer);
				spawn_data->output_buffer = 0;
			}
			return;
		}
	}

	if (len>0) {
		int write_size = write(spawn_data->master_fd,str,len);

		if (write_size<0) {
			write_size = 0;
		}

		if (write_size<len) {
			int unwritten = len-write_size;
			spawn_data->output_buffer = (char *)malloc(unwritten);
			spawn_data->output_buffer_size = unwritten;
			memcpy(spawn_data->output_buffer,str+write_size,unwritten);
			fprintf(stderr,"Buffered %d characters\n",unwritten);
		}
	}
}

int Spawn_Kill(SpawnData *spawn_data,const char *signame)
{
    if (!spawn_data)
    {
        fprintf(stderr, "NULL spawn_data passed to Spawn_Kill\n");
        return 0;
    }
	if (spawn_data->child_pid==-1) {
		fprintf(stderr,"Child is already dead\n");
		return 1;
	}
	{
		int i =0;
		for (;signals[i].name;++i) {
			if (strcmp(signals[i].name,signame)==0) {
				fprintf(
						stderr,
						"killing pid %d with %d\n",spawn_data->child_pid,signals[i].signum
				);
				kill(spawn_data->child_pid,signals[i].signum);
				return 1;
			}
		}
	}
	fprintf(stderr,"Invalid signal name: %s\n",signame);
	return 0;
}

void Spawn_Stty(SpawnData *spawn_data,int rows,int cols)
{
        assert(spawn_data);
	spawn_data->window_size.ws_row = rows;
	spawn_data->window_size.ws_col = cols;
	ioctl(spawn_data->slave_fd,TIOCSWINSZ,&spawn_data->window_size);
}

SpawnData *Spawn_Open(CommandHandlers *cmd_handlers,void *client_data)
{
	SpawnData *spawn_data = NewSpawnData(cmd_handlers,client_data);

	OpenPty(spawn_data);
	return spawn_data;
}

void Spawn_SetOutputEnabled(SpawnData *spawn_data,int enabled)
{
	assert(spawn_data->master_fd!=-1);
	spawn_data->output_enabled = enabled;

	if (!spawn_data->output_enabled) {
		/* stop listening for output */
	}
	else {
		/* start listening for output */
	}
}

void Spawn_SetDefaultProcs(SpawnData *spawn_data,void *client_data)
{
	spawn_data->default_data = client_data;
}

void Spawn_Init()
{
#if 0
	assert(!orig_child_signal_handler);
	orig_child_signal_handler = signal(SIGCHLD,SIGCHLDHandler);
	if (orig_child_signal_handler==SIG_ERR) {
		fprintf(stderr,"Error installing signal handler\n");
		perror("signal");
	}
	assert(orig_child_signal_handler!=SIGCHLDHandler);
#endif
}
