#
# Makefile for src/testbin (sources for programs installed in /testbin)
#

TOP=../..
.include "$(TOP)/mk/os161.config.mk"

SUBDIRS=add argtest badcall bigfile conman crash ctest dirconc dirseek \
	dirtest f_test farm faulter fileonlytest filetest forkbomb forktest guzzle \
	hash hog huge kitchen malloctest matmult palin parallelvm psort \
	randcall rmdirtest rmtest sink sort sty tail tictac triplehuge \
	triplemat triplesort ft1 ft2 ft3 ft4 pt1 pt2 pt3 pt4 pt5

# But not:
#    userthreads    (no support in kernel API in base system)

.include "$(TOP)/mk/os161.subdir.mk"
