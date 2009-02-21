#! /bin/sh
# check script for GNU ddrescue - Data recovery tool
# Copyright (C) 2009 Antonio Diaz Diaz.
#
# This script is free software: you have unlimited permission
# to copy, distribute and modify it.

objdir=`pwd`
testdir=`cd $1 ; pwd`
DDRESCUE=${objdir}/ddrescue
framework_failure() { echo 'failure in testing framework'; exit 1; }

if [ ! -x ${DDRESCUE} ] ; then
	echo "${DDRESCUE}: cannot execute"
	exit 1
fi

if [ -d tmp ] ; then rm -r tmp ; fi
mkdir tmp
echo testing ddrescue...
cd ${objdir}/tmp

cat ${testdir}/../COPYING > in || framework_failure
fail=0

${DDRESCUE} -q -t -i15000 in out1 logfile || fail=1
${DDRESCUE} -q -D -s15000 in out1 logfile || fail=1
${DDRESCUE} -q -F+ -o15000 in out2 logfile || fail=1
${DDRESCUE} -q -S -i15000 -o0 out2 out3 || fail=1
${DDRESCUE} -q -t -m ${testdir}/logfile1 in out2 || fail=1
${DDRESCUE} -q -m ${testdir}/logfile2 in out2 || fail=1
cat ${testdir}/logfile1 > logfile || framework_failure
${DDRESCUE} -q in out4 logfile || fail=1
cat ${testdir}/logfile2 > logfile || framework_failure
${DDRESCUE} -q in out4 logfile || fail=1
cmp in out1 || fail=1
cmp in out2 || fail=1
cmp in out3 || fail=1
cmp in out4 || fail=1

if test ${fail} = 0; then
	echo "tests completed successfully."
	if cd ${objdir} ; then rm -r tmp ; fi
else
	echo "tests failed."
fi
exit ${fail}
