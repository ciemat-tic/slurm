/*****************************************************************************\
 *  smigrate.c - Submit a SLURM batch script.$
 *****************************************************************************
 *  Copyright (C) 2006-2007 The Regents of the University of California.
 *  Copyright (C) 2008-2010 Lawrence Livermore National Security.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Christopher J. Morrone <morrone2@llnl.gov>
 *  CODE-OCEC-09-009. All rights reserved.
 *
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://slurm.schedmd.com/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/resource.h> /* for RLIMIT_NOFILE */
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>               /* MAXPATHLEN */
#include <fcntl.h>

#include "slurm/slurm.h"

#include "src/common/env.h"
#include "src/common/plugstack.h"
#include "src/common/read_config.h"
#include "src/common/slurm_rlimits_info.h"
#include "src/common/xstring.h"
#include "src/common/xmalloc.h"

#include "src/smigrate/opt.h"

#define MAX_RETRIES 15

static void  _set_exit_code(void);
static void  _set_prio_process_env(void);
static void  _set_submit_dir_env(void);

int main(int argc, char *argv[])
{

	/*********BEGINING OF EXECUTABLE INITIALIZATION ************/


	log_options_t logopt = LOG_OPTS_STDERR_ONLY;
	job_desc_msg_t desc;
	submit_response_msg_t *resp;
	char *script_name;
	void *script_body;
	int script_size = 0;
	int retries = 0;

	slurm_conf_init(NULL);
	log_init(xbasename(argv[0]), logopt, 0, NULL);

	_set_exit_code();
	if (spank_init_allocator() < 0) {
		error("Failed to initialize plugin stack");
		exit(error_exit);
	}

	/* Be sure to call spank_fini when smigrate exits
	 */
	if (atexit((void (*) (void)) spank_fini) < 0)
		error("Failed to register atexit handler for plugins: %m");

	script_name = process_options_first_pass(argc, argv);
	/* reinit log with new verbosity (if changed by command line) */
	if (opt.verbose || opt.quiet) {
		logopt.stderr_level += opt.verbose;
		logopt.stderr_level -= opt.quiet;
		logopt.prefix_level = 1;
		log_alter(logopt, 0, NULL);
	}

	if (process_options_second_pass(
				(argc),
				argv,
				script_name ? xbasename (script_name) : "stdin",
				script_body, script_size) < 0) {
		error("smigrate parameter parsing");
		exit(error_exit);
	}

	if (spank_init_post_opt() < 0) {
		error("Plugin stack post-option processing failed");
		exit(error_exit);

	}



	/*********END OF INITIALIZATION ************/

	/*********START OF MIGRATION PROCESS ************/

	printf ("Slurm task migration\n\n");
	printf(" slurm jobid: %d\n", opt.jobid);

	job_info_msg_t * job_ptr = NULL;
	uint16_t show_flags = 0;
	if (slurm_load_job (&job_ptr, opt.jobid, show_flags) != SLURM_SUCCESS ){
		printf ("Specified ID does not correspond to an existing Slurm task\n");
		exit(-1);
	}

	slurm_job_info_t job_info = job_ptr->job_array[job_ptr->record_count-1];
	if (job_info.job_state  != JOB_RUNNING) {
		printf ("Job status must be RUNNING to be migrated.\n");
		printf("Assigning jobs to particular nodes is an scheduling problem,"
				" it has to be performed somewhere else\n");
		exit(-1);
	}


	time_t start_time; //if checkpointing is already being performed, the start time is set here
	if (slurm_checkpoint_able( opt.jobid, opt.stepid, &start_time) != SLURM_SUCCESS){
		printf ("Job is not checkpointable\n");
		printf ("it should be feinish here, but I'll employ the same code for test\n");
		//exit(-1);
	}

	node_info_msg_t *node_access = NULL;
	if (slurm_load_node_single(&node_access, opt.node, show_flags) != 0) {
		printf ("Specified node does not exist\n");
		exit(-1);
	}
	node_info_t node = node_access->node_array[0];

	if (node.node_state !=	NODE_STATE_IDLE) {
		printf ("Node should be iddle and ready to be used\n");
		exit(-1);
	}



	else {
		printf("Node %s is iddle and ready to be used\n", node.name);
		exit(-1);
	}

	/*
	 * slurm_free_node_info_msg - free the node information response message
	 * IN msg - pointer to node information response message
	 * NOTE: buffer is loaded by slurm_load_node.
	 */
	//extern void slurm_free_node_info_msg PARAMS(
		//(node_info_msg_t * node_buffer_ptr));

	/*
	 *
	 *
	 */



	exit(0);

	if (opt.test_only) {
		if (slurm_job_will_run(&desc) != SLURM_SUCCESS) {
			slurm_perror("allocation failure");
			exit (1);
		}
		exit (0);
	}

	while (slurm_submit_batch_job(&desc, &resp) < 0) {
		static char *msg;

		if (errno == ESLURM_ERROR_ON_DESC_TO_RECORD_COPY)
			msg = "Slurm job queue full, sleeping and retrying.";
		else if (errno == ESLURM_NODES_BUSY) {
			msg = "Job step creation temporarily disabled, "
			      "retrying";
		} else if (errno == EAGAIN) {
			msg = "Slurm temporarily unable to accept job, "
			      "sleeping and retrying.";
		} else
			msg = NULL;
		if ((msg == NULL) || (retries >= MAX_RETRIES)) {
			error("Batch job submission failed: %m");
			exit(error_exit);
		}

		if (retries)
			debug("%s", msg);
		else if (errno == ESLURM_NODES_BUSY)
			info("%s", msg); /* Not an error, powering up nodes */
		else
			error("%s", msg);
		sleep (++retries);
        }

	xfree(desc.script);
	slurm_free_submit_response_response_msg(resp);
	return 0;
}

static void _set_exit_code(void)
{
	int i;
	char *val = getenv("SLURM_EXIT_ERROR");

	if (val) {
		i = atoi(val);
		if (i == 0)
			error("SLURM_EXIT_ERROR has zero value");
		else
			error_exit = i;
	}
}


/* Set SLURM_SUBMIT_DIR and SLURM_SUBMIT_HOST environment variables within
 * current state */
static void _set_submit_dir_env(void)
{
	char buf[MAXPATHLEN + 1], host[256];

	if ((getcwd(buf, MAXPATHLEN)) == NULL)
		error("getcwd failed: %m");
	else if (setenvf(NULL, "SLURM_SUBMIT_DIR", "%s", buf) < 0)
		error("unable to set SLURM_SUBMIT_DIR in environment");

	if ((gethostname(host, sizeof(host))))
		error("gethostname_short failed: %m");
	else if (setenvf(NULL, "SLURM_SUBMIT_HOST", "%s", host) < 0)
		error("unable to set SLURM_SUBMIT_HOST in environment");
}


/*
 * _set_prio_process_env
 *
 * Set the internal SLURM_PRIO_PROCESS environment variable to support
 * the propagation of the users nice value and the "PropagatePrioProcess"
 * config keyword.
 */
static void  _set_prio_process_env(void)
{
	int retval;

	errno = 0; /* needed to detect a real failure since prio can be -1 */

	if ((retval = getpriority (PRIO_PROCESS, 0)) == -1)  {
		if (errno) {
			error ("getpriority(PRIO_PROCESS): %m");
			return;
		}
	}

	if (setenvf (NULL, "SLURM_PRIO_PROCESS", "%d", retval) < 0) {
		error ("unable to set SLURM_PRIO_PROCESS in environment");
		return;
	}

	debug ("propagating SLURM_PRIO_PROCESS=%d", retval);
}

