#!python3
# Copyright (c) 2026 Huawei Technologies Co., Ltd
# OmniOperator is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import sys
import numpy as np
import re
import matplotlib.pyplot as plt

import plot_util

def printHelp():
    print("Generate plots from benchmark data")
    print("Usage: generate_plot.py OUTPUT_DIR NAME=DATA_FILE_PATH [name=DATA_FILE_PATH ...]")
    print("  OUTPUT_DIR: folder to store plot images")
    print("  NAME: benchmark name (i.e. original, optimized)")
    print("  DATA_FILE_PATH: path to extracted benchmark data")

if len(sys.argv) < 2:
    print("Too few arguments")
    printHelp()
    exit(-1)
if sys.argv[1].find("=") >= 0:
    print("Output directory not set")
    printHelp()
    exit(-1)

outputDir = sys.argv[1]
datasets = {}
header = []
for i in range(2, len(sys.argv)):
    input = sys.argv[i]
    idx = input.find("=")
    if idx <= 0:
        print("Invalid NAME=PATH tuple at argument {}: {}".format(i, input))
        exit(-2)
    name = input[0:idx]
    (h, data) = plot_util.loadData(input[idx + 1:], "%(Iterations)|(Time\(ms\))|(CPU\(ms\))$")
    print("[{}]: #rows: {}".format(name, len(data)))
    if len(header) == 0:
        header = h
    datasets[name] = data

print("Generating plot for Sum Aggregator Primitive...")
filter = {"AggType" : "^sum$", "VectorType" : "^(int)|(long)|(double)$",\
          "UseMask" : "false", "InputRaw" : "true", "OutputPartial" : "false"}
nameMap = [["HasNull", {"false" : "not-nullable", "true" : "nullable"}], \
           ["IsDictionary", {"false" : "flat", "true" : "dict"}],\
           ["VectorType", {}]]
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", "Sum Aggregator Primitive (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", "Sum Aggregator Primitive (CPU)")

print("Generating plot for Sum Aggregator Decimal...")
filter = {"AggType" : "^sum$", "VectorType" : "^(decimal64)|(decimal128)$", "UseMask" : "false"}
nameMap = [["HasNull", {"false" : "not-nullable", "true" : "nullable"}], \
           ["IsDictionary", {"false" : "flat", "true" : "dict"}],\
           ["InputRaw", {"false" : "raw", "true" : "not-raw"}],\
           ["OutputPartial", {"false" : "partial", "true" : "not-partial"}],\
           ["VectorType", {}]]
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", "Sum Aggregator Decimal (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", "Sum Aggregator Decimal (CPU)")

print("Generating plot for Average Aggregator Primitive...")
filter = {"AggType" : "^avg$", "VectorType" : "^(int)|(long)|(double)$",\
          "UseMask" : "false", "InputRaw" : "true", "OutputPartial" : "false"}
nameMap = [["HasNull", {"false" : "not-nullable", "true" : "nullable"}], \
           ["IsDictionary", {"false" : "flat", "true" : "dict"}],\
           ["VectorType", {}]]
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", "Average Aggregator Primitive (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", "Average Aggregator Primitive (CPU)")

print("Generating plot for Average Aggregator Decimal...")
filter = {"AggType" : "^avg$", "VectorType" : "^(decimal64)|(decimal128)$", "UseMask" : "false"}
nameMap = [["HasNull", {"false" : "not-nullable", "true" : "nullable"}], \
           ["IsDictionary", {"false" : "flat", "true" : "dict"}],\
           ["InputRaw", {"false" : "raw", "true" : "not-raw"}],\
           ["OutputPartial", {"false" : "partial", "true" : "not-partial"}],\
           ["VectorType", {}]]
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", "Average Aggregator Decimal (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", "Average Aggregator Decimal (CPU)")

print("Generating plot for Min Aggregator...")
filter = {"AggType" : "^min$", "VectorType" : "^(int)|(long)|(double)|(decimal64)|(decimal128)|(char)$",\
        "UseMask" : "false", "InputRaw" : "true", "OutputPartial" : "false"}
nameMap = [["HasNull", {"false" : "not-nullable", "true" : "nullable"}], \
           ["IsDictionary", {"false" : "flat", "true" : "dict"}], ["VectorType", {}]]
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", "Min Aggregator (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", "Min Aggregator (CPU)")

print("Generating plot for Max Aggregator...")
filter["AggType"] = "^max$"
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", "Max Aggregator (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", "Max Aggregator (CPU)")

print("Generating plot for Count Column Aggregator...")
filter["AggType"] = "^count$"
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", "Count Column Aggregator (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", "Count Column Aggregator (CPU)")

print("Generating plot for Count All Aggregator...")
filter["AggType"] = "^count_all$"
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", "Count All Aggregator (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", "Count All Aggregator (CPU)")


# masked
print("Generating plot for Masked Sum Aggregator Primitive...")
filter = {"AggType" : "^sum$", "VectorType" : "^(int)|(long)|(double)$",\
          "UseMask" : "true", "InputRaw" : "true", "OutputPartial" : "false"}
nameMap = [["HasNull", {"false" : "not-nullable", "true" : "nullable"}], \
           ["IsDictionary", {"false" : "flat", "true" : "dict"}],\
           ["VectorType", {}]]
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", "Masked Sum Aggregator Primitive (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", "Masked Sum Aggregator Primitive (CPU)")

print("Generating plot for Masked Sum Aggregator Decimal...")
filter = {"AggType" : "^sum$", "VectorType" : "^(decimal64)|(decimal128)$", "UseMask" : "true"}
nameMap = [["HasNull", {"false" : "not-nullable", "true" : "nullable"}], \
           ["IsDictionary", {"false" : "flat", "true" : "dict"}],\
           ["InputRaw", {"false" : "raw", "true" : "not-raw"}],\
           ["OutputPartial", {"false" : "partial", "true" : "not-partial"}],\
           ["VectorType", {}]]
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", "Masked Sum Aggregator Decimal (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", "Masked Sum Aggregator Decimal (CPU)")

print("Generating plot for Masked Average Aggregator Primitive...")
filter = {"AggType" : "^avg$", "VectorType" : "^(int)|(long)|(double)$",\
          "UseMask" : "true", "InputRaw" : "true", "OutputPartial" : "false"}
nameMap = [["HasNull", {"false" : "not-nullable", "true" : "nullable"}], \
           ["IsDictionary", {"false" : "flat", "true" : "dict"}],\
           ["VectorType", {}]]
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", "Masked Average Aggregator Primitive (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", "Masked Average Aggregator Primitive (CPU)")

print("Generating plot for Masked Average Aggregator Decimal...")
filter = {"AggType" : "^avg$", "VectorType" : "^(decimal64)|(decimal128)$", "UseMask" : "true"}
nameMap = [["HasNull", {"false" : "not-nullable", "true" : "nullable"}], \
           ["IsDictionary", {"false" : "flat", "true" : "dict"}],\
           ["InputRaw", {"false" : "raw", "true" : "not-raw"}],\
           ["OutputPartial", {"false" : "partial", "true" : "not-partial"}],\
           ["VectorType", {}]]
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", "Masked Average Aggregator Decimal (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", "Masked Average Aggregator Decimal (CPU)")

print("Generating plot for Masked Min Aggregator...")
filter = {"AggType" : "^min$", "VectorType" : "^(int)|(long)|(double)|(decimal64)|(decimal128)|(char)$",\
        "UseMask" : "true", "InputRaw" : "true", "OutputPartial" : "false"}
nameMap = [["HasNull", {"false" : "not-nullable", "true" : "nullable"}], \
           ["IsDictionary", {"false" : "flat", "true" : "dict"}], ["VectorType", {}]]
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", "Masked Min Aggregator (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", "Masked Min Aggregator (CPU)")

print("Generating plot for Masked Max Aggregator...")
filter["AggType"] = "^max$"
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", "Masked Max Aggregator (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", "Masked Max Aggregator (CPU)")

print("Generating plot for Masked Count Column Aggregator...")
filter["AggType"] = "^count$"
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", "Masked Count Column Aggregator (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", "Masked Count Column Aggregator (CPU)")

print("Generating plot for Masked Count All Aggregator...")
filter["AggType"] = "^count_all$"
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", "Masked Count All Aggregator (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", "Masked Count All Aggregator (CPU)")
