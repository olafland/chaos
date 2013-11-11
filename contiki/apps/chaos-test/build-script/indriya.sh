#!/bin/bash
set -e

sink=1
testbed=indriya


dir=build/${testbed}

rm -rf ${dir}/
mkdir -p ${dir}/

for pw in 31 24 17 10
do
	(cd ..
	make clean
	make chaos-test.sky TARGET=sky DEFINES=TESTBED=${testbed},INITIATOR_NODE_ID=${sink},CC2420_TXPOWER=${pw})	
	cp ../chaos-test.sky ${dir}/ct_${testbed}_pw${pw}_sink${sink}.exe
done
