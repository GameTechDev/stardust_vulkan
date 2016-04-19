// Copyright 2016 Intel Corporation All Rights Reserved
// 
// Intel makes no representations about the suitability of this software for any purpose.
// THIS SOFTWARE IS PROVIDED ""AS IS."" INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES,
// EXPRESS OR IMPLIED, AND ALL LIABILITY, INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES,
// FOR THE USE OF THIS SOFTWARE, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY
// RIGHTS, AND INCLUDING THE WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
// Intel does not assume any responsibility for any errors which may appear in this software
// nor any responsibility to update it.

#include "Settings.h"
#include "Stardust.h"
#include "Misc.h"
#include "Metrics.h"
#include "Graph.h"

#include "SDL.h"
#include "SDL_syswm.h"
#include <stdlib.h>
#include <assert.h>

int Global_Init(struct glob_state_t *state, int argc, char **argv)
{
    memset(state, 0, sizeof(*state));
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    state->cpu_core_count = si.dwNumberOfProcessors;
#else
    state->cpu_core_count = 4;
#endif
    state->transform_time = 0.2;
    state->transform_animate = 1;
    state->windowed = 1;
    state->seed = 23232323;
    state->batch_size = k_Def_Batch_Size;
    state->point_count = k_Def_Point_Count;
    for (int i = 0; i < 6 * 9; ++i) {
        RND_GEN(state->seed);
    }
    for (int i = 0; i < state->cpu_core_count; i++) {
        state->graph_data[i].empty_flag = 1;
    }
    Enable_Logging(state->verbose);

    if (!Metrics_Init()) {
        Log("Metrics initialization failed");
        return 1;
    }

#if defined(_WIN32)
    // This informs the WM, that we can handle high DPI ourselves and disables scaling. 
    // This is required, so SDL_GetDisplayBounds returns sane values on highDPI below.
    SetProcessDPIAware();
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) return 1;

    SDL_Rect dpyBounds;
    SDL_GetDisplayBounds(0, &dpyBounds);
    Log("SDL_GetDisplayBounds: %d, %d ... %d, %d\n", dpyBounds.x, dpyBounds.y, dpyBounds.w, dpyBounds.h);

    int xoffset, yoffset;
    if (!state->windowed) {
        state->width = dpyBounds.w;
        state->height = dpyBounds.h;
        xoffset = dpyBounds.x;
        yoffset = dpyBounds.y;
    } else {
        state->width  = (int)(dpyBounds.w * 0.9);
        state->height = (int)(dpyBounds.h * 0.9);
        if (state->width > 1920) state->width = 1920;
        if (state->height > 1080) state->height = 1080;
        xoffset = SDL_WINDOWPOS_CENTERED;
        yoffset = SDL_WINDOWPOS_CENTERED;
    }
    Log("Creating Window of size %dx%d", state->width, state->height);
    state->window = SDL_CreateWindow("loading...", xoffset, yoffset, state->width, state->height,
                                     SDL_WINDOW_SHOWN | (state->windowed ? 0 : SDL_WINDOW_BORDERLESS));

    if (!state->window) {
        Log("SDL_CreateWindow failed: cannot create window");
        return STARDUST_ERROR;
    }

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(state->window, &info);
    state->hwnd = info.info.win.window;

    if (!state->hwnd) {
        Log("SDL_GetWindowWMInfo failed: zero HWND");
        return 0;
    }
    return STARDUST_CONTINUE;
}

int Global_Deinit(struct glob_state_t *state)
{
    if (state->window) {
        SDL_DestroyWindow(state->window);
    }
    SDL_Quit();
    return STARDUST_CONTINUE;
}

int Handle_Events(struct glob_state_t *state)
{
    int exit_code = STARDUST_CONTINUE;
    SDL_Event evt;

    while (SDL_PollEvent(&evt)) {
        if (evt.type == SDL_QUIT) {
            exit_code = STARDUST_EXIT;
        }
        else if (evt.type == SDL_KEYDOWN) {
            if (evt.key.keysym.sym == SDLK_ESCAPE) {
                exit_code = STARDUST_EXIT;
            }
            else if (evt.key.keysym.sym == SDLK_SPACE) {
                state->transform_animate = !state->transform_animate;
            }
        }
    }
    return exit_code;
}

int SDL_main(int argc, char *argv[])
{
    int exit_code = STARDUST_CONTINUE;
    int not_supported_count = 0;

#if defined(_WIN32)
    /* core 0 as default */ {
        DWORD_PTR procAffinityMask = 1;
        BOOL res = 0 != SetThreadAffinityMask(GetCurrentThread(), procAffinityMask);
    }
#endif
    struct glob_state_t global_state;
    if (Global_Init(&global_state, argc, argv) != STARDUST_CONTINUE) {
        Log("Application initialization failed");
        return 1;
    }

    if (VK_Init(&global_state) == STARDUST_CONTINUE) {
        VK_Run(&global_state);
        VK_Shutdown(&global_state);
    }

    Global_Deinit(&global_state);
    return 1;
}

#ifdef _WIN32

#if defined(NO_CONSOLE) && defined(_WIN32)
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    return SDL_main(__argc, __argv);
}
#else
int main(int argc, char **argv)
{
    return SDL_main(argc, argv);
}
#endif

#endif
