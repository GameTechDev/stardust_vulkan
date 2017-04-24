/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

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
