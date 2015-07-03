#! /bin/sh
# check script for GNU ddrescue - Data recovery tool
# Copyright (C) 2009-2015 Antonio Diaz Diaz.
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

if [ ! -f "${DDRESCUE}" ] || [ ! -x "${DDRESCUE}" ] ; then
	echo "${DDRESCUE}: cannot execute"
	exit 1
fi

if [ -d tmp ] ; then rm -rf tmp ; fi
mkdir tmp
cd "${objdir}"/tmp

in="${testdir}"/test.txt
in1="${testdir}"/test1.txt
in2="${testdir}"/test2.txt
in3="${testdir}"/test3.txt
in4="${testdir}"/test4.txt
in5="${testdir}"/test5.txt
blank="${testdir}"/blockfile_blank
bf1="${testdir}"/blockfile1
bf2="${testdir}"/blockfile2
bf2i="${testdir}"/blockfile2i
bf3="${testdir}"/blockfile3
bf4="${testdir}"/blockfile4
bf5="${testdir}"/blockfile5
fail=0

printf "testing ddrescue-%s..." "$2"

"${DDRESCUE}" -q ${in}
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q ${in} out blockfile extra
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q ${in} ${in} blockfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q ${in} out ${in}
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q ${in} out out
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -F- ${in} out
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -F ${in} out blockfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -F- --ask ${in} out blockfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -G --ask ${in} out blockfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -G ${in} out
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -F- -G ${in} out blockfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -H ${bf2i} ${in} out blockfile
if [ $? = 2 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -K ${in} out
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -K, ${in} out
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -K0, ${in} out
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -K0,65535 ${in} out
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -i 0, ${in} out
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -i -1 ${in} out
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -m ${bf1} -m ${bf2} ${in} out blockfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -m ${bf2i} ${in} out blockfile
if [ $? = 2 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -w ${in} out blockfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q --cpass=1, ${in} out
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q --cpass=4 ${in} out
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi

rm -f blockfile
"${DDRESCUE}" -q -t -p -i15000 ${in} out blockfile || fail=1
"${DDRESCUE}" -q -y -f -n -s15000 ${in} out blockfile || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
rm -f blockfile
"${DDRESCUE}" -q -R -i15000 ${in} out blockfile || fail=1
"${DDRESCUE}" -q -R -s15000 --cpass=3 ${in} out blockfile || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
"${DDRESCUE}" -q -F+ -o15000 ${in} out2 blockfile || fail=1
"${DDRESCUE}" -q -R -S -i15000 -o0 --unidirectional out2 out || fail=1
cmp ${in} out || fail=1
printf .

printf "garbage" >> out || framework_failure
"${DDRESCUE}" -q -R -t -i15000 -o0 out2 out || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
"${DDRESCUE}" -q -O -H ${bf1} ${in} out || fail=1
cmp ${in1} out || fail=1
printf .
"${DDRESCUE}" -q -O -L -K0 -c1 -H ${bf2i} ${in2} out || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
"${DDRESCUE}" -q -c1 -H ${bf3} ${in3} out || fail=1
"${DDRESCUE}" -q -c2 -H ${bf4} ${in4} out || fail=1
"${DDRESCUE}" -q -H ${bf5} ${in5} out || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
"${DDRESCUE}" -q -X -m ${bf1} ${in} out || fail=1
cmp ${in1} out || fail=1
printf .
"${DDRESCUE}" -q -X -L -m ${bf2i} ${in2} out || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
"${DDRESCUE}" -q -R -m ${bf2} ${in} out || fail=1
cmp ${in2} out || fail=1
printf .
"${DDRESCUE}" -q -R -K,64KiB -m ${bf1} ${in1} out || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
"${DDRESCUE}" -q -m ${bf5} ${in5} out || fail=1
"${DDRESCUE}" -q -m ${bf4} ${in4} out || fail=1
"${DDRESCUE}" -q -m ${bf3} ${in3} out || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
cat ${bf1} > blockfile || framework_failure
"${DDRESCUE}" -q -I ${in2} out blockfile || fail=1
cat ${bf2} > blockfile || framework_failure
"${DDRESCUE}" -q -I ${in} out blockfile || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
cat ${bf1} > blockfile || framework_failure
"${DDRESCUE}" -q -R ${in2} out blockfile || fail=1
cat ${bf2} > blockfile || framework_failure
"${DDRESCUE}" -q -R -C ${in1} out blockfile || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
fail2=0
for i in 0 8000 16000 24000 32000 ; do
	"${DDRESCUE}" -q -i${i} -s4000 -m ${bf1} ${in} out || fail2=1
done
cmp -s ${in} out && fail2=1
for i in 4000 12000 20000 28000 36000 ; do
	"${DDRESCUE}" -q -i${i} -s4000 -m ${bf1} ${in} out || fail2=1
done
cmp ${in1} out || fail2=1
for i in 0 8000 16000 24000 32000 ; do
	"${DDRESCUE}" -q -i${i} -s4000 -m ${bf2} ${in2} out || fail2=1
done
cmp -s ${in} out && fail2=1
for i in 4000 12000 20000 28000 36000 ; do
	"${DDRESCUE}" -q -i${i} -s4000 -m ${bf2} ${in2} out || fail2=1
done
cmp ${in} out || fail2=1
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

rm -f blockfile
cat ${in1} > out || framework_failure
"${DDRESCUE}" -q -G ${in} out blockfile || fail=1
"${DDRESCUE}" -q ${in2} out blockfile || fail=1
cmp ${in} out || fail=1
printf .

rm -f blockfile
cat ${in} > copy || framework_failure
printf "garbage" >> copy || framework_failure
cat ${in2} > out || framework_failure
"${DDRESCUE}" -q -t -x 36388 ${in1} copy || fail=1
"${DDRESCUE}" -q -G ${in} out blockfile || fail=1
"${DDRESCUE}" -q -R -T1.5d copy out blockfile || fail=1
cmp ${in} out || fail=1
printf .

printf "\ntesting ddrescuelog-%s..." "$2"

"${DDRESCUELOG}" -q blockfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -q -d
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -q -l+l ${bf1}
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -q -t -d blockfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -q -m ${bf2i} -t blockfile
if [ $? = 2 ] ; then printf . ; else printf - ; fail=1 ; fi

"${DDRESCUELOG}" -a '?,+' -i3072 ${bf1} > blockfile
"${DDRESCUELOG}" -D blockfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -a '?,+' -i2048 -s1024 blockfile > blockfile2
"${DDRESCUELOG}" -d blockfile2
if [ $? = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

"${DDRESCUELOG}" -b2048 -l+ ${bf1} > out || fail=1
"${DDRESCUELOG}" -b2048 -f -c blockfile < out || fail=1
"${DDRESCUELOG}" -b2048 -l+ blockfile > copy || fail=1
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -q -p ${bf1} blockfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -P ${bf1} blockfile || fail=1
printf .
"${DDRESCUELOG}" -b2048 -s36388 -f -c?+ blockfile < out || fail=1
"${DDRESCUELOG}" -p ${bf2} blockfile || fail=1
printf .
"${DDRESCUELOG}" -b2048 -f -c?+ blockfile < out || fail=1
"${DDRESCUELOG}" -s36388 -p ${bf2} blockfile || fail=1
printf .
"${DDRESCUELOG}" -q -s36389 -p ${bf2} blockfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi

printf "10\n12\n14\n16\n" | "${DDRESCUELOG}" -b2048 -f -c+? blockfile || fail=1
"${DDRESCUELOG}" -q -p blockfile ${bf1}
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -q -i0x5000 -p blockfile ${bf1}
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -i0x5000 -s0x3800 -p blockfile ${bf1} || fail=1
printf .

"${DDRESCUELOG}" -C ${bf2i} > blockfile || fail=1
"${DDRESCUELOG}" -p ${bf2} blockfile || fail=1
printf .

cat ${bf1} > blockfile || framework_failure
"${DDRESCUELOG}" -i1024 -s2048 -d blockfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -i1024 -s1024 -d blockfile || fail=1
printf .
"${DDRESCUELOG}" -q -i1024 -s1024 -d blockfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi

cat ${bf2} > blockfile || framework_failure
"${DDRESCUELOG}" -m ${bf1} -D blockfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -L -m ${bf2i} -D blockfile || fail=1
printf .
"${DDRESCUELOG}" -i1024 -s2048 -d blockfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -i2048 -s2048 -d blockfile || fail=1
printf .

"${DDRESCUELOG}" -b2048 -l+ ${bf1} > out || fail=1
printf "0\n2\n4\n6\n8\n10\n12\n14\n16\n" > copy || framework_failure
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -b2048 -l?- ${bf1} > out || fail=1
printf "1\n3\n5\n7\n9\n11\n13\n15\n17\n" > copy || framework_failure
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -b2048 -l+ -i0x1800 -o0 -s0x4000 ${bf1} > out || fail=1
printf "1\n3\n5\n7\n" > copy || framework_failure
cmp out copy || fail=1
printf .

"${DDRESCUELOG}" -n ${bf2} > blockfile || framework_failure
"${DDRESCUELOG}" -b2048 -l+ blockfile > out || fail=1
printf "0\n2\n4\n6\n8\n10\n12\n14\n16\n" > copy || framework_failure
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -b2048 -l?- blockfile > out || fail=1
printf "1\n3\n5\n7\n9\n11\n13\n15\n17\n" > copy || framework_failure
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -b2048 -l+ -i2048 -o0 -s0x4000 blockfile > out || fail=1
printf "1\n3\n5\n7\n" > copy || framework_failure
cmp out copy || fail=1
printf .

"${DDRESCUELOG}" -q -P ${bf2i} ${bf2}
if [ $? = 2 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -L -P ${bf2i} ${bf2} || fail=1
printf .

fail2=0			# test XOR
for i in ${bf1} ${bf2} ${bf3} ${bf4} ${bf5} ; do
	for j in ${bf1} ${bf2} ${bf3} ${bf4} ${bf5} ; do
		"${DDRESCUELOG}" -x ${j} ${i} > out || fail2=1
		"${DDRESCUELOG}" -x ${i} ${j} > copy || fail2=1
		"${DDRESCUELOG}" -P out copy || fail2=1
		"${DDRESCUELOG}" -x ${j} out > copy || fail2=1
		"${DDRESCUELOG}" -P ${i} copy || fail2=1
	done
done
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0
"${DDRESCUELOG}" -x ${bf1} ${bf2} > out || fail2=1
"${DDRESCUELOG}" -x ${bf2} ${bf1} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
"${DDRESCUELOG}" -d out || fail2=1
"${DDRESCUELOG}" -d copy || fail2=1
"${DDRESCUELOG}" -x ${bf1} ${blank} > out || fail2=1
"${DDRESCUELOG}" -x ${blank} ${bf1} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
"${DDRESCUELOG}" -p out ${bf1} || fail2=1
"${DDRESCUELOG}" -p ${bf1} copy || fail2=1
"${DDRESCUELOG}" -x ${bf2} ${bf2} > blockfile || fail2=1
"${DDRESCUELOG}" -P ${blank} blockfile || fail2=1
"${DDRESCUELOG}" -x ${bf1} ${bf1} > blockfile || fail2=1
"${DDRESCUELOG}" -P ${blank} blockfile || fail2=1
"${DDRESCUELOG}" -b2048 -l+ ${bf1} > out || fail2=1
"${DDRESCUELOG}" -b2048 -l- blockfile > copy || fail2=1
cmp out copy || fail2=1
"${DDRESCUELOG}" -b2048 -i0x2000 -s0x2800 -l+ ${bf1} > out || fail2=1
"${DDRESCUELOG}" -i0x1800 -s0x3800 -x ${bf1} ${bf1} > blockfile || fail2=1
"${DDRESCUELOG}" -b2048 -l- blockfile > copy || fail2=1
cmp out copy || fail2=1
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0
"${DDRESCUELOG}" -x ${bf3} ${bf4} > out || fail2=1
"${DDRESCUELOG}" -x ${bf4} ${bf3} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
"${DDRESCUELOG}" -x ${bf3} ${bf5} > out || fail2=1
"${DDRESCUELOG}" -x ${bf5} ${bf3} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
"${DDRESCUELOG}" -x ${bf4} ${bf5} > out || fail2=1
"${DDRESCUELOG}" -x ${bf5} ${bf4} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0
"${DDRESCUELOG}" -x ${bf3} ${bf4} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -x out ${bf5} > blockfile || fail2=1
"${DDRESCUELOG}" -d blockfile || fail2=1

"${DDRESCUELOG}" -x ${bf3} ${bf5} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -x out ${bf4} > blockfile || fail2=1
"${DDRESCUELOG}" -d blockfile || fail2=1

"${DDRESCUELOG}" -x ${bf4} ${bf3} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -x out ${bf5} > blockfile || fail2=1
"${DDRESCUELOG}" -d blockfile || fail2=1

"${DDRESCUELOG}" -x ${bf4} ${bf5} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -x out ${bf3} > blockfile || fail2=1
"${DDRESCUELOG}" -d blockfile || fail2=1

"${DDRESCUELOG}" -x ${bf5} ${bf3} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -x out ${bf4} > blockfile || fail2=1
"${DDRESCUELOG}" -d blockfile || fail2=1

"${DDRESCUELOG}" -x ${bf5} ${bf4} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -x out ${bf3} > blockfile || fail2=1
"${DDRESCUELOG}" -d blockfile || fail2=1
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0			# test AND
for i in ${bf1} ${bf2} ${bf3} ${bf4} ${bf5} ; do
	for j in ${bf1} ${bf2} ${bf3} ${bf4} ${bf5} ; do
		"${DDRESCUELOG}" -y ${j} ${i} > out || fail2=1
		"${DDRESCUELOG}" -y ${i} ${j} > copy || fail2=1
		"${DDRESCUELOG}" -P out copy || fail2=1
	done
done
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0
"${DDRESCUELOG}" -b2048 -l+ ${bf1} > out || fail2=1
"${DDRESCUELOG}" -y ${bf1} ${bf2} > blockfile || fail2=1
"${DDRESCUELOG}" -P ${blank} blockfile || fail2=1
"${DDRESCUELOG}" -b2048 -l? blockfile > copy || fail2=1
cmp out copy || fail2=1
"${DDRESCUELOG}" -y ${bf2} ${bf1} > blockfile || fail2=1
"${DDRESCUELOG}" -P ${blank} blockfile || fail2=1
"${DDRESCUELOG}" -b2048 -l- blockfile > copy || fail2=1
cmp out copy || fail2=1
"${DDRESCUELOG}" -b2048 -i0x2000 -s0x2800 -l+ ${bf1} > out || fail2=1
"${DDRESCUELOG}" -i0x1800 -s0x3800 -y ${bf2} ${bf1} > blockfile || fail2=1
"${DDRESCUELOG}" -b2048 -l- blockfile > copy || fail2=1
cmp out copy || fail2=1
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0
"${DDRESCUELOG}" -y ${bf3} ${bf4} > out || fail2=1
"${DDRESCUELOG}" -P ${blank} out || fail2=1
"${DDRESCUELOG}" -y ${bf3} ${bf5} > out || fail2=1
"${DDRESCUELOG}" -P ${blank} out || fail2=1
"${DDRESCUELOG}" -y ${bf4} ${bf5} > out || fail2=1
"${DDRESCUELOG}" -P ${blank} out || fail2=1
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0
"${DDRESCUELOG}" -i0x2000 -s0x2800 -z ${bf2} ${bf1} > blockfile || fail2=1
"${DDRESCUELOG}" -D blockfile
if [ $? != 1 ] ; then fail2=1 ; fi
"${DDRESCUELOG}" -i0x1C00 -s0x2C00 -D blockfile
if [ $? != 1 ] ; then fail2=1 ; fi
"${DDRESCUELOG}" -i0x2000 -s0x2C00 -D blockfile
if [ $? != 1 ] ; then fail2=1 ; fi
"${DDRESCUELOG}" -i0x2000 -s0x2800 -d blockfile
if [ $? != 0 ] ; then fail2=1 ; fi
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0			# test OR
for i in ${bf1} ${bf2} ${bf3} ${bf4} ${bf5} ; do
	for j in ${bf1} ${bf2} ${bf3} ${bf4} ${bf5} ; do
		"${DDRESCUELOG}" -z ${j} ${i} > out || fail2=1
		"${DDRESCUELOG}" -z ${i} ${j} > copy || fail2=1
		"${DDRESCUELOG}" -P out copy || fail2=1
	done
done
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0
"${DDRESCUELOG}" -z ${bf1} ${bf2} > out || fail2=1
"${DDRESCUELOG}" -z ${bf2} ${bf1} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
"${DDRESCUELOG}" -d out || fail2=1
"${DDRESCUELOG}" -d copy || fail2=1
"${DDRESCUELOG}" -z ${bf1} ${blank} > out || fail2=1
"${DDRESCUELOG}" -z ${blank} ${bf1} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
"${DDRESCUELOG}" -p out ${bf1} || fail2=1
"${DDRESCUELOG}" -p ${bf1} copy || fail2=1
"${DDRESCUELOG}" -z ${bf3} ${bf4} > out || fail2=1
"${DDRESCUELOG}" -z ${bf4} ${bf3} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
"${DDRESCUELOG}" -z ${bf3} ${bf5} > out || fail2=1
"${DDRESCUELOG}" -z ${bf5} ${bf3} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
"${DDRESCUELOG}" -z ${bf4} ${bf5} > out || fail2=1
"${DDRESCUELOG}" -z ${bf5} ${bf4} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0
"${DDRESCUELOG}" -z ${bf3} ${bf4} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -z out ${bf5} > blockfile || fail2=1
"${DDRESCUELOG}" -d blockfile || fail2=1

"${DDRESCUELOG}" -z ${bf3} ${bf5} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -z out ${bf4} > blockfile || fail2=1
"${DDRESCUELOG}" -d blockfile || fail2=1

"${DDRESCUELOG}" -z ${bf4} ${bf3} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -z out ${bf5} > blockfile || fail2=1
"${DDRESCUELOG}" -d blockfile || fail2=1

"${DDRESCUELOG}" -z ${bf4} ${bf5} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -z out ${bf3} > blockfile || fail2=1
"${DDRESCUELOG}" -d blockfile || fail2=1

"${DDRESCUELOG}" -z ${bf5} ${bf3} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -z out ${bf4} > blockfile || fail2=1
"${DDRESCUELOG}" -d blockfile || fail2=1

"${DDRESCUELOG}" -z ${bf5} ${bf4} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -z out ${bf3} > blockfile || fail2=1
"${DDRESCUELOG}" -d blockfile || fail2=1
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0			# test ( a && b ) == !( !a || !b )
for i in ${bf1} ${bf2} ${bf3} ${bf4} ${bf5} ; do
	for j in ${bf1} ${bf2} ${bf3} ${bf4} ${bf5} ; do
		"${DDRESCUELOG}" -n ${i} > na || fail2=1
		"${DDRESCUELOG}" -n ${j} > nb || fail2=1
		"${DDRESCUELOG}" -z nb na > out || fail2=1
		"${DDRESCUELOG}" -n out > copy || fail2=1
		"${DDRESCUELOG}" -y ${j} ${i} > out || fail2=1
		"${DDRESCUELOG}" -P out copy || fail2=1
	done
done
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

echo
if [ ${fail} = 0 ] ; then
	echo "tests completed successfully."
	cd "${objdir}" && rm -r tmp
else
	echo "tests failed."
fi
exit ${fail}
