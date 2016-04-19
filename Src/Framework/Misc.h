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
