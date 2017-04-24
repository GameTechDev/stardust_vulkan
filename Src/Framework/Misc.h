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

#include "stb_image.h"

char *Load_Text_File(const char *filename);
char *Load_Binary_File(const char *filename, int *ret_size);
stbi_uc *Load_Image(char const *filename, int *x, int *y, int *comp, int req_comp);

void Log(const char *fmt, ...);

static const char *Format_Log_Message(const char *function, int line)
{
    static char buffer[1024];
    sprintf(buffer, "%s:%d\n", function, line);
    Log(buffer);
    return buffer;
}

#ifdef _WIN32
#define __func__ __FUNCTION__
#endif

#define LOG_AND_RETURN0() return Log(Format_Log_Message(__func__, __LINE__)), 0

#define RND_GEN(x) (x = x * 196314165 + 907633515)

void Print_Current_Time(const char *message);

int Update_Frame_Stats(double *time, float *time_delta, int frame, int filter_fps, float *fps, float *ms);

void Get_Window_Title(char *outTitle, const char *api_name);
void Enable_Logging(int enable);
