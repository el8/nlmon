/*
 * Copyright Penguin Boost, 2016
 * Author(s): Jan Glauber <jan.glauber@gmail.com>
 *
 * Derived from:
 * http://bewareofgeek.livejournal.com/2945.html
 * This file is licensed under the GPL v2 (http://www.gnu.org/licenses/gpl2.txt)
 * (some parts was originally borrowed from proc events example)
 */

#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <linux/cn_proc.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <ctype.h>
#include <pthread.h>

#include "helper.h"
#include "bitmap.h"
#include "hash.h"
#include "nlmon.h"

/* one-shot scan thread */
pthread_t procfs_thread;

/* number of currently running threads */
atomic_t nr_threads;

static int setup_connector(void)
{
	struct sockaddr_nl nl_sa;
	int rc, nl_fd;

	nl_fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
	if (nl_fd < 0)
		DIE_PERROR("socket failed");

	nl_sa.nl_family = AF_NETLINK;
	nl_sa.nl_groups = CN_IDX_PROC;
	nl_sa.nl_pid = getpid();

	rc = bind(nl_fd, (struct sockaddr *) &nl_sa, sizeof(nl_sa));
	if (rc < 0)
		DIE_PERROR("bind failed");
	return nl_fd;
}


struct nlcn_send_msg {
	struct nlmsghdr nl_hdr;
	struct {
		struct cn_msg cn_msg;
		enum proc_cn_mcast_op cn_mcast;
	} __attribute__ ((__packed__));
} __attribute__ ((aligned(NLMSG_ALIGNTO)));

/* subscribe on proc events (process notifications) */
static void set_proc_ev_listen(int nl_fd, bool enable)
{
	struct nlcn_send_msg nlcn_msg;
	int rc;

	memset(&nlcn_msg, 0, sizeof(nlcn_msg));
	nlcn_msg.nl_hdr.nlmsg_len = sizeof(nlcn_msg);
	nlcn_msg.nl_hdr.nlmsg_pid = getpid();
	nlcn_msg.nl_hdr.nlmsg_type = NLMSG_DONE;

	nlcn_msg.cn_msg.id.idx = CN_IDX_PROC;
	nlcn_msg.cn_msg.id.val = CN_VAL_PROC;
	nlcn_msg.cn_msg.len = sizeof(enum proc_cn_mcast_op);

	nlcn_msg.cn_mcast = enable ? PROC_CN_MCAST_LISTEN : PROC_CN_MCAST_IGNORE;

	rc = send(nl_fd, &nlcn_msg, sizeof(nlcn_msg), 0);
	if (rc < 0)
		DIE_PERROR("send failed");
}

struct nlcn_recv_msg {
	struct nlmsghdr nl_hdr;
	struct {
		struct cn_msg cn_msg;
		struct proc_event proc_ev;
	} __attribute__ ((__packed__));
} __attribute__ ((aligned(NLMSG_ALIGNTO)));

static void handle_proc_ev(int nl_fd)
{
	struct nlcn_recv_msg nlcn_msg;
	int rc;

	while (1) {
		rc = recv(nl_fd, &nlcn_msg, sizeof(nlcn_msg), 0);
		if (!rc) {
			/* shutdown? */
			return;
		} else if (rc < 0) {
			if (errno == EINTR)
				continue;
			DIE_PERROR("receive failed");
		}

		switch (nlcn_msg.proc_ev.what) {
		case PROC_EVENT_FORK:
			DEBUG("fork: parent tid=%d pid=%d -> child tid=%d pid=%d\n",
			//fprintf(stderr, "fork: parent tid=%d pid=%d -> child tid=%d pid=%d\n",
				nlcn_msg.proc_ev.event_data.fork.parent_pid,
				nlcn_msg.proc_ev.event_data.fork.parent_tgid,
				nlcn_msg.proc_ev.event_data.fork.child_pid,
				nlcn_msg.proc_ev.event_data.fork.child_tgid);
			bm_set(nlcn_msg.proc_ev.event_data.fork.child_pid);
			create_hash_entry(nlcn_msg.proc_ev.event_data.fork.child_pid, nlcn_msg.proc_ev.event_data.fork.child_tgid);
			atomic_inc(&nr_threads);
			break;
		case PROC_EVENT_EXIT:
			DEBUG("exit: tid=%d pid=%d exit_code=%d\n",
			//fprintf(stderr, "exit: tid=%d pid=%d exit_code=%d\n",
				nlcn_msg.proc_ev.event_data.exit.process_pid,
				nlcn_msg.proc_ev.event_data.exit.process_tgid,
				nlcn_msg.proc_ev.event_data.exit.exit_code);
			// TODO: do not remove imeediately, a seperate exit event will follow containing the exit time,
			// only remove the pid after that one was given out...
			bm_clear(nlcn_msg.proc_ev.event_data.exit.process_pid);
			remove_hash_entry(nlcn_msg.proc_ev.event_data.exit.process_pid);
			atomic_dec(&nr_threads);
			break;
			/* TODO: is PROC_EVENT_COREDUMP also an exit event? */
			/* ignore all others */
		default:
			break;
		}
	}
}

/* Note: errors may happen here since the process may be already gone */
static int scan_procfs_threads(int pid)
{
	int len, tid, count = 0;
	struct dirent *dentry;
	char name[30];
	DIR *dir;

	memset(name, 0, 30);
	snprintf(name, 30, "/proc/%d/task", pid);
	dir = opendir(name);
	if (!dir)
		return 0;

	while ((dentry = readdir(dir)) != NULL) {
                if (dentry->d_name[0] == '.')
                        continue;
                len = strlen(dentry->d_name);
                if (!len)
                        continue;
                /* we're only interested in tid files */
                if (!isdigit(dentry->d_name[0]))
                        DIE("invalid file found");
		tid = atoi(dentry->d_name);
		bm_set(tid);
		create_hash_entry(tid, pid);
		count++;
	}
	closedir(dir);
	return count;
}

static void *scan_procfs(void *unused)
{
	struct dirent *dentry;
	int len, threads;
	DIR *dir;

	dir = opendir("/proc");
	if (!dir)
		DIE_PERROR("opendir failed");

	while ((dentry = readdir(dir)) != NULL) {
                if (dentry->d_name[0] == '.')
                        continue;
                len = strlen(dentry->d_name);
                if (!len)
                        continue;
                /* we're only interested in pid files */
                if (!isdigit(dentry->d_name[0]))
                        continue;
		threads = scan_procfs_threads(atoi(dentry->d_name));
		atomic_add(threads, &nr_threads);
	}
	DEBUG("Initial threads found: %d\n", atomic_read(&nr_threads));
	closedir(dir);
	pthread_exit(NULL);
}

void *proc_events_main(void *unused)
{
	int nl_fd, rc;

	nl_fd = setup_connector();
	set_proc_ev_listen(nl_fd, true);

	/* now get all existing tasks out of proc in parallel */
	rc = pthread_create(&procfs_thread, NULL, scan_procfs, NULL);
	if (rc)
		DIE_PERROR("pthread_create failed");
	pthread_setname_np(procfs_thread, "nlmon-scan");

	/* endless loop */
	handle_proc_ev(nl_fd);

	set_proc_ev_listen(nl_fd, false);
	bm_destroy();
	pthread_exit(NULL);
}
