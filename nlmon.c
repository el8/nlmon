/*
 * Copyright Penguin Boost, 2016
 * Author(s): Jan Glauber <jan.glauber@gmail.com>
 *
 * Netlink based process monitor.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <ctype.h>
#include <limits.h>
#include <getopt.h>

#include <linux/netlink.h>
#include <linux/genetlink.h>

#define COMP "nlmon"
#include "helper.h"
#include "hash.h"
#include "bitmap.h"
#include "nlmon.h"

#ifdef DEBUG_ENABLED
FILE *logfile;
#endif

static int opt_realtime = 0;

/* default intervall is one second */
struct timespec target = { 1, 0 };

/* sync intervall is one second */
struct timespec ts_sync = { 1, 0 };

/* Generic macros for dealing with netlink sockets */
#define GENLMSG_DATA(glh)       ((void *)(NLMSG_DATA(glh) + GENL_HDRLEN))
#define GENLMSG_PAYLOAD(glh)    (NLMSG_PAYLOAD(glh, 0) - GENL_HDRLEN)
#define NLA_DATA(na)            ((void *)((char*)(na) + NLA_HDRLEN))
#define NLA_PAYLOAD(len)        (len - NLA_HDRLEN)

static int nl_fd;
static int nl_id;
static char *nl_cpumask;

static int current_query;

extern struct output_operations oops_stdout;
extern struct output_operations oops_csv;
extern struct output_operations oops_ncurses;
extern struct output_operations oops_nop;

enum sort_options opt_sort = OPT_SORT_TIME;

/* Maximum size of response requested or message sent */
#define MAX_MSG_SIZE    1024

struct msgtemplate {
	struct nlmsghdr n;
	struct genlmsghdr g;
	char buf[MAX_MSG_SIZE];
};

static int send_cmd(int sd, __u16 nlmsg_type, __u32 nlmsg_pid,
	     __u8 genl_cmd, __u16 nla_type,
	     void *nla_data, int nla_len)
{
	struct nlattr *na;
	struct sockaddr_nl nladdr;
	int r, buflen;
	char *buf;

	struct msgtemplate msg;

	msg.n.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
	msg.n.nlmsg_type = nlmsg_type;
	msg.n.nlmsg_flags = NLM_F_REQUEST;
	msg.n.nlmsg_seq = 0;
	msg.n.nlmsg_pid = nlmsg_pid;
	msg.g.cmd = genl_cmd;
	msg.g.version = 0x1;
	na = (struct nlattr *) GENLMSG_DATA(&msg);
	na->nla_type = nla_type;
	na->nla_len = nla_len + 1 + NLA_HDRLEN;
	memcpy(NLA_DATA(na), nla_data, nla_len);
	msg.n.nlmsg_len += NLMSG_ALIGN(na->nla_len);

	buf = (char *) &msg;
	buflen = msg.n.nlmsg_len ;
	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;
	while ((r = sendto(sd, buf, buflen, 0, (struct sockaddr *) &nladdr,
			   sizeof(nladdr))) < buflen) {
		if (r > 0) {
			buf += r;
			buflen -= r;
		} else if (errno != EAGAIN)
			return -1;
	}
	return 0;
}

static int get_family_id(int sd)
{
	struct {
		struct nlmsghdr n;
		struct genlmsghdr g;
		char buf[256];
	} ans;

	char name[100];
	int id = 0, rc;
	struct nlattr *na;
	int rep_len;

	memset(name, 0, 100);
	strcpy(name, TASKSTATS_GENL_NAME);
	rc = send_cmd(sd, GENL_ID_CTRL, getpid(), CTRL_CMD_GETFAMILY,
			CTRL_ATTR_FAMILY_NAME, (void *)name,
			strlen(TASKSTATS_GENL_NAME)+1);
	if (rc < 0)
		return 0;	/* sendto() failure? */

	rep_len = recv(sd, &ans, sizeof(ans), 0);
	if (ans.n.nlmsg_type == NLMSG_ERROR ||
	    (rep_len < 0) || !NLMSG_OK((&ans.n), rep_len))
		return 0;

	na = (struct nlattr *) GENLMSG_DATA(&ans);
	na = (struct nlattr *) ((char *) na + NLA_ALIGN(na->nla_len));
	if (na->nla_type == CTRL_ATTR_FAMILY_ID) {
		id = *(__u16 *) NLA_DATA(na);
	}
	return id;
}

static int setup_netlink(void)
{
	int rc, id, rcvbufsz = 0;
	struct sockaddr_nl nla;
	socklen_t len;

	nl_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
	if (nl_fd < 0)
		DIE_PERROR("netlink socket failed");

	memset(&nla, 0, sizeof(nla));
	nla.nl_family = AF_NETLINK;

	rc = bind(nl_fd, (struct sockaddr *) &nla, sizeof(struct sockaddr_nl));
	if (rc < 0)
		DIE_PERROR("netlink bind failed");

	len = sizeof(int);
	if (getsockopt(nl_fd, SOL_SOCKET, SO_RCVBUF, &rcvbufsz, &len) < 0)
		fprintf(stderr, "Unable to get socket rcv buf size\n");
	else
		DEBUG("receive buffer size: %d\n", rcvbufsz);

	id = get_family_id(nl_fd);
	if (!id)
		DIE_PERROR("Error getting family id");

	nl_id = id;
	DEBUG("family id %d\n", nl_id);
	return 0;
}

static void handle_async_event(struct taskstats *t, int pid)
{
	fprintf(stderr, "Exit record for PID: %5d [%s]  exitcode: %d\n",
		pid,
		t->ac_comm,
		t->ac_exitcode
		);
	fprintf(stderr, "utime: %llu  stime: %llu\n", t->ac_utime, t->ac_stime);

}

static void __gather_data_sanity(struct hash_entry *h, struct taskstats *t)
{
	// stupid missing memset, can be removed later
	if (h->utime > t->ac_utime)
		DIE("invalid utime  value old: %llu  new: %llu\n", h->utime, t->ac_utime);
	if (h->stime > t->ac_stime)
		DIE("invalid stime  value old: %llu  new: %llu\n", h->stime, t->ac_stime);
	if (h->io_rd_bytes > t->read_char)
		DIE("invalid io read value old: %llu  new: %llu\n", h->io_rd_bytes, (unsigned long long) t->read_char);
	if (h->io_wr_bytes > t->write_char)
		DIE("invalid io write value old: %llu  new: %llu\n", h->io_wr_bytes, (unsigned long long) t->write_char);
	if (h->blkio_delay > t->blkio_delay_total)
		DIE("invalid blkio delay total value old: %llu  new: %llu\n", h->blkio_delay, t->blkio_delay_total);
	if (h->cpu_delay > t->cpu_delay_total)
		DIE("invalid cpu delay total value old: %llu  new: %llu\n", h->cpu_delay, t->cpu_delay_total);
}

static int output_wanted(struct taskstat_delta *delta)
{
	return  delta->utime ||
		delta->stime ||
		delta->rss ||
		delta->io_rd_bytes ||
		delta->io_wr_bytes ||
		delta->blkio_delay ||
		delta->cpu_delay;
}

static int once;

static void gather_data(struct taskstats *t)
{
	struct taskstat_delta *delta;
	struct hash_entry *h;

	if (!nr_cycles && !once) {
		ts_version = t->version;
		ts_size = sizeof(*t);
		output->print_banner(t);
		once = 1;
	}

	h = get_hash_entry(t->ac_pid);
	if (!h)
		DIE("hash entry missing for process %d!", t->ac_pid);
	if (t->ac_pid != h->tid)
		DIE("pid mismatch in hash!");

	// XXX this sucks, optimize later
	delta = malloc(sizeof(struct taskstat_delta));
	if (!delta)
		DIE_PERROR("malloc failed");
	memset(delta, 0, sizeof(struct taskstat_delta));
	delta->pid = h->tgid;
	delta->tid = t->ac_pid;

	__gather_data_sanity(h, t);

	delta->utime = t->ac_utime - h->utime;
	delta->stime = t->ac_stime - h->stime;
	delta->cpu_delay = t->cpu_delay_total - h->cpu_delay;
	delta->rss = t->coremem - h->rss;
	delta->io_rd_bytes = t->read_char - h->io_rd_bytes;
	delta->io_wr_bytes = t->write_char - h->io_wr_bytes;
	delta->blkio_delay = t->blkio_delay_total - h->blkio_delay;

	/* store new values */
	h->utime = t->ac_utime;
	h->stime = t->ac_stime;
	h->cpu_delay = t->cpu_delay_total;
	h->rss = t->coremem;
	h->io_rd_bytes = t->read_char;
	h->io_wr_bytes = t->write_char;
	h->blkio_delay = t->blkio_delay_total;

	put_hash_entry(t->ac_pid);

	if (t->ac_exitcode)
		DEBUG("exiting task: %d [%s]\n", t->ac_pid, t->ac_comm);

	current_sum_utime += delta->utime / 1000;
	current_sum_stime += delta->stime / 1000;

	if (!nr_cycles)
		return;

	/* only output if one value changed! */
	if (output_wanted(delta)) {
		// XXX optimize later, maybe pointer to task string in hash entry?
		memcpy(&delta->comm, t->ac_comm, TS_COMM_LEN);
		cache_add(delta);
	}
}

static int query_task(int victim)
{
	int cmd_type = TASKSTATS_CMD_ATTR_PID;
	//int cmd_type = TASKSTATS_CMD_ATTR_TGID;
	int rc, tid = victim;

	current_query = victim;
	// XXX optimize getpid away
	rc = send_cmd(nl_fd, nl_id, getpid(), TASKSTATS_CMD_GET,
		      cmd_type, &tid, sizeof(unsigned int));
	if (rc < 0) {
		fprintf(stderr, "error sending tid/tgid cmd\n");
		return -1;
	}
	DEBUG("query: sent pid/tgid %d, retval %d\n", victim, rc);
	return 0;
}

static void receive_taskstats(void)
{
	int rep_len, len2, aggr_len;
	int len = 0, count = 0, resolved = 0;
	struct msgtemplate msg;
	struct nlattr *na;
	pid_t rtid = 0;

	do {
		DEBUG("record: %d  ", count);
		rep_len = recv(nl_fd, &msg, sizeof(msg), 0);

		if (rep_len < 0) {
			fprintf(stderr, "nonfatal reply error: errno %d\n", errno);
			continue;
		}
		if (msg.n.nlmsg_type == NLMSG_ERROR || !NLMSG_OK((&msg.n), rep_len)) {
			struct nlmsgerr *err = NLMSG_DATA(&msg);
			fprintf(stderr, "fatal reply error,  errno %d\n", err->error);
			return;
		}

		DEBUG("nlmsghdr size=%zu, nlmsg_len=%d, rep_len=%d\n",
			sizeof(struct nlmsghdr), msg.n.nlmsg_len, rep_len);

		rep_len = GENLMSG_PAYLOAD(&msg.n);
		na = (struct nlattr *) GENLMSG_DATA(&msg);
		len = 0;
		while (len < rep_len) {
			len += NLA_ALIGN(na->nla_len);
			switch (na->nla_type) {
			case TASKSTATS_TYPE_NULL:
				break;
			case TASKSTATS_TYPE_AGGR_TGID:
			case TASKSTATS_TYPE_AGGR_PID:
				aggr_len = NLA_PAYLOAD(na->nla_len);
				len2 = 0;
				/* For nested attributes, na follows */
				na = (struct nlattr *) NLA_DATA(na);
				while (len2 < aggr_len) {
					switch (na->nla_type) {
					case TASKSTATS_TYPE_PID:
						rtid = *(int *) NLA_DATA(na);
						if (rtid == current_query)
							resolved = 1;
						DEBUG("receive: rtid PID\t%d\n", rtid);
						break;
					case TASKSTATS_TYPE_TGID:
						rtid = *(int *) NLA_DATA(na);
						if (rtid == current_query)
							resolved = 1;
						fprintf(stderr, "rtid TGID\t%d\n", rtid);
						break;
					case TASKSTATS_TYPE_STATS:
						count++;
						if (rtid == current_query)
							gather_data((struct taskstats *) NLA_DATA(na));
						else
							handle_async_event((struct taskstats *) NLA_DATA(na), rtid);
						break;
					default:
						fprintf(stderr, "Unknown nested nla_type %d\n",
							na->nla_type);
						break;
					}
					len2 += NLA_ALIGN(na->nla_len);
					na = (struct nlattr *) ((char *) na + len2);
				}
				break;
			default:
				fprintf(stderr, "Unknown nla_type %d\n", na->nla_type);
			}
			na = (struct nlattr *) (GENLMSG_DATA(&msg) + len);
		}
	} while (!resolved);
}

static void timespec_delta(const struct timespec *start, const struct timespec *end, struct timespec *res)
{
	if ((end->tv_nsec - start->tv_nsec) < 0) {
		res->tv_sec = end->tv_sec - start->tv_sec - 1;	// WTF
		res->tv_nsec = 1000000000 + end->tv_nsec - start->tv_nsec;
	} else {
		res->tv_sec = end->tv_sec - start->tv_sec;
		res->tv_nsec = end->tv_nsec - start->tv_nsec;
	}
}

static void query_tasks(void)
{
	int pid;

	for (pid = 0; pid < PID_MAX; pid++)
		if (bm_test(pid)) {
			query_task(pid);
			receive_taskstats();
		}
}

void wait_for_cycle_end(struct timespec *sleep)
{
	struct timespec remain;
	int rc;

resume:
	rc = nanosleep(sleep, &remain);
	if (rc < 0 && errno == EINTR) {
		fprintf(stderr, "resuming interrupted sleep\n");
		*sleep = remain;
		goto resume;
	}
	if (rc < 0)
		DIE_PERROR("nanosleep failed");
}

static void print_tasks(void)
{
	struct taskstat_delta *delta = NULL;

	for (;;) {
		delta = cache_walk(delta);
		if (delta)
			output->print_data(delta);
		else
			break;
	}
	cache_flush();
}

static void measure_one_cycle(void)
{
	struct timespec ts1, ts2, delta, sleep;
	int rc;

	current_sum_utime = 0;
	current_sum_stime = 0;
	current_sum_cpu_utime = 0;
	current_sum_cpu_stime = 0;

	if (nr_cycles)
		output->print_cycle_start();
	else
		output->print_sync();
	new_cycle = 1;

	rc = clock_gettime(CLOCK_MONOTONIC, &ts1);
	if (rc < 0)
		DIE_PERROR("clock_gettime failed");

	query_tasks();
	query_memory();
	query_cpus();

	print_tasks();
	print_memory();
	print_cpus();

	rc = clock_gettime(CLOCK_MONOTONIC, &ts2);
	if (rc < 0)
		DIE_PERROR("clock_gettime failed");

	timespec_delta(&ts1, &ts2, &delta);
	if (nr_cycles)
		output->print_cycle_end(&delta);

	/* check if we can meet the target measurement interval */
	if (delta.tv_sec > target.tv_sec ||
	    (delta.tv_sec == target.tv_sec && delta.tv_nsec > target.tv_nsec)) {
		output->exit_output();
		DIE("Target measurement intervall too small. Current overhead: %u seconds %lu ms\n",
			(int) delta.tv_sec, delta.tv_nsec / NSECS_PER_MSEC);
	}

	/* now go sleeping for the rest of the measurement interval */
	if (nr_cycles)
		timespec_delta(&delta, &target, &sleep);
	else
		timespec_delta(&delta, &ts_sync, &sleep);

	wait_for_cycle_end(&sleep);
	nr_cycles++;
}

static void setup_cpumask(void)
{
	nl_cpumask = malloc(20);	/* enough for "0-4096" :) */
	if (nl_cpumask < 0)
		DIE_PERROR("malloc failed");
	memset(nl_cpumask, 0, 20);
	snprintf(nl_cpumask, 20, "0-%d", nr_cpus - 1);
}

static void start_task_monitor(void)
{
	int rc;

	setup_cpumask();
	DEBUG("Starting task life cycle monitor on CPUs %s\n", nl_cpumask);

	rc = send_cmd(nl_fd, nl_id,
		      getpid(),
		      TASKSTATS_CMD_GET, TASKSTATS_CMD_ATTR_REGISTER_CPUMASK,
		      nl_cpumask, strlen(nl_cpumask) + 1);
	if (rc < 0)
		DIE("send cmd failed with error %d\n", rc);
}

static void stop_task_monitor(void)
{
	send_cmd(nl_fd, nl_id, getpid(), TASKSTATS_CMD_GET,
		 TASKSTATS_CMD_ATTR_DEREGISTER_CPUMASK,
		 nl_cpumask, strlen(nl_cpumask) + 1);
}

/* elevate to maximum realtime priority :) */
static void elevate_prio(void)
{
	struct sched_param sp;
	FILE *cgroup_fp;
	pid_t us;
	int rc;

	us = getpid();

	/* if cgroups are present add us otherwise we don't have permission to enable RT */
	cgroup_fp = fopen("/sys/fs/cgroup/cpu/tasks", "w");
	if (!cgroup_fp)
		goto nop;

	rc = fprintf(cgroup_fp, "%d", us);
	if (rc < 0)
		DIE_PERROR("fprintf failed");
	fclose(cgroup_fp);

nop:
	sp.sched_priority = sched_get_priority_max(SCHED_RR);
	if (sp.sched_priority < 0)
		DIE_PERROR("sched_get_priority_max failed");
	rc = sched_setscheduler(0, SCHED_RR, &sp);
	if (rc < 0)
		DIE_PERROR("sched_setscheduler failed");
}

static void print_help(int argc, char* argv[])
{
	fprintf(stderr, "Usage: %s [options]\n", argv[0]);
	fprintf(stderr, "  where options are:\n");
	fprintf(stderr, "  -o <mode> or --output <mode>\n");
	fprintf(stderr, "      Modes: stdout, csv, ncurses\n");
	fprintf(stderr, "  -s <mode> or --sort <mode>\n");
	fprintf(stderr, "      Modes: id, name, time, io, mem\n");
	fprintf(stderr, "  --realtime\n");
	fprintf(stderr, "  --seconds <seconds>\n");
	fprintf(stderr, "  --milliseconds <milliseconds>\n");
	fprintf(stderr, "  -c <cycles> or --cycles <cycles>\n");
	fprintf(stderr, "  -h or --help\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
	pthread_t proc_events_thread;
	void *status;
	int rc, opt, cycles = INT_MAX;

#ifdef DEBUG_ENABLED
	logfile = fopen(DEBUG_LOGFILE, "w");
	if (logfile == NULL)
		DIE_PERROR("Cannot open file " DEBUG_LOGFILE " for writing");
#endif

#ifdef CONFIG_NCURSES
	/* default is ncurses output */
	output = &oops_ncurses;
#else
	output = &oops_stdout;
#endif

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "realtime",	no_argument,		&opt_realtime, 1},
			{ "sort",	required_argument,	0,  's'},
			{ "output",	required_argument,	0,  'o'},
			{ "seconds",	required_argument,	0,  't' },
			{ "milliseconds",required_argument,	0,  'm' },
			{ "cycles",	required_argument,	0,  'c' },
			{ "help",	no_argument,		0,  'h' },
			{ 0, 0, 0, 0 },
		};

		opt = getopt_long(argc, argv, "s:o:c:h?", long_options, &option_index);
		if (opt == -1)
			break;

		switch (opt) {
		case 'o':
			if (strcmp(optarg, "csv") == 0)
				output = &oops_csv;
			else if (strcmp(optarg, "stdout") == 0)
				output = &oops_stdout;
#ifdef CONFIG_NCURSES
			else if (strcmp(optarg, "ncurses") == 0)
				output = &oops_ncurses;
#endif
			else if (strcmp(optarg, "nop") == 0)
				output = &oops_nop;
			else { /* unknown */
				fprintf(stderr, "Unknown output method %s\n", optarg);
				print_help(argc, argv);
			}
			break;
		case 's':
			if (strcmp(optarg, "name") == 0)
				opt_sort = OPT_SORT_NAME;
			else if (strcmp(optarg, "id") == 0)
				opt_sort = OPT_SORT_TID;
			else if (strcmp(optarg, "time") == 0)
				opt_sort = OPT_SORT_TIME;
			else if (strcmp(optarg, "io") == 0)
				opt_sort = OPT_SORT_IO;
			else if (strcmp(optarg, "mem") == 0)
				opt_sort = OPT_SORT_MEM;
			else { /* unknown */
				fprintf(stderr, "Unknown sort method %s\n", optarg);
				print_help(argc, argv);
			}
			break;
		case 't':
			target.tv_sec = atoi(optarg);
			break;
		case 'm':
			target.tv_nsec = atol(optarg) * NSECS_PER_MSEC;
			break;
		case 'c':
			cycles = atoi(optarg);
			break;
		case 0:
			break;
		case '?':
		case 'h':
		default:
			print_help(argc, argv);
		}
	}

	nr_cpus = get_nr_cpus();

	bm_alloc(PID_MAX);
	rc = pthread_create(&proc_events_thread, NULL, proc_events_main, NULL);
	if (rc)
		DIE_PERROR("pthread_create failed");
	pthread_setname_np(proc_events_thread, "nlmon-pevent");

	setup_netlink();
	start_task_monitor();

	while (!procfs_thread) {
		DEBUG("...\n");
		pthread_yield();
		__sync_synchronize();
	}

	rc = pthread_join(procfs_thread, &status);
	if (rc)
		DIE_PERROR("pthread_join failed");
	else
		DEBUG("procfs scan thread exited\n");

	data_init_cpu();
	if (opt_realtime)
		elevate_prio();

	cache_init();
	output->init_output();
	while (cycles--)
		measure_one_cycle();
	output->exit_output();
	stop_task_monitor();
	exit(EXIT_SUCCESS);
}
