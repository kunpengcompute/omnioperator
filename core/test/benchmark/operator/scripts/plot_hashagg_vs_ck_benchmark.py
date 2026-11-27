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

plotName = "Average (Long)"
filter = {"LabAggByIdx" : "^0$"}
print("Generating plot for {} ...".format(plotName))
nameMap = [["CardinalitieIdx", {"0" : "100", "1" : "1000", "2" : "10000", "3" : "100000"}], \
           ["LabGroupByIdx", {"0" : "VarChar50", "1" : "LongType", "2" : "DoubleType", "3" : "Decimal128"}]]
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", plotName + " (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", plotName + " (CPU)")

plotName = "Average (Decimal128)"
filter = {"LabAggByIdx" : "^1$"}
print("Generating plot for {} ...".format(plotName))
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", plotName + " (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", plotName + " (CPU)")

plotName = "Min (Long)"
filter = {"LabAggByIdx" : "^2$"}
print("Generating plot for {} ...".format(plotName))
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", plotName + " (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", plotName + " (CPU)")

plotName = "Max (Long)"
filter = {"LabAggByIdx" : "^3$"}
print("Generating plot for {} ...".format(plotName))
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", plotName + " (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", plotName + " (CPU)")

plotName = "Sum (Long)"
filter = {"LabAggByIdx" : "^4$"}
print("Generating plot for {} ...".format(plotName))
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", plotName + " (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", plotName + " (CPU)")

plotName = "Sum (Decimal128)"
filter = {"LabAggByIdx" : "^5$"}
print("Generating plot for {} ...".format(plotName))
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", plotName + " (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", plotName + " (CPU)")

plotName = "CountColumn (Long)"
filter = {"LabAggByIdx" : "^6$"}
print("Generating plot for {} ...".format(plotName))
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "Time(ms)", plotName + " (TIME)")
plot_util.generatePlot(outputDir, datasets, header, filter, nameMap, "CPU(ms)", plotName + " (CPU)")
