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
