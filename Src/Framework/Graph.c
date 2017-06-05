////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License.  You may obtain a copy
// of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
// License for the specific language governing permissions and limitations
// under the License.
////////////////////////////////////////////////////////////////////////////////

#include "Graph.h"


//-----------------------------------------------------------------------------
void Graph_Clear(struct graph_data_t *data)
{
    if (!data) return;

    for (data->sampleidx = 0; data->sampleidx < k_Graph_Samples; data->sampleidx++) {
        data->sampley[data->sampleidx] = 0.0f;
        data->sample[data->sampleidx] = 0.0f;
    }
    data->sampleidx = 0;
}
//-----------------------------------------------------------------------------
void Graph_Add_Sample(struct graph_data_t *data, float s)
{
    Graph_Add_Sample_Y(data, s, 0.0f);
}
//-----------------------------------------------------------------------------
void Graph_Add_Sample_Y(struct graph_data_t *data, float s, float y)
{
    if (!data) return;

    if (!data->empty_flag) {
        data->sampley[data->sampleidx] = y;
        data->sample[data->sampleidx++] = s;
        if (data->sampleidx >= k_Graph_Samples)
            data->sampleidx = 0;
    }
}
//-----------------------------------------------------------------------------
