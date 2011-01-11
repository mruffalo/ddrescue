#! /bin/sh
# check script for GNU ddrescue - Data recovery tool
# Copyright (C) 2009, 2010, 2011 Antonio Diaz Diaz.
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
cat "${testdir}"/test1.txt > in1 || framework_failure
cat "${testdir}"/test2.txt > in2 || framework_failure
cat "${testdir}"/logfile1 > logfile1 || framework_failure
cat "${testdir}"/logfile2 > logfile2 || framework_failure
fail=0

"${DDRESCUE}" -t -pq -i15000 in out logfile > /dev/null || fail=1
"${DDRESCUE}" -D -fn -s15000 in out logfile > /dev/null || fail=1
cmp in out || fail=1
printf .

rm out || framework_failure
rm logfile || framework_failure
"${DDRESCUE}" -R -i15000 in out logfile > /dev/null || fail=1
"${DDRESCUE}" -R -s15000 in out logfile > /dev/null || fail=1
cmp in out || fail=1
printf .

rm out || framework_failure
"${DDRESCUE}" -F+ -o15000 in out2 logfile > /dev/null || fail=1
"${DDRESCUE}" -RS -i15000 -o0 out2 out > /dev/null || fail=1
cmp in out || fail=1
printf .

printf "garbage" >> out || framework_failure
"${DDRESCUE}" -Rt -i15000 -o0 out2 out > /dev/null || fail=1
cmp in out || fail=1
printf .

rm out || framework_failure
"${DDRESCUE}" -m logfile1 in out > /dev/null || fail=1
cmp in1 out || fail=1
printf .
"${DDRESCUE}" -m logfile2 in out > /dev/null || fail=1
cmp in out || fail=1
printf .

rm out || framework_failure
"${DDRESCUE}" -Rm logfile2 in out > /dev/null || fail=1
cmp in2 out || fail=1
printf .
"${DDRESCUE}" -Rm logfile1 in out > /dev/null || fail=1
cmp in out || fail=1
printf .

rm out || framework_failure
cat logfile1 > logfile || framework_failure
"${DDRESCUE}" in out logfile > /dev/null || fail=1
cat logfile2 > logfile || framework_failure
"${DDRESCUE}" in out logfile > /dev/null || fail=1
cmp in out || fail=1
printf .

rm out || framework_failure
cat logfile1 > logfile || framework_failure
"${DDRESCUE}" -R in out logfile > /dev/null || fail=1
cat logfile2 > logfile || framework_failure
"${DDRESCUE}" -R in out logfile > /dev/null || fail=1
cmp in out || fail=1
printf .

cat in1 > out || framework_failure
rm logfile || framework_failure
"${DDRESCUE}" -g in out logfile > /dev/null || fail=1
"${DDRESCUE}" in2 out logfile > /dev/null || fail=1
cmp in out || fail=1
printf .

cat in2 > out || framework_failure
rm logfile || framework_failure
"${DDRESCUE}" -g -s34816 in out logfile > /dev/null || fail=1
"${DDRESCUE}" -R in1 out logfile > /dev/null || fail=1
cmp in out || fail=1
printf .

echo
if [ ${fail} = 0 ] ; then
	echo "tests completed successfully."
	cd "${objdir}" && rm -r tmp
else
	echo "tests failed."
fi
exit ${fail}
