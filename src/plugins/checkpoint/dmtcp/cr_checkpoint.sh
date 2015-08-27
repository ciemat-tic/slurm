#!/bin/sh
image_dir=`dirname $2`
mkdir -p $image_dir
#exec @dmtcp_HOME@/bin/cr_checkpoint -T $1 -f $2

exec dmtcp_command  -p  $SLURM_CHECKPOINT_PORT c
