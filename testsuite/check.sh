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
blank="${testdir}"/mapfile_blank
map1="${testdir}"/mapfile1
map2="${testdir}"/mapfile2
map2i="${testdir}"/mapfile2i
map3="${testdir}"/mapfile3
map4="${testdir}"/mapfile4
map5="${testdir}"/mapfile5
fail=0

printf "testing ddrescue-%s..." "$2"

"${DDRESCUE}" -q ${in}
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q ${in} out mapfile extra
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q ${in} ${in} mapfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q ${in} out ${in}
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q ${in} out out
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -F- ${in} out
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -F ${in} out mapfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -F- --ask ${in} out mapfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -G --ask ${in} out mapfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -G ${in} out
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -F- -G ${in} out mapfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -H ${map2i} ${in} out mapfile
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
"${DDRESCUE}" -q -m ${map1} -m ${map2} ${in} out mapfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -m ${map2i} ${in} out mapfile
if [ $? = 2 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q -w ${in} out mapfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q --cpass=1, ${in} out
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUE}" -q --cpass=4 ${in} out
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi

rm -f mapfile
"${DDRESCUE}" -q -t -p -J -b1024 -i15000 ${in} out mapfile || fail=1
"${DDRESCUE}" -q -A -y -e0 -f -n -s15000 ${in} out mapfile || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
rm -f mapfile
"${DDRESCUE}" -q -R -i15000 -a 1k -E0 ${in} out mapfile || fail=1
"${DDRESCUE}" -q -R -s15000 --cpass=3 ${in} out mapfile || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
"${DDRESCUE}" -q -F+ -o15000 -c143 ${in} out2 mapfile || fail=1
"${DDRESCUE}" -q -R -S -i15000 -o0 -u -Z1MiB out2 out || fail=1
cmp ${in} out || fail=1
printf .

printf "garbage" >> out || framework_failure
"${DDRESCUE}" -q -N -R -t -i15000 -o0 --pause=0 out2 out || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
"${DDRESCUE}" -q -O -r1 -H - ${in} out < ${map1} || fail=1
cmp ${in1} out || fail=1
printf .
"${DDRESCUE}" -q -O -L -K0 -c1 -H ${map2i} ${in2} out || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
"${DDRESCUE}" -q -c1 -H ${map3} ${in3} out || fail=1
"${DDRESCUE}" -q -c2 -H ${map4} ${in4} out || fail=1
"${DDRESCUE}" -q -M -H ${map5} ${in5} out || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
"${DDRESCUE}" -q -X -m - ${in} out < ${map1} || fail=1
cmp ${in1} out || fail=1
printf .
"${DDRESCUE}" -q -X -L -m ${map2i} ${in2} out || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
"${DDRESCUE}" -q -R -B -m ${map2} ${in} out || fail=1
cmp ${in2} out || fail=1
printf .
"${DDRESCUE}" -q -R -K,64KiB -m ${map1} ${in1} out || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
"${DDRESCUE}" -q -m ${map5} ${in5} out || fail=1
"${DDRESCUE}" -q -m ${map4} ${in4} out || fail=1
"${DDRESCUE}" -q -m ${map3} ${in3} out || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
cat ${map1} > mapfile || framework_failure
"${DDRESCUE}" -q -I ${in2} out mapfile || fail=1
cat ${map2} > mapfile || framework_failure
"${DDRESCUE}" -q -I ${in} out mapfile || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
cat ${map1} > mapfile || framework_failure
"${DDRESCUE}" -q -R ${in2} out mapfile || fail=1
cat ${map2} > mapfile || framework_failure
"${DDRESCUE}" -q -R -C ${in1} out mapfile || fail=1
cmp ${in} out || fail=1
printf .

rm -f out
fail2=0
for i in 0 8000 16000 24000 32000 40000 48000 56000 64000 72000 ; do
	"${DDRESCUE}" -q -i${i} -s4000 -m ${map1} ${in} out || fail2=1
done
cmp -s ${in} out && fail2=1
for i in 4000 12000 20000 28000 36000 44000 52000 60000 68000 ; do
	"${DDRESCUE}" -q -i${i} -s4000 -m ${map1} ${in} out || fail2=1
done
cmp ${in1} out || fail2=1
for i in 0 8000 16000 24000 32000 40000 48000 56000 64000 72000 ; do
	"${DDRESCUE}" -q -i${i} -s4000 -m ${map2} ${in2} out || fail2=1
done
cmp -s ${in} out && fail2=1
for i in 4000 12000 20000 28000 36000 44000 52000 60000 68000 ; do
	"${DDRESCUE}" -q -i${i} -s4000 -m ${map2} ${in2} out || fail2=1
done
cmp ${in} out || fail2=1
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

rm -f mapfile
cat ${in1} > out || framework_failure
"${DDRESCUE}" -q -G ${in} out mapfile || fail=1
"${DDRESCUE}" -q ${in2} out mapfile || fail=1
cmp ${in} out || fail=1
printf .

rm -f mapfile
cat ${in} > copy || framework_failure
printf "garbage" >> copy || framework_failure
cat ${in2} > out || framework_failure
"${DDRESCUE}" -q -t -x 72776 ${in1} copy || fail=1
"${DDRESCUE}" -q -G ${in} out mapfile || fail=1
"${DDRESCUE}" -q -R -T1.5d copy out mapfile || fail=1
cmp ${in} out || fail=1
printf .

printf "\ntesting ddrescuelog-%s..." "$2"

"${DDRESCUELOG}" -q mapfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -q -d
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -q -l+l ${map1}
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -q -t -d mapfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -q -m ${map2i} -t mapfile
if [ $? = 2 ] ; then printf . ; else printf - ; fail=1 ; fi

"${DDRESCUELOG}" -a '?,+' -i3072 - < ${map1} > mapfile
"${DDRESCUELOG}" -D - < mapfile
r=$?
"${DDRESCUELOG}" -D mapfile
if [ $? = 1 ] && [ $r = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -a '?,+' -i2048 -s1024 mapfile > mapfile2
"${DDRESCUELOG}" -q -d - < mapfile2
r=$?
"${DDRESCUELOG}" -d mapfile2
if [ $? = 0 ] && [ $r = 1 ] ; then printf . ; else printf - ; fail=1 ; fi

"${DDRESCUELOG}" -b2048 -l+ - < ${map1} > out || fail=1
"${DDRESCUELOG}" -b2048 -c - < out > mapfile || fail=1
"${DDRESCUELOG}" -b2048 -l+ mapfile > copy || fail=1
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -q -p ${map1} mapfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -P ${map1} mapfile || fail=1
printf .
"${DDRESCUELOG}" -b2048 -s72776 -f -c?+ mapfile < out || fail=1
"${DDRESCUELOG}" -p ${map2} - < mapfile || fail=1
printf .
"${DDRESCUELOG}" -b2048 -f -c?+ mapfile < out || fail=1
"${DDRESCUELOG}" -s72776 -p ${map2} mapfile || fail=1
printf .
"${DDRESCUELOG}" -q -s72777 -p ${map2} mapfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi

printf "10\n12\n14\n16\n" | "${DDRESCUELOG}" -b2048 -f -c+? mapfile || fail=1
"${DDRESCUELOG}" -q -p mapfile ${map1}
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -q -i0x5000 -p mapfile ${map1}
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -i0x5000 -s0x3800 -p mapfile ${map1} || fail=1
printf .

"${DDRESCUELOG}" -C ${map2i} > mapfile || fail=1
"${DDRESCUELOG}" -p ${map2} mapfile || fail=1
printf .

cat ${map1} > mapfile || framework_failure
"${DDRESCUELOG}" -i1024 -s2048 -d mapfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -i1024 -s1024 -d mapfile || fail=1
printf .
"${DDRESCUELOG}" -q -i1024 -s1024 -d mapfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi

cat ${map2} > mapfile || framework_failure
"${DDRESCUELOG}" -m ${map1} -D mapfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -L -m - -D mapfile < ${map2i} || fail=1
printf .
"${DDRESCUELOG}" -i1024 -s2048 -d mapfile
if [ $? = 1 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -i2048 -s2048 -d mapfile || fail=1
printf .

"${DDRESCUELOG}" -b2048 -l+ ${map1} > out || fail=1
printf "0\n2\n4\n6\n8\n10\n12\n14\n16\n18\n20\n22\n24\n26\n28\n30\n32\n34\n" > copy || framework_failure
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -b2048 -l?- ${map1} > out || fail=1
printf "1\n3\n5\n7\n9\n11\n13\n15\n17\n19\n21\n23\n25\n27\n29\n31\n33\n35\n" > copy || framework_failure
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -b2048 -l+ -i0x1800 -o0 -s0x4000 ${map1} > out || fail=1
printf "1\n3\n5\n7\n" > copy || framework_failure
cmp out copy || fail=1
printf .

"${DDRESCUELOG}" -n ${map2} > mapfile || framework_failure
"${DDRESCUELOG}" -b2048 -l+ mapfile > out || fail=1
printf "0\n2\n4\n6\n8\n10\n12\n14\n16\n18\n20\n22\n24\n26\n28\n30\n32\n34\n" > copy || framework_failure
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -b2048 -l?- mapfile > out || fail=1
printf "1\n3\n5\n7\n9\n11\n13\n15\n17\n19\n21\n23\n25\n27\n29\n31\n33\n35\n" > copy || framework_failure
cmp out copy || fail=1
printf .
"${DDRESCUELOG}" -b2048 -l+ -i2048 -o0 -s0x4000 mapfile > out || fail=1
printf "1\n3\n5\n7\n" > copy || framework_failure
cmp out copy || fail=1
printf .

"${DDRESCUELOG}" -q -P ${map2i} - < ${map2}
if [ $? = 2 ] ; then printf . ; else printf - ; fail=1 ; fi
"${DDRESCUELOG}" -L -P ${map2i} ${map2} || fail=1
printf .

fail2=0			# test XOR
for i in ${map1} ${map2} ${map3} ${map4} ${map5} ; do
	for j in ${map1} ${map2} ${map3} ${map4} ${map5} ; do
		"${DDRESCUELOG}" -x ${j} ${i} > out || fail2=1
		"${DDRESCUELOG}" -x ${i} ${j} > copy || fail2=1
		"${DDRESCUELOG}" -P out copy || fail2=1
		"${DDRESCUELOG}" -x ${j} out > copy || fail2=1
		"${DDRESCUELOG}" -P ${i} copy || fail2=1
	done
done
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0
"${DDRESCUELOG}" -x ${map1} - < ${map2} > out || fail2=1
"${DDRESCUELOG}" -x ${map2} ${map1} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
"${DDRESCUELOG}" -d out || fail2=1
"${DDRESCUELOG}" -d copy || fail2=1
"${DDRESCUELOG}" -x ${map1} ${blank} > out || fail2=1
"${DDRESCUELOG}" -x ${blank} ${map1} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
"${DDRESCUELOG}" -p out ${map1} || fail2=1
"${DDRESCUELOG}" -p ${map1} copy || fail2=1
"${DDRESCUELOG}" -x ${map2} ${map2} > mapfile || fail2=1
"${DDRESCUELOG}" -P ${blank} mapfile || fail2=1
"${DDRESCUELOG}" -x ${map1} ${map1} > mapfile || fail2=1
"${DDRESCUELOG}" -P ${blank} mapfile || fail2=1
"${DDRESCUELOG}" -b2048 -l+ ${map1} > out || fail2=1
"${DDRESCUELOG}" -b2048 -l- mapfile > copy || fail2=1
cmp out copy || fail2=1
"${DDRESCUELOG}" -b2048 -i0x2000 -s0x2800 -l+ ${map1} > out || fail2=1
"${DDRESCUELOG}" -i0x1800 -s0x3800 -x ${map1} ${map1} > mapfile || fail2=1
"${DDRESCUELOG}" -b2048 -l- mapfile > copy || fail2=1
cmp out copy || fail2=1
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0
"${DDRESCUELOG}" -x ${map3} ${map4} > out || fail2=1
"${DDRESCUELOG}" -x ${map4} ${map3} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
"${DDRESCUELOG}" -x ${map3} ${map5} > out || fail2=1
"${DDRESCUELOG}" -x ${map5} ${map3} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
"${DDRESCUELOG}" -x ${map4} ${map5} > out || fail2=1
"${DDRESCUELOG}" -x ${map5} ${map4} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0
"${DDRESCUELOG}" -x ${map3} ${map4} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -x out ${map5} > mapfile || fail2=1
"${DDRESCUELOG}" -d mapfile || fail2=1

"${DDRESCUELOG}" -x ${map3} ${map5} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -x out ${map4} > mapfile || fail2=1
"${DDRESCUELOG}" -d mapfile || fail2=1

"${DDRESCUELOG}" -x ${map4} ${map3} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -x out ${map5} > mapfile || fail2=1
"${DDRESCUELOG}" -d mapfile || fail2=1

"${DDRESCUELOG}" -x ${map4} ${map5} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -x out ${map3} > mapfile || fail2=1
"${DDRESCUELOG}" -d mapfile || fail2=1

"${DDRESCUELOG}" -x ${map5} ${map3} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -x out ${map4} > mapfile || fail2=1
"${DDRESCUELOG}" -d mapfile || fail2=1

"${DDRESCUELOG}" -x ${map5} ${map4} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -x out ${map3} > mapfile || fail2=1
"${DDRESCUELOG}" -d mapfile || fail2=1
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0			# test AND
for i in ${map1} ${map2} ${map3} ${map4} ${map5} ; do
	for j in ${map1} ${map2} ${map3} ${map4} ${map5} ; do
		"${DDRESCUELOG}" -y ${j} ${i} > out || fail2=1
		"${DDRESCUELOG}" -y ${i} ${j} > copy || fail2=1
		"${DDRESCUELOG}" -P out copy || fail2=1
	done
done
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0
"${DDRESCUELOG}" -b2048 -l+ ${map1} > out || fail2=1
"${DDRESCUELOG}" -y ${map1} - < ${map2} > mapfile || fail2=1
"${DDRESCUELOG}" -P ${blank} mapfile || fail2=1
"${DDRESCUELOG}" -b2048 -l? mapfile > copy || fail2=1
cmp out copy || fail2=1
"${DDRESCUELOG}" -y ${map2} ${map1} > mapfile || fail2=1
"${DDRESCUELOG}" -P ${blank} mapfile || fail2=1
"${DDRESCUELOG}" -b2048 -l- mapfile > copy || fail2=1
cmp out copy || fail2=1
"${DDRESCUELOG}" -b2048 -i0x2000 -s0x2800 -l+ ${map1} > out || fail2=1
"${DDRESCUELOG}" -i0x1800 -s0x3800 -y ${map2} ${map1} > mapfile || fail2=1
"${DDRESCUELOG}" -b2048 -l- mapfile > copy || fail2=1
cmp out copy || fail2=1
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0
"${DDRESCUELOG}" -y ${map3} ${map4} > out || fail2=1
"${DDRESCUELOG}" -P ${blank} out || fail2=1
"${DDRESCUELOG}" -y ${map3} ${map5} > out || fail2=1
"${DDRESCUELOG}" -P ${blank} out || fail2=1
"${DDRESCUELOG}" -y ${map4} ${map5} > out || fail2=1
"${DDRESCUELOG}" -P ${blank} out || fail2=1
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0
"${DDRESCUELOG}" -i0x2000 -s0x2800 -z ${map2} ${map1} > mapfile || fail2=1
"${DDRESCUELOG}" -D mapfile
if [ $? != 1 ] ; then fail2=1 ; fi
"${DDRESCUELOG}" -i0x1C00 -s0x2C00 -D mapfile
if [ $? != 1 ] ; then fail2=1 ; fi
"${DDRESCUELOG}" -i0x2000 -s0x2C00 -D mapfile
if [ $? != 1 ] ; then fail2=1 ; fi
"${DDRESCUELOG}" -i0x2000 -s0x2800 -d mapfile
if [ $? != 0 ] ; then fail2=1 ; fi
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0			# test OR
for i in ${map1} ${map2} ${map3} ${map4} ${map5} ; do
	for j in ${map1} ${map2} ${map3} ${map4} ${map5} ; do
		"${DDRESCUELOG}" -z ${j} ${i} > out || fail2=1
		"${DDRESCUELOG}" -z ${i} ${j} > copy || fail2=1
		"${DDRESCUELOG}" -P out copy || fail2=1
	done
done
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0
"${DDRESCUELOG}" -z ${map1} - < ${map2} > out || fail2=1
"${DDRESCUELOG}" -z ${map2} ${map1} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
"${DDRESCUELOG}" -d out || fail2=1
"${DDRESCUELOG}" -d copy || fail2=1
"${DDRESCUELOG}" -z ${map1} ${blank} > out || fail2=1
"${DDRESCUELOG}" -z ${blank} ${map1} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
"${DDRESCUELOG}" -p out ${map1} || fail2=1
"${DDRESCUELOG}" -p ${map1} copy || fail2=1
"${DDRESCUELOG}" -z ${map3} ${map4} > out || fail2=1
"${DDRESCUELOG}" -z ${map4} ${map3} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
"${DDRESCUELOG}" -z ${map3} ${map5} > out || fail2=1
"${DDRESCUELOG}" -z ${map5} ${map3} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
"${DDRESCUELOG}" -z ${map4} ${map5} > out || fail2=1
"${DDRESCUELOG}" -z ${map5} ${map4} > copy || fail2=1
"${DDRESCUELOG}" -p out copy || fail2=1
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0
"${DDRESCUELOG}" -z ${map3} ${map4} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -z out ${map5} > mapfile || fail2=1
"${DDRESCUELOG}" -d mapfile || fail2=1

"${DDRESCUELOG}" -z ${map3} ${map5} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -z out ${map4} > mapfile || fail2=1
"${DDRESCUELOG}" -d mapfile || fail2=1

"${DDRESCUELOG}" -z ${map4} ${map3} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -z out ${map5} > mapfile || fail2=1
"${DDRESCUELOG}" -d mapfile || fail2=1

"${DDRESCUELOG}" -z ${map4} ${map5} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -z out ${map3} > mapfile || fail2=1
"${DDRESCUELOG}" -d mapfile || fail2=1

"${DDRESCUELOG}" -z ${map5} ${map3} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -z out ${map4} > mapfile || fail2=1
"${DDRESCUELOG}" -d mapfile || fail2=1

"${DDRESCUELOG}" -z ${map5} ${map4} > out || fail2=1
"${DDRESCUELOG}" -D out && fail2=1
"${DDRESCUELOG}" -z out ${map3} > mapfile || fail2=1
"${DDRESCUELOG}" -d mapfile || fail2=1
if [ ${fail2} = 0 ] ; then printf . ; else printf - ; fail=1 ; fi

fail2=0			# test ( a && b ) == !( !a || !b )
for i in ${map1} ${map2} ${map3} ${map4} ${map5} ; do
	for j in ${map1} ${map2} ${map3} ${map4} ${map5} ; do
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
