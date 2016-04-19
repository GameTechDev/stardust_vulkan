// Copyright 2016 Intel Corporation All Rights Reserved
// 
// Intel makes no representations about the suitability of this software for any purpose.
// THIS SOFTWARE IS PROVIDED ""AS IS."" INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES,
// EXPRESS OR IMPLIED, AND ALL LIABILITY, INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES,
// FOR THE USE OF THIS SOFTWARE, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY
// RIGHTS, AND INCLUDING THE WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
// Intel does not assume any responsibility for any errors which may appear in this software
// nor any responsibility to update it.

// Number of cmdBuffer & other object siblings for inter-frame double buffering
#define k_Resource_Buffering 3

// The total number of points
#define k_Def_Point_Count 2000000

// Number of points per draw call
#define k_Def_Batch_Size 10

// Metrics graph settings
#define k_Graph_Samples 60
#define k_Graph_Width 200
#define k_Graph_Height 134

#define MAX_CPU_CORES 16
