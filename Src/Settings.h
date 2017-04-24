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
