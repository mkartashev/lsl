#!/bin/bash

handler()
{
	cd "$ROOT/.."
	echo "Caught a signal, exiting..."
	exit 0
}

run_one()
{
	local script="`basename "$1"`"
	local tdir="$TESTDIR/$script"
	mkdir -p $tdir
	[ $? -ne 0 ] && return 1

	pushd $tdir >/dev/null
	ln -s "$ROOT/$script" $script

	./$script > run.log 2>&1
	local rc=$?

	popd >/dev/null
	return $rc
}

TESTS=`find tests/ -type f -executable | sort`

LS=`pwd`/ls
[ ! -x $LS ] && echo "$LS not executable, exiting..." && exit 1
echo "Will run tests for $LS"
export LS

export ROOT="`pwd`/tests/"

# Make testrun more stable and less dependent on the environment
export _LS_TESTRUN=1
umask 00002

trap "handler()" 2

TESTDIR="RUN.$$"
[ -d "$TESTDIR" ] && echo "Testrun directory $TESTDIR not empty, exiting..." && exit 1

echo "Creating testrun directory $TESTDIR..."
mkdir "$TESTDIR"


let good=0
let bad=0
for T in $TESTS; do
	TNAME="$TESTDIR/`basename $T`"
	echo -n "Running	$TNAME		"
	run_one $T
	if [ $? -ne 0 ]; then
		echo "FAILED"
		let bad++
	else
		echo "SUCCESS"
		let good++
	fi
done

echo
echo "Done running tests in $TESTDIR"
echo "Success: $good"
echo "Failed: $bad"

