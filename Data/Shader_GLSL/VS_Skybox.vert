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

#version 430 core

layout(location = 0) in vec4 vs_in_position;

layout(binding = 0, std430) buffer CONSTANT
{
  mat4 viewproj;
  uint data[48];
  float palette_factor;
} g_constant_buffer;

out gl_PerVertex
{
  vec4 gl_Position;
};

out INVOCATION
{
  vec3 texcoord;
} vs_out;


void main(void)
{
  vec4 p = vs_in_position;
  mat4 m = g_constant_buffer.viewproj;
  m[3] = vec4(0.0, 0.0, 0.0, 1.0);
  p = m * p;
  gl_Position = p.xyww;
  vs_out.texcoord = vs_in_position.xyz;
}
