# Copyright (c) 2026 Huawei Technologies Co., Ltd
# OmniOperator is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

#/bin/bash

if [ -z "$1" ]  || [ -z "$2" ]
then
    echo "Too few arguments"
    echo "Usage: extract_output.sh BENCHMARK_NAME BENCHMARK_OUTPUT"
    echo "  BENCHMARK_NAME: name of benchmark test, i.e. 'AggBenchmark'"
    echo "  BENCHMARK_OUTPUT: path to ouput of benchmark"
    exit -1
fi
if [ "$1" == "-h" ] || [ "$1" == "--help" ]
then
    echo "Extract result of benchmark from benchmark output file for specified benchmark"
    echo "Usage: extract_output.sh BENCHMARK_NAME BENCHMARK_OUTPUT"
    echo "  BENCHMARK_NAME: name of benchmark test, i.e. 'AggBenchmark'"
    echo "  BENCHMARK_OUTPUT: path to ouput of benchmark"
    exit 0
fi

benchmark="$1"
file="$2"

if [ "$benchmark" == "AggBenchmark" ]
then
    echo "HasNull IsDictionary UseMask InputRaw OutputPartial AggType VectorType Iterations Time(ms) CPU(ms)"
    grep -e "^AggBenchmark/" $file | grep -v "inf.*nan Skipped" | perl -pe 's/.*manual_time\s*(.*) ms\s*(.*) ms\s*(.*) addInput.*HasNull:(.*)\|IsDictionary:(.*)\|UseMask:(.*)\|InputRaw:(.*)\|OutputPartial:(.*)\|AggType:(.*)\|VectorType:(.*)$/$4 $5 $6 $7 $8 $9 $10 $3 $1 $2/g;'
elif [ "$benchmark" == "HashAgg" ]
then
    echo "RowsPerGroup IsDictionary OverflowAsNull PrefixLength SqlId Iterations Time(ms) CPU(ms)"
    grep -e "^HashAgg/" $file | grep -v "inf.*nan Skipped" | perl -pe 's/.*manual_time\s*(.*) ms\s*(.*) ms\s*(.*) addInput.*RowsPerGroup:(.*)\|IsDictionary:(.*)\|OverflowAsNull:(.*)\|PrefixLength:(.*)\|SqlId:(.*)$/$4 $5 $6 $7 $8 $3 $1 $2/g;'
elif [ "$benchmark" == "HashAggBenchmark" ]
then
    echo "CardinalitieIdx LabAggByIdx LabGroupByIdx Iterations Time(ms) CPU(ms)"
    grep -e "^HashAggBenchmark/" $file | grep -v "inf.*nan Skipped" | perl -pe 's/.*manual_time\s*(.*) ms\s*(.*) ms\s*(.*) addInput.*CardinalitieIdx:(.*)\|LabAggByIdx:(.*)\|LabGroupByIdx:(.*)$/$4 $5 $6 $3 $1 $2/g;'
else
    echo "Filter for $benchmark not defined"
    exit -2
fi
