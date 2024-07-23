/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <random>

#include <gtest/gtest.h>
#include "util/graph_check.h"

using namespace std;
using namespace testing;
using namespace ffrt;
using namespace testing::ext;

class GraphCheckTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

HWTEST_F(GraphCheckTest, HasCyclic, TestSize.Level1)
{
    GraphCheckCyclic graph;

    graph.AddVetexByLabel(0x12);
    graph.AddVetexByLabel(0x34);
    graph.AddEdgeByLabel(0x12, 0x34);
    graph.AddEdgeByLabel(0x34, 0x12);

    EXPECT_EQ(graph.VertexNum(), 2);
    EXPECT_EQ(graph.IsCyclic(), true);
}

HWTEST_F(GraphCheckTest, HasNoCyclic, TestSize.Level1)
{
    GraphCheckCyclic graph;

    graph.AddVetexByLabel(0x12);
    graph.AddVetexByLabel(0x34);
    graph.AddEdgeByLabel(0x12, 0x34);
    graph.AddEdgeByLabel(0x34, 0x12);
    graph.RemoveEdgeByLabel(0x12);

    EXPECT_EQ(graph.EdgeNum(), 1);
    EXPECT_EQ(graph.IsCyclic(), false);
}
