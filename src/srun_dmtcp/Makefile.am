# Makefile for srun_dmtcp

AUTOMAKE_OPTIONS = foreign
CLEANFILES = core.*

if WITH_DMTCP

AM_CPPFLAGS = -I$(top_srcdir) $(DMTCP_CPPFLAGS)

bin_PROGRAMS = srun_dmtcp

srun_dmtcp_SOURCES = srun_dmtcp.c

convenience_libs = $(top_builddir)/src/api/libslurm.o $(DL_LIBS)

srun_dmtcp_LDADD = $(convenience_libs) $(DMTCP_LIBS)

srun_dmtcp_LDFLAGS = -export-dynamic $(CMD_LDFLAGS) $(DMTCP_LDFLAGS)

force:
$(convenience_libs) : force
	@cd `dirname $@` && $(MAKE) `basename $@`

else

EXTRA_srun_dmtcp_SOURCES = srun_dmtcp.c

endif
