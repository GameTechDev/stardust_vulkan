// Copyright 2016 Intel Corporation All Rights Reserved
// 
// Intel makes no representations about the suitability of this software for any purpose.
// THIS SOFTWARE IS PROVIDED ""AS IS."" INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES,
// EXPRESS OR IMPLIED, AND ALL LIABILITY, INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES,
// FOR THE USE OF THIS SOFTWARE, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY
// RIGHTS, AND INCLUDING THE WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
// Intel does not assume any responsibility for any errors which may appear in this software
// nor any responsibility to update it.

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
