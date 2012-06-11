#! /bin/sh
# check script for GNU ddrescue - Data recovery tool
# Copyright (C) 2009, 2010, 2011, 2012 Antonio Diaz Diaz.
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

in="${testdir}"/test.txt
in1="${testdir}"/test1.txt
in2="${testdir}"/test2.txt
logfile1="${testdir}"/logfile1
logfile2="${testdir}"/logfile2
fail=0

printf "testing ddrescue-%s..." "$2"

"${DDRESCUE}" -q ${in}
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUE}" -q ${in} out logfile extra
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUE}" -q ${in} ${in} logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUE}" -q ${in} out ${in}
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUE}" -q ${in} out out
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUE}" -q -g ${in} out
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUE}" -q -F- ${in} out
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUE}" -q -F ${in} out logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUE}" -q -F- -g ${in} out logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUE}" -q -m ${logfile1} -m ${logfile1} ${in} out logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi

if [ -r logfile ] ; then rm logfile || framework_failure ; fi
"${DDRESCUE}" -t -pq -i15000 ${in} out logfile || fail=1
"${DDRESCUE}" -D -fnq -s15000 ${in} out logfile || fail=1
cmp ${in} out || fail=1
printf .

rm out || framework_failure
rm logfile || framework_failure
"${DDRESCUE}" -qR -i15000 ${in} out logfile || fail=1
"${DDRESCUE}" -qR -s15000 ${in} out logfile || fail=1
cmp ${in} out || fail=1
printf .

rm out || framework_failure
"${DDRESCUE}" -qF+ -o15000 ${in} out2 logfile || fail=1
"${DDRESCUE}" -qRS -i15000 -o0 out2 out || fail=1
cmp ${in} out || fail=1
printf .

printf "garbage" >> out || framework_failure
"${DDRESCUE}" -qRt -i15000 -o0 out2 out || fail=1
cmp ${in} out || fail=1
printf .

rm out || framework_failure
"${DDRESCUE}" -q -m ${logfile1} ${in} out || fail=1
cmp ${in1} out || fail=1
printf .
"${DDRESCUE}" -q -m ${logfile2} ${in} out || fail=1
cmp ${in} out || fail=1
printf .

rm out || framework_failure
"${DDRESCUE}" -qRm ${logfile2} ${in} out || fail=1
cmp ${in2} out || fail=1
printf .
"${DDRESCUE}" -qRm ${logfile1} ${in} out || fail=1
cmp ${in} out || fail=1
printf .

rm out || framework_failure
cat ${logfile1} > logfile || framework_failure
"${DDRESCUE}" -qI ${in} out logfile || fail=1
cat ${logfile2} > logfile || framework_failure
"${DDRESCUE}" -qI ${in} out logfile || fail=1
cmp ${in} out || fail=1
printf .

rm out || framework_failure
cat ${logfile1} > logfile || framework_failure
"${DDRESCUE}" -qR ${in} out logfile || fail=1
cat ${logfile2} > logfile || framework_failure
"${DDRESCUE}" -qR ${in} out logfile || fail=1
cmp ${in} out || fail=1
printf .

cat ${in1} > out || framework_failure
rm logfile || framework_failure
"${DDRESCUE}" -qg ${in} out logfile || fail=1
"${DDRESCUE}" -q ${in2} out logfile || fail=1
cmp ${in} out || fail=1
printf .

cat ${in} > copy || framework_failure
printf "garbage" >> copy || framework_failure
cat ${in2} > out || framework_failure
rm logfile || framework_failure
"${DDRESCUE}" -qt -x 35744 ${in1} copy || fail=1
"${DDRESCUE}" -qg ${in} out logfile || fail=1
"${DDRESCUE}" -qR -T1.5d copy out logfile || fail=1
cmp ${in} out || fail=1
printf .

printf "\ntesting ddrescuelog-%s..." "$2"

"${DDRESCUELOG}" -q logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUELOG}" -q -d
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUELOG}" -q -t -d logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi

"${DDRESCUELOG}" -b2048 -l+ ${logfile1} > out || fail=1
cat out | "${DDRESCUELOG}" -b2048 -fc logfile || fail=1
"${DDRESCUELOG}" -b2048 -l+ logfile > copy || fail=1
cmp out copy || fail=1
printf .
cat out | "${DDRESCUELOG}" -b2048 -s35744 -fc?+ logfile || fail=1
"${DDRESCUELOG}" -p ${logfile2} logfile || fail=1
printf .
cat out | "${DDRESCUELOG}" -b2048 -fc?+ logfile || fail=1
"${DDRESCUELOG}" -s35744 -p ${logfile2} logfile || fail=1
printf .
"${DDRESCUELOG}" -s35745 -q -p ${logfile2} logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi

cat ${logfile1} > logfile || framework_failure
"${DDRESCUELOG}" -i1024 -s2048 -d logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUELOG}" -i1024 -s1024 -d logfile || fail=1
printf .
"${DDRESCUELOG}" -i1024 -s1024 -d -q logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi

cat ${logfile2} > logfile || framework_failure
"${DDRESCUELOG}" -m ${logfile1} -D logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUELOG}" -m ${logfile2} -D logfile || fail=1
printf .
"${DDRESCUELOG}" -i1024 -s2048 -d logfile
if [ $? = 0 ] ; then fail=1 ; printf - ; else printf . ; fi
"${DDRESCUELOG}" -i2048 -s2048 -d logfile || fail=1
printf .

cat ${logfile1} > logfile || framework_failure
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

"${DDRESCUELOG}" -n ${logfile2} > logfile || framework_failure
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

"${DDRESCUELOG}" -b2048 -l+ ${logfile1} > out || fail=1
"${DDRESCUELOG}" -x ${logfile1} ${logfile1} > logfile || fail=1
"${DDRESCUELOG}" -b2048 -l- logfile > copy || fail=1
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -y ${logfile2} ${logfile1} > logfile || fail=1
"${DDRESCUELOG}" -b2048 -l- logfile > copy || fail=1
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -z ${logfile1} ${logfile2} > logfile || fail=1
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
