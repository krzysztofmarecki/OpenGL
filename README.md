# OpenGL

Various graphic effects implemented in OpenGL.

![](https://i.ibb.co/jbZBCp1/Bez-tytu-u.png)

#### Shadow mapping
- PCF (2 modes)
  - Disc sampling
    - 16x16x16 3D texture of random rotations tiled in world space
    - Vogel disc
      - Produces smoother penumbra than Poisson disc
      - Tiling is less visible than with Poisson disc
      - Poisson disc as fallback option is available in shader
  - Optimized fixed texel size PCF kernel using GatherCmp &#42;
- PCSS (only for Disc sampling) &#42;&#42;
  - Disc taps are presorted by distance from the kernel center
  - Use this kernel to estimate the average distance ratio
  - The amount of samples is reduced proportionally to the average distance ratio
    - This affects the radius of kernel, since the taps are sorted
  - Use only the reduced amount of samples for final shadow computation
- Cascade shadow mapping
  - stable &#42;
  - projection base cascade selection &#42;
  - smooth transition across cascades &#42;
  - partitioning using mix between logarithmic and linear
  - hardcoded near and far (no bounding boxes)

&#42; A Sampling of Shadow Techniques https://therealmjp.github.io/posts/shadow-maps/  
&#42;&#42; Playing with Real-Time Shadows https://www.realtimeshadows.com/sites/default/files/Playing%20with%20Real-Time%20Shadows_0.pdf
#### SMAA integration
- T2x, 1x
- blend weights looks different than in reference demo&#42; due to clip origin in lower left corner
  - with upper left looks the same
- for 1x final image exactly matches reference demo
- non zero subsample indicies produce worse edges, hence disabled by default

&#42; https://github.com/iryoku/smaa
#### Normal mapping
- world space

#### Reverse-Z
- depth buffer and shadow maps
- comparison function: GEQUAL

#### Eye adaptation
- log-average
- based on pure (without affecting surface) diffuse light generated on main pass (MRT)
- no lag

#### Other
- deferred shading
- G-Buffer:
  - RGBA8 - RGB diffuse, A specular
  - RGB10A2 - RGB ws normals, A unused
- Other RTs:
  - R16F - log pure diffuse light
  - RGBA16F - RGB final HDR RT, A unsused
- lightning model: Blinn-Phong

## Build Instructions
The repository contains Visual Studio 2017 project and solution file and all external dependencies.  
Requires C++17 compiler and OpenGL 4.5.

##### Controls
Press
"4" and "6" on numpad to move the sun  
"N" to toggle normal mapping  
"G" to show shadow maps
"H" to switch between visualized shadow maps
"Q" and "E" to change exposure.  
"R" and "T" to decrease/increase bias (constant value added to normalized depth)  
"Y" and "U" to decrease/increase scale of normal offset bias  
"," and "." to decrease/increase size of ligh source for PCSS  
"Z" to cycle through no AA, Smaa 1x, Smaa T2x
"X" to toggle subsample indicies for Smaa
Scroll mouse wheel to change FOV.


##### Sponza scene
From https://github.com/SaschaWillems/VulkanSponza
Textures were converted to tga.