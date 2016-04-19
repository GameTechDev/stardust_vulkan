# Stardust

The Stardust sample application uses the Vulkan graphics API to efficiently render a cloud of animated particles. To highlight Vulkan’s low CPU overhead and multithreading capabilities, particles are rendered using 200,000 draw calls. The demo is not using instancing; each draw call uses different region of a vertex buffer so that each draw could be a unique piece of geometry. Furthermore, all CPU cores are used for draw call preparation to ensure that graphics commands are generated and delivered to the GPU as fast as possible. The per-core CPU load graph is displayed in the upper-right corner.

# Requirements
<ul>
<li>Visual Studio 2013 for building from source code (pre-built x64 binaries included).
<li>Vulkan capable hardware and drivers. For instance, Intel HD Graphics 520 or newer.
</ul>
