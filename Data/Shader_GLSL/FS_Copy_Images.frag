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
  vec2 texcoord;
} fs_in;

layout(binding = 1) uniform sampler2DArray g_src_texture;

layout(location = 0) out vec4 fs_out_color0;
layout(location = 1) out vec4 fs_out_color1;
layout(location = 2) out vec4 fs_out_color2;
layout(location = 3) out vec4 fs_out_color3;
layout(location = 4) out vec4 fs_out_color4;
layout(location = 5) out vec4 fs_out_color5;

void main(void)
{
  vec2 tc = vec2(fs_in.texcoord.x , fs_in.texcoord.y);
  fs_out_color0 = texture(g_src_texture, vec3(tc, 0.0));
  fs_out_color1 = texture(g_src_texture, vec3(tc, 1.0));
  fs_out_color2 = texture(g_src_texture, vec3(tc, 2.0));
  fs_out_color3 = texture(g_src_texture, vec3(tc, 3.0));
  fs_out_color4 = texture(g_src_texture, vec3(tc, 4.0));
  fs_out_color5 = texture(g_src_texture, vec3(tc, 5.0));
}
