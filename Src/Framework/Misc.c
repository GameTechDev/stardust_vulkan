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

#include "Misc.h"
#include <stdio.h>
#include <stdlib.h>
#include "SDL_timer.h"
#ifdef _WIN32
#   include <windows.h>
#endif
#include "../Settings.h"
//=============================================================================
static int s_log_enabled = 1;
static int s_app_start_tics;
static int s_frame_begin_tics;
static int s_last_current_time_printf_tics;
//=============================================================================
stbi_uc *Load_Image(char const *filename, int *x, int *y, int *comp, int req_comp)
{
    return stbi_load(filename, x, y, comp, req_comp);
}
//=============================================================================
char *Load_Text_File(const char *filename)
{
  FILE *fp = fopen(filename, "rb");
  if (!fp) return NULL;

  fseek(fp, 0, SEEK_END);
  int size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *data = malloc(size + 1);
  fread(data, 1, size, fp);
  data[size] = '\0';
  fclose(fp);

  return data;
}
//=============================================================================
void Log(const char* fmt, ...)
{
    if (!s_log_enabled) return;

    char msg[1024];
    va_list myargs;
    int count = sprintf(msg, "Stardust: ");

    va_start(myargs, fmt);
    count += vsprintf(&msg[count], fmt, myargs);
    va_end(myargs);

    sprintf(&msg[count], "\n");

#if defined(NO_CONSOLE) && defined(_WIN32)
    OutputDebugStringA(msg);
#else
    printf(msg);
#endif
}
//=============================================================================
void Print_Current_Time(const char *message)
{
    static char buffer[1024];
    int current_tics = SDL_GetTicks();
    sprintf(buffer, "%s - %d, (%d)\n", message, current_tics - s_app_start_tics, current_tics - s_last_current_time_printf_tics);
    s_last_current_time_printf_tics = current_tics;
    Log(buffer);
}
//=============================================================================
int Update_Frame_Stats(double *time, float *time_delta, int frame, int filter_fps, float *fps, float *ms)
{
    static double prev_time;
    static double prev_fps_time;
    static int fps_frame = 0;

    s_frame_begin_tics = SDL_GetTicks();

    if (frame == 0) {
        s_app_start_tics = s_frame_begin_tics;
        prev_time = s_frame_begin_tics * 0.001;
        prev_fps_time = s_frame_begin_tics * 0.001;
    }

    *time = s_frame_begin_tics * 0.001;
    *time_delta = (float)(*time - prev_time);
    prev_time = *time;

    if ((*time - prev_fps_time) >= 0.5) {
        float fps_unfiltered = fps_frame / (float)(*time - prev_fps_time);

        *fps = fps_unfiltered;
        *ms = 1.0f / fps_unfiltered * 1000;
        Log("[%.1f fps %.3f ms]\n", *fps, *ms);

        prev_fps_time = *time;
        fps_frame = 1;
        return 1;
    }
    fps_frame++;
    return 0;
}
//=============================================================================
void Get_Window_Title(char* outTitle, const char* api_name)
{
    sprintf(outTitle, "Stardust 1.1");
}
//=============================================================================
void Enable_Logging(int enable)
{
    s_log_enabled = enable;
}
//=============================================================================
