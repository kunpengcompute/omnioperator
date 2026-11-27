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
#include <gtest/gtest.h>

#include "plannode/planNode.h"

using namespace ::omniruntime;

TEST(TestPlanNode, sortOrder)
{
    struct {
        SortOrderInfo order1;
        SortOrderInfo order2;
        int expectedEqual;

        std::string debugString() const
        {
            return Format("order1 {} order2 {} expectedEqual {}", order1.ToString(), order2.ToString(), expectedEqual);
        }
    } testSettings[] = {{{true, true}, {true, true}, true},    {{true, false}, {true, false}, true},
                        {{false, true}, {false, true}, true},  {{false, false}, {false, false}, true},
                        {{true, true}, {true, false}, false},  {{true, true}, {false, false}, false},
                        {{true, true}, {false, true}, false},  {{true, false}, {false, false}, false},
                        {{true, false}, {false, true}, false}, {{false, true}, {false, false}, false}};
    for (const auto &testData : testSettings) {
        SCOPED_TRACE(testData.debugString());
        if (testData.expectedEqual) {
            ASSERT_EQ(testData.order1, testData.order2);
        } else {
            ASSERT_NE(testData.order1, testData.order2);
        }
    }
}
