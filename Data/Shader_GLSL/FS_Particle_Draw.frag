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

in INVOCATION
{
  float texcoord;
} fs_in;

layout(binding = 0, std430) readonly buffer CONSTANT
{
  mat4 viewproj;
  uint data[48];
  float palette_factor;
} g_constant;

layout(binding = 3) uniform sampler2D g_palette_a;
layout(binding = 4) uniform sampler2D g_palette_b;

layout(location = 0) out vec4 fs_out_color;

void main(void)
{
  vec2 tc = vec2(fs_in.texcoord, 0.0);
  vec4 ca = texture(g_palette_a, tc);
  vec4 cb = texture(g_palette_b, tc);
  fs_out_color = 0.05 * mix(ca, cb, g_constant.palette_factor);
}
