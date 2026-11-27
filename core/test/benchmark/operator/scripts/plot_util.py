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
import pathlib

def loadData(dataPath, numericColsFilter):
    rawData = np.genfromtxt(dataPath, dtype=None, delimiter=" ", encoding="utf-8")
    rawHeader = rawData[0]
    header = []
    numericColumns = set()
    for i in range(0, len(rawHeader)):
        strVal = rawHeader[i]
        if re.search(numericColsFilter, strVal):
            numericColumns.add(i)
        header.append(strVal)
    # print("Header ({}): {}".format(len(header), header))
    # print("Numeric Columns ({}): {}".format(len(numericColumns), numericColumns))

    data = []
    for i in range(1, len(rawData)):
        rawRow = rawData[i]
        row = []
        for j in range(0, len(rawRow)):
            if j in numericColumns:
                row.append(float(rawRow[j]))
            else:
                row.append(rawRow[j])
        data.append(row)

    return (header, data)

def filterData(data, header, filters):
    subList = []
    for row in data:
        matched = True
        for (colName, filter) in filters.items():
            if not re.search(filter, row[header.index(colName)]):
                matched = False
                break
        if matched:
            subList.append(row)
    return subList

def extractData(data, header, nameCols, extractCols):
    res = []
    for inRow in data:
        outRow = []
        fullName = ""
        for nameCol in nameCols:
            if len(fullName) > 0:
                fullName += "_"
            val = inRow[header.index(nameCol[0])]
            if len(nameCol[1]) == 0:
                fullName += val
            else:
                fullName += nameCol[1][val]
        outRow.append(fullName)

        for extractCol in extractCols:
            outRow.append(inRow[header.index(extractCol)])
        res.append(outRow)
    return res

def plotData(outputDir, datas, colIdx, yLabel, title):
    colors = ["orange", "green", "red"]
    width = 0.35

    labels = []
    bars = []
    legend = []
    for (name, data) in datas.items():
        legend.append(name)
        if len(labels) == 0:
            bar = []
            for row in data:
                labels.append(row[0])
                bar.append(row[colIdx])
            bars.append(bar)
        else:
            bar = [None] * len(labels)
            for row in data:
                bar[labels.index(row[0])] = row[colIdx]
            bars.append(bar)
    # print("Labels: ", labels)
    # print("Bars: ", bars)

    x = np.arange(len(labels))
    start = -(width * ((len(datas) - 1.0) / 2.0))
    plt.rcParams["figure.figsize"] = (20, 15)
    for i in range(0, len(bars)):
        b = plt.bar(x + start, bars[i], width=width, color=colors[i])
        plt.bar_label(b)
        start += width

    plt.xticks(x, labels, rotation=45, horizontalalignment="right")
    plt.title(title)
    plt.ylabel(yLabel)
    plt.legend(labels=legend)

    # for b in bh:
    #     plt.bar_label(b)

    plt.savefig(outputDir + "/" + title.replace(" ", "_") + ".png", bbox_inches="tight")
    plt.close()

def generatePlot(outputDir, datas, header, filter, nameMap, colLabel, plotLable):
    pathlib.Path(outputDir).mkdir(parents=True, exist_ok=True)

    res = {}
    dataLength = -1
    for (name, data) in datas.items():
        subList = filterData(data, header, filter)
        # print("[{}] SubListLen: {}".format(name, len(subList)))
        if dataLength < 0:
            dataLength = len(subList)
            if dataLength == 0:
                print("WARN: Plot {} not generaged. Data length is zero for {}".format(plotLable, name))
                return
        else:
            if dataLength != len(subList):
                print("WARN: Plot {} not generated. Data length {} for {} does not match expected length {}"\
                    .format(plotLable, len(subList), name, dataLength))
                return
        res[name] = extractData(subList, header, nameMap, [colLabel])

    plotData(outputDir, res, 1, colLabel, plotLable)
