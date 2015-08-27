#!/bin/sh

checkpoint_path=$SLURM_CHECKPOINT_IMAGE_DIR/$PMI_JOBID
checkpoint_port=$SLURM_CHECKPOINT_PORT

type dmtcp_launch >/dev/null 2>&1 || { echo >&2 "I require dmtcp_launch but it is not installed or not present in \$PATH.  Aborting."; exit 1; }
dmtcp_launch --ckptdir $checkpoint_path -p $checkpoint_port $@
