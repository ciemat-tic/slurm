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
//static void  _set_prio_process_env(void);
//static void  _set_submit_dir_env(void);
//static void updateMinJobAge (char* source, int min_job_age);

int main(int argc, char *argv[])
{

	/*********BEGINING OF EXECUTABLE INITIALIZATION ************/


	log_options_t logopt = LOG_OPTS_STDERR_ONLY;
	char *script_name;
	void *script_body;
	int script_size = 0;
	int errorCode = 0;

	slurm_conf_init(NULL);
	log_init(xbasename(argv[0]), logopt, 0, NULL);

	_set_exit_code();
	if (spank_init_allocator() < 0) {
		error("Failed to initialize plugin stack");
		exit(error_exit);
	}

	if (argc < 2) {
		error("this program require input parameters. Execute it with \"--help\" for a complete list of options");
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


	/*VERIFICATION OF INPUT DATA */

	printf ("Slurm task migration\n\n");
	printf("slurm jobid: %d\n", opt.jobid);

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
	if (( errorCode = slurm_checkpoint_able( opt.jobid, opt.stepid, &start_time)) != SLURM_SUCCESS){
		slurm_perror ("Job is not checkpointable\n");
		//printf ("it should be finish here, but I'll employ the same code for test\n");
		exit(errorCode);
	}

	node_info_msg_t *node_access = NULL;
	if (( errorCode = slurm_load_node_single(&node_access, opt.node, show_flags)) != 0) {
		slurm_perror ("Specified node does not exist.\n");
		exit(errorCode);
	}
	node_info_t node = node_access->node_array[0];
	if (node.node_state !=	NODE_STATE_IDLE) {
		printf ("Node should be iddle and ready to be used.\n");
		exit(-1);
	}

	


/*
	//NODE RESERVATION
    	resv_desc_msg_t         resv_msg;
	slurm_init_resv_desc_msg ( &resv_msg );
	resv_msg.start_time = time(NULL) + 1;  
	resv_msg.duration = job_info.time_limit;
	//uint32_t node_cnt = 1;
	//resv_msg.node_cnt = &node_cnt;
	resv_msg.node_list = opt.node; //TODO this reserves just one node. We have to think what to do with parallel jobs
	resv_msg.users = "root"; //TODO this has to be taken from the job. The problem is that job_info.user_id gives the Id, and i don't know how to get the user name from that

	char * resv_name; 
	resv_name = slurm_create_reservation (&resv_msg);
	if (!resv_name) {
		 slurm_perror ("slurm_create_reservation error");
		 exit (1);
	}
	printf("Node %s is reserved with name %s, waiting for the migration.\n", node.name, resv_name);
	//aqui hay que meter el reservation


	// free the node information response message
	slurm_free_node_info_msg(node_access);
*/

	printf("Checkpointing code\n");
	char* checkpoint_directory = "/home/slurm";



	printf("Saving status of running task\n");
	if (( errorCode = slurm_checkpoint_vacate (opt.jobid, opt.stepid, 0,checkpoint_directory )) != 0){
		slurm_perror ("there was an error calling slurm_checkpoint_vacate. Error:");
		exit(errorCode);
	 }


	//make sure that the job has been purged
	printf("Waiting system to update internal info\n");
	while (job_info.job_state  == JOB_RUNNING){
		if (( errorCode =  slurm_purge (0, opt.jobid)) != 0){
			slurm_perror ("there was an error calling slurm_purge. Error:");
			exit(errorCode);
		 }
		if (slurm_load_job (&job_ptr, opt.jobid, show_flags) != SLURM_SUCCESS ){
			printf ("job does not exist, so it has been checkpointed and removed frmo the system\n");
			break;
		}
		job_info = job_ptr->job_array[job_ptr->record_count-1];
		sleep(1);
	}



	printf("Restarting checkpoint\n");

	int i = 0;
	//while ( slurm_checkpoint_restart(opt.jobid, opt.stepid, 0,  checkpoint_directory) != 0) {
	while ( slurm_checkpoint_migrate(opt.jobid , opt.stepid, opt.node, checkpoint_directory) != 0){
		//slurm_perror ("there was an error calling slurm_checkpoint_migrate. Error:");
		sleep (i++);
		}

	printf("Job has been migrated\n");



/*
	//RESERVATION 2
	//this is probably a bad idea. I think it is better to create a reservation with a short life, so there is 
	//no need of deleting it. The basic problem is that the deletion seems to require that no job is running on that reservation...

	if ((errorCode = slurm_delete_reservation (resv_name)) != 0) {
		slurm_perror ("slurm_delete_reservation");
		exit(errorCode);
	}
	free(resv_name);

*/
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




/*
static void updateMinJobAge (char* source, int min_job_age){

	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	char tmpFileName[99] = "/tmp/tmp_slurm.conf";
	int errorCode = 0;

	FILE *source_file, *target_file;
	source_file = fopen(source, "r");
	target_file = fopen(tmpFileName, "w");

	while ((read = getline(&line, &len, source_file)) != -1) {
		if (strncmp(line, "MinJobAge", strlen("MinJobAge")) == 0){
			char newString[99];
			sprintf(newString, "MinJobAge=%d\n", min_job_age);
			fprintf(target_file, newString);
		}
		else {
			fprintf(target_file, line);
		}
	}


	fclose(source_file);
	fclose(target_file);


	source_file = fopen(tmpFileName, "r");
	target_file = fopen(source, "w");

	while ((read = getline(&line, &len, source_file)) != -1) {
		fprintf(target_file, line);
	}
	fclose(source_file);
	fclose(target_file);


	if (( errorCode = unlink (tmpFileName)) != 0){
		printf("there was an error deleting tmp file %s ", tmpFileName);
		exit(errorCode);
	 }



	if (( errorCode = slurm_reconfigure()) != 0){
		slurm_perror ("there was an error calling slurm_reconfigure. Error:");
		exit(errorCode);
	 }


}

*/

