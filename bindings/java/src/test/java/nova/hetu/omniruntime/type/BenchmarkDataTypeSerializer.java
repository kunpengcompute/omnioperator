/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

package nova.hetu.omniruntime.type;

import static java.util.concurrent.TimeUnit.MILLISECONDS;

import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Fork;
import org.openjdk.jmh.annotations.Measurement;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.annotations.Warmup;
import org.openjdk.jmh.runner.Runner;
import org.openjdk.jmh.runner.options.Options;
import org.openjdk.jmh.runner.options.OptionsBuilder;
import org.openjdk.jmh.runner.options.VerboseMode;

import java.util.ArrayList;
import java.util.List;

/**
 * DataType serialize benchmark
 *
 * @since 2022-5-18
 */
@State(Scope.Thread)
@OutputTimeUnit(MILLISECONDS)
@Fork(1)
@Warmup(iterations = 5, batchSize = 1)
@Measurement(iterations = 20, batchSize = 1)
@BenchmarkMode(Mode.AverageTime)
public class BenchmarkDataTypeSerializer {
    @Param("1000")
    int times = 1000;

    /**
     * create benchmark for datatype serialize
     *
     * @return Deserialized dataType list
     */
    @Benchmark
    public List<DataType[]> dataTypeSerializeBenchmark() {
        DataType[] dataTypes = new DataType[]{BooleanDataType.BOOLEAN, ShortDataType.SHORT, IntDataType.INTEGER,
                LongDataType.LONG, DoubleDataType.DOUBLE, Decimal64DataType.DECIMAL64, Decimal128DataType.DECIMAL128,
                Date32DataType.DATE32, Date64DataType.DATE64, VarcharDataType.VARCHAR, CharDataType.CHAR,
                TimestampDataType.TIMESTAMP};
        List<DataType[]> list = new ArrayList<>();
        for (int i = 0; i < times; i++) {
            String allSerializedTypes = DataTypeSerializer.serialize(dataTypes);
            DataType[] allDeserializedTypes = DataTypeSerializer.deserialize(allSerializedTypes);
            list.add(allDeserializedTypes);
        }
        return list;
    }

    public static void main(String[] args) throws Throwable {
        Options options = new OptionsBuilder().verbosity(VerboseMode.NORMAL)
                .include(".*" + BenchmarkDataTypeSerializer.class.getSimpleName() + ".*").build();
        new Runner(options).run();
    }
}
