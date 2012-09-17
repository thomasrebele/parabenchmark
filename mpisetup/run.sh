#!/bin/bash

if [ -z $testcmd ]; then mpi_test_cmd=./a.out; fi
if [ -z $repetitions ]; then
	repetitions=100;
fi
echo "# repetitions: $repetitions"
if [ -z $ntaskspernodemax ]; then ntaskspernodemax=4; fi

source ./statistic.sh
source ./print_function.sh

is_fn() { [ `type -t $1`"" == 'function' ]; }
echoerr() { echo "$@" 1>&2; }

get_nodes_n() {
	local out=$($par_cmd echo "+ 1" 2>&1)
	local err=$(echo "$out" | grep -i error)
	if [ ! -z "$err" ]; then
		echo 1;
	else
		expr $($par_cmd echo "+ 1");
	fi
}

if ! is_fn mpi_fn; then
	if [ -z "$mpi_cmd" ]; then mpi_cmd="srun_ps"; fi
	par_cmd="srun"
	if [ -z $tasks ]; then
		tasks=$(get_nodes_n);
	fi
	mpi_fn () {
		local cmd="$mpi_cmd -n $tasks $1"
		if [ $# -le 1 ] || [ $2 -le 1 ]; then
			echo "# execute cmd $cmd" >&2
		fi
		LANG=C local out="$($cmd 2>&1)"
		LANG=C local error="$(echo \"$out\" | grep -i 'error\|fatal')"
		if [ ! -z "$error" ]; then 
			echo "# error: $error" >&2;
			return 1;
		fi
		echo "$out"
		return 0;
	}
fi
nodes=$(get_nodes_n)

mpi_setup_test () {
	print "repetitions, " "$repetitions, "
	for ((r=1;r<=$repetitions;r++))
	do
		startrep=$(date +%s%N)
		outputrep=$(mpi_fn "bin/$testcmd" $r) || exit
		endrep=$(date +%s%N)
		
		runtimerep=$(echo "scale=$scale; ($endrep - $startrep) / 1000000000" | bc)
		inittimerep=$(echo "$outputrep" | grep Init | average)
		finalizetimerep=$(echo "$outputrep" | grep Finalize | average)
		
		if [ ! -z "$runtimerep" ] && [ ! -z "$inittimerep" ] && [ ! -z $finalizetimerep ]
		then 
			statistic runtimerep
			statistic inittimerep
			statistic finalizetimerep
		fi
	done
	print "nodes, " "$nodes, "
	strtasks=$(printf "%3d" $tasks)
	print "tasks, " "$strtasks, "
	LANG=C tpn=$(printf "%s" $(echo "scale=0; $tasks / $nodes" | bc))
	print "tasks per node, " "$tpn, "
	
	stat=$(statistic runtimerep mean deviation)
	print "runtime mean, runtime deviation, " "$stat"
	
	stat=$(statistic inittimerep mean deviation)
	print "init mean, init deviation, " "$stat"
	
	stat=$(statistic finalizetimerep mean deviation)
	print "finalize mean, finalize deviation, " "$stat"
	
	print line
}

start_mpi_setup_test() {
	if [ -z $nodes ]; then nodes=1; fi
	if [ -z $ntaskspernodemax ]; then ntaskspernodemax=1; fi
	if [ -z $ntasksmin ]; then ntasksmin=$(get_nodes_n); fi
	if [ -z $ntasksmin ]; then ntasksmin=1; fi
	echo \# ntasksmax expr $nodes \* $ntaskspernodemax
	if [ -z $ntasksmax ]; then ntasksmax=$(expr $nodes \* $ntaskspernodemax); fi
	if [ -z $ntasksmax ]; then $ntasksmax=8; fi
	print_header=1
	for ((i=$ntasksmin;i<=$ntasksmax;i*=2))
	do
		export tasks=$i
		mpi_setup_test
	done
}

### print hostnames
echo \# running $mpi_cmd hostname
echo \# date=$(date +%Y-%m-%d-%H-%M)\;
hostnames=$($mpi_cmd bin/${testcmd/run/hostname})
echo \# hostnames=$hostnames\;
echo \# using test command=$testcmd\;

### start tests
start_mpi_setup_test
