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

layout(location = 0) in vec2 vs_in_vertex;
layout(location = 1) in vec4 vs_in_color;

out gl_PerVertex
{
  vec4 gl_Position;
};

out INVOCATION
{
  vec4 color;
} vs_out;

void main(void)
{
  gl_Position = vec4(vs_in_vertex, 0.0, 1.0);
  vs_out.color = vs_in_color;
}
