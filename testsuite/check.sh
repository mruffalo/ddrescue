#! /bin/sh
# check script for GNU ddrescue - Data recovery tool
# Copyright (C) 2009, 2010 Antonio Diaz Diaz.
#
# This script is free software: you have unlimited permission
# to copy, distribute and modify it.

LC_ALL=C
export LC_ALL
objdir=`pwd`
testdir=`cd "$1" ; pwd`
DDRESCUE="${objdir}"/ddrescue
framework_failure() { echo "failure in testing framework" ; exit 1 ; }

if [ ! -x "${DDRESCUE}" ] ; then
	echo "${DDRESCUE}: cannot execute"
	exit 1
fi

if [ -d tmp ] ; then rm -rf tmp ; fi
mkdir tmp
printf "testing ddrescue-%s..." "$2"
cd "${objdir}"/tmp

cat "${testdir}"/test.txt > in || framework_failure
fail=0

"${DDRESCUE}" -t -p -i15000 in out1 logfile2a > /dev/null || fail=1
"${DDRESCUE}" -D -f -s15000 in out1 logfile2a > /dev/null || fail=1
"${DDRESCUE}" -F+ -o15000 in out2a logfile2a > /dev/null || fail=1
"${DDRESCUE}" -S -i15000 -o0 out2a out2 > /dev/null || fail=1
"${DDRESCUE}" -t -m "${testdir}"/logfile1 in out3 > /dev/null || fail=1
"${DDRESCUE}" -m "${testdir}"/logfile2 in out3 > /dev/null || fail=1
cat "${testdir}"/logfile1 > logfile || framework_failure
"${DDRESCUE}" in out4 logfile > /dev/null || fail=1
cat "${testdir}"/logfile2 > logfile || framework_failure
"${DDRESCUE}" in out4 logfile > /dev/null || fail=1
cmp in out1 || fail=1
printf .
cmp in out2 || fail=1
printf .
cmp in out3 || fail=1
printf .
cmp in out4 || fail=1
printf .

echo
if [ ${fail} = 0 ] ; then
	echo "tests completed successfully."
	cd "${objdir}" && rm -r tmp
else
	echo "tests failed."
fi
exit ${fail}
