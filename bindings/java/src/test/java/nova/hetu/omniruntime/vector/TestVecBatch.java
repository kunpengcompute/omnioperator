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

package nova.hetu.omniruntime.vector;

import static org.testng.Assert.assertEquals;

import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.List;

/**
 * test vec batch
 *
 * @since 2021-6-23
 */
public class TestVecBatch {
    /**
     * test new vec batch
     */
    @Test
    public void testNewVecBatch() {
        int vecCount = 10;
        int rowCount = 1024;
        Vec[] vecArray = new Vec[vecCount];
        for (int i = 0; i < vecCount; i++) {
            vecArray[i] = new LongVec(rowCount);
        }
        VecBatch vecBatch = new VecBatch(vecArray, rowCount);
        vecBatch.releaseAllVectors();
        vecBatch.close();
    }

    @Test
    public void testNewVecBatchWithEmptyVectors() {
        // for load libomni_runtime.so
        LongVec vec = new LongVec(1);
        vec.close();
        List<Vec> emptyVecs = new ArrayList<>();
        int rowCount = 100;
        VecBatch vecBatch = new VecBatch(emptyVecs, rowCount);
        assertEquals(vecBatch.getRowCount(), rowCount);
        assertEquals(vecBatch.getVectorCount(), 0);
        vecBatch.releaseAllVectors();
        vecBatch.close();

        // rowcount and vectorcount is 0
        VecBatch vecBatch1 = new VecBatch(emptyVecs, 0);
        assertEquals(vecBatch1.getRowCount(), 0);
        assertEquals(vecBatch1.getVectorCount(), 0);
        vecBatch1.releaseAllVectors();
        vecBatch1.close();
    }
}
