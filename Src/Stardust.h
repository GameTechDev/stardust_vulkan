// Copyright 2016 Intel Corporation All Rights Reserved
// 
// Intel makes no representations about the suitability of this software for any purpose.
// THIS SOFTWARE IS PROVIDED ""AS IS."" INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES,
// EXPRESS OR IMPLIED, AND ALL LIABILITY, INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES,
// FOR THE USE OF THIS SOFTWARE, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY
// RIGHTS, AND INCLUDING THE WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
// Intel does not assume any responsibility for any errors which may appear in this software
// nor any responsibility to update it.

#pragma once

#include <SDL_video.h>
#include "Settings.h"

enum exit_code_t
{
    STARDUST_EXIT = 0,
    STARDUST_ERROR = 1,
    STARDUST_NOT_SUPPORTED,
    STARDUST_CONTINUE
};

struct graph_data_t
{
    int sampleidx;
    float sample[k_Graph_Samples];
    float sampley[k_Graph_Samples];
    float scale;
    int empty_flag;
};

struct glob_state_t
{
    void *hwnd;
    SDL_Window *window;
    int width;
    int height;

    int frame;
    float transform_time;
    int transform_animate;
    float palette_factor;
    int palette_image_idx;
    unsigned int seed;
    struct graph_data_t graph_data[MAX_CPU_CORES];

    int batch_size;
    int point_count;
    int windowed;
    int verbose;
    int cpu_core_count;
};

int VK_Init(struct glob_state_t *state);
int VK_Shutdown(struct glob_state_t *state);
int VK_Run(struct glob_state_t *state);
int Handle_Events(struct glob_state_t *state);
