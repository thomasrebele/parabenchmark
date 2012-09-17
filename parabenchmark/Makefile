#############
# variables #
#############
CFLAGS=
ENV=
BIN=bin$(ENV)
OBJ=obj$(ENV)
LOG=log$(ENV)
SOURCE=.

CC=gcc -std=gnu99 -lm -lrt -lpthread
RUNCMD=
MPICMD=mpiexec
release-mpi debug-mpi: CC=mpicc -std=gnu99 -lrt -lpthread
debug-mpi: CFLAGS=-DCOMPILE_WITH_MPI -g

GITREF=echo "\#define GIT_REF \"`git show-ref refs/heads/master | cut -d " " -f 1`\"" > git_ref.h
MPIREF=$(CC) mpi_ref.c -E 2>&1 | grep "^\#.*mpi\.h\" 1$$" | head -n 1 | awk '{ print "\#define MPI_INCLUDE_PATH " $$3; }' > mpi_ref.h

NP=
ARG=
ARGDEFAULT=--output-dir $(LOG)

PROG=main
LIST=$(addprefix $(BIN)/, $(PROG))
SUFFIX=

AUX_MPI_=mpi_benchmark.o mpi_functions.o
AUX_MPI=$(addprefix $(OBJ)/, $(AUX_MPI_))
BENCHMARKS=memory_benchmark.o pthread_benchmark.o speedup_benchmark.o
AUXILIARY=timer.o statistics.o getopt.o print_functions.o system_info.o nested_for.o pthread_functions.o range.o parse.o
OBJFILES_=main.o $(AUXILIARY) $(BENCHMARKS)
OBJFILES=$(addprefix $(OBJ)/, $(OBJFILES_))

compile: directories $(LIST)
release: CFLAGS=-O0
release: compile 

release-mpi: CFLAGS=-DCOMPILE_WITH_MPI -O3
.SECONDEXPANSION:
release-mpi: $(OBJFILES) $(AUX_MPI) $(OBJ)/main.o
	$(CC) $(CFLAGS) $^ -o $(BIN)/$(PROG)$(SUFFIX)

#######
# run #
#######
run: release batch
batch:
	$(RUNCMD) $(BIN)/$(PROG) $(ARGDEFAULT) $(ARG)

run-mpi: release-mpi batch-mpi
batch-mpi:
	$(MPICMD) $(NP) $(BIN)/$(PROG) $(ARGDEFAULT) $(ARG)

#########
# debug #
#########
debug: SUFFIX=_debug
debug: CC += -g
debug: compile

.SECONDEXPANSION:
debug-mpi: SUFFIX=_debug
debug-mpi: $(OBJFILES) $(AUX_MPI) $(OBJ)/main.o
	$(CC) $(CFLAGS) $^ -g -o $(BIN)/$(PROG)$(SUFFIX)

#############
# run debug #
#############
gdb: SUFFIX=_debug
gdb: debug
	(echo r ; cat) | gdb --args $(BIN)/$(PROG)$(SUFFIX) $(ARGDEFAULT) $(ARG)

gdb-mpi: SUFFIX=_debug
gdb-mpi: debug-mpi
	$(MPICMD) $(NP) xterm -e gdb --args $(BIN)/$(PROG)$(SUFFIX) $(ARGDEFAULT) $(ARG)

run-mpi-debug: debug-mpi
	$(MPICMD) $(NP) $(BIN)/$(PROG)$(SUFFIX) $(ARGDEFAULT) $(ARG)

#############
# shortcuts #
#############
plot: release
	$(BIN)/$(PROG) --plot-data --max-repetition-time 3.5 --step-threshold-time 0.08

test-mpi: ARG=--run-tests mpi-bandwidth -ffalse --iterations=0-2147483647 $(TESTARG)
test-mpi: batch-mpi

test: ARG=--run-tests pthread-loop[int] -ffalse --iterations=5
test: release run

###################
# compiling rules #
###################
.SECONDEXPANSION:
$(LIST) :  $(OBJFILES) $(OBJ)/$$(@F).o
	$(CC) $(CFLAGS) $^ -o $@$(SUFFIX)

$(OBJ)/%.o: $(SOURCE)/%.h
$(OBJ)/%.o: $(SOURCE)/%.c
	@$(GITREF)
	@$(MPIREF)
	$(CC) $(CFLAGS) -c $^ -o $@

directories:
	mkdir -p $(BIN)
	mkdir -p $(OBJ)
	mkdir -p $(LOG)

############
# valgrind #
############
VALGRINDARG=
.PHONY:
valgrind: SUFFIX=_debug
valgrind: debug
	valgrind --tool=memcheck --main-stacksize=167772160 $(VALGRINDARG) bin/$(PROG)$(SUFFIX) $(ARGDEFAULT) $(ARG)
.PHONY:
valgrind-ptrcheck: SUFFIX=_debug
valgrind-ptrcheck: debug
	valgrind --tool=ptrcheck $(VALGRINDARG) bin/$(PROG)$(SUFFIX) $(ARGDEFAULT) $(ARG)

.PHONY:
valgrind-mpi-local: MPICMD=mpiexec $(NP)
valgrind-mpi-local: release-mpi
	LD_PRELOAD=/usr/lib/valgrind/libmpiwrap-amd64-linux.so $(MPICMD) valgrind $(BIN)/$(PROG) $(ARGDEFAULT) $(ARG)

.PHONY:
valgrind-mpi-lrz: release-mpi
	LD_PRELOAD=/lrz/sys/tools/valgrind/3.5.0/lib/valgrind/libmpiwrap-amd64-linux.so $(MPICMD) valgrind $(BIN)/$(PROG) $(ARGDEFAULT) $(ARG)

#########
# clean #
#########
.PHONY: clean
clean:
	-rm -f $(LIST) $(OBJ)/*.o

.PHONY: clean-log
clean-log:
	rm -f log/*.log
