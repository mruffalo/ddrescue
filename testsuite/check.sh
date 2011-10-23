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
DDRESCUELOG="${objdir}"/ddrescuelog
framework_failure() { echo "failure in testing framework" ; exit 1 ; }

if [ ! -x "${DDRESCUE}" ] ; then
	echo "${DDRESCUE}: cannot execute"
	exit 1
fi

if [ -d tmp ] ; then rm -rf tmp ; fi
mkdir tmp
cd "${objdir}"/tmp

cat "${testdir}"/test.txt > in || framework_failure
cat "${testdir}"/test1.txt > in1 || framework_failure
cat "${testdir}"/test2.txt > in2 || framework_failure
cat "${testdir}"/logfile1 > logfile1 || framework_failure
cat "${testdir}"/logfile2 > logfile2 || framework_failure
fail=0

printf "testing ddrescue-%s..." "$2"
"${DDRESCUE}" -q in
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUE}" -q -g in out
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUE}" -q -F- in out
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUE}" -q -F in out logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUE}" -q -F- -g in out logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi

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

printf "\ntesting ddrescuelog-%s..." "$2"
"${DDRESCUELOG}" -q logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUELOG}" -q -d
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUELOG}" -q -t -d logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi

"${DDRESCUELOG}" -b2048 -l+ logfile1 > out || fail=1
cat out | "${DDRESCUELOG}" -b2048 -fc logfile || fail=1
"${DDRESCUELOG}" -b2048 -l+ logfile > copy || fail=1
cmp out copy || fail=1
printf .
cat out | "${DDRESCUELOG}" -b2048 -s35744 -fc?+ logfile || fail=1
"${DDRESCUELOG}" -p logfile2 logfile || fail=1
printf .
cat out | "${DDRESCUELOG}" -b2048 -fc?+ logfile || fail=1
"${DDRESCUELOG}" -s35744 -p logfile2 logfile || fail=1
printf .
"${DDRESCUELOG}" -s35745 -q -p logfile2 logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi

cat logfile1 > logfile || framework_failure
"${DDRESCUELOG}" -i1024 -s2048 -d logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUELOG}" -i1024 -s1024 -d logfile || fail=1
printf .
"${DDRESCUELOG}" -i1024 -s1024 -d -q logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi

cat logfile2 > logfile || framework_failure
"${DDRESCUELOG}" -m logfile1 -D logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUELOG}" -m logfile2 -D logfile || fail=1
printf .
"${DDRESCUELOG}" -i1024 -s2048 -d logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUELOG}" -i2048 -s2048 -d logfile || fail=1
printf .

cat logfile1 > logfile || framework_failure
"${DDRESCUELOG}" -b2048 -l+ logfile > out || fail=1
printf "0\n2\n4\n6\n8\n10\n12\n14\n16\n" > copy || framework_failure
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -b2048 -l?- logfile > out || fail=1
printf "1\n3\n5\n7\n9\n11\n13\n15\n17\n" > copy || framework_failure
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -b2048 -l+ -i6KiB -o0 -s16KiB logfile > out || fail=1
printf "1\n3\n5\n7\n" > copy || framework_failure
cmp out copy || fail=1
printf .

"${DDRESCUELOG}" -n logfile2 > logfile || framework_failure
"${DDRESCUELOG}" -b2048 -l+ logfile > out || fail=1
printf "0\n2\n4\n6\n8\n10\n12\n14\n16\n" > copy || framework_failure
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -b2048 -l?- logfile > out || fail=1
printf "1\n3\n5\n7\n9\n11\n13\n15\n17\n" > copy || framework_failure
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -b2048 -l+ -i2048 -o0 -s16KiB logfile > out || fail=1
printf "1\n3\n5\n7\n" > copy || framework_failure
cmp out copy || fail=1
printf .

"${DDRESCUELOG}" -b2048 -l+ logfile1 > out || fail=1
"${DDRESCUELOG}" -x logfile1 logfile1 > logfile || fail=1
"${DDRESCUELOG}" -b2048 -l- logfile > copy || fail=1
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -y logfile2 logfile1 > logfile || fail=1
"${DDRESCUELOG}" -b2048 -l- logfile > copy || fail=1
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -z logfile1 logfile2 > logfile || fail=1
"${DDRESCUELOG}" -d logfile || fail=1
printf .

echo
if [ ${fail} = 0 ] ; then
	echo "tests completed successfully."
	cd "${objdir}" && rm -r tmp
else
	echo "tests failed."
fi
exit ${fail}
