// Copyright 2016 Intel Corporation All Rights Reserved
// 
// Intel makes no representations about the suitability of this software for any purpose.
// THIS SOFTWARE IS PROVIDED ""AS IS."" INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES,
// EXPRESS OR IMPLIED, AND ALL LIABILITY, INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES,
// FOR THE USE OF THIS SOFTWARE, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY
// RIGHTS, AND INCLUDING THE WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
// Intel does not assume any responsibility for any errors which may appear in this software
// nor any responsibility to update it.

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
