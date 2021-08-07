# OpenGL

Various graphic effects implemented in OpenGL.

![](https://i.ibb.co/6br1K5x/Bez-tytu-u.png)

#### Shadow mapping
- High performance & quality PCSS
  - 10 + 10 taps
  - vogel disk
    - slightly better than poisson
  - blue noise
  - temporal disk rotaions
    - (almost) completely hides moving noise on camera movement
    - same rotations as for GTAO (it just works)
  - bilateral pixel wide blur

- Cascade shadow mapping
  - stable &#42;
  - projection base cascade selection &#42;
  - smooth transition across cascades &#42;
  - partitioning using mix between logarithmic and linear
  - hardcoded near and far (no bounding boxes)

&#42; A Sampling of Shadow Techniques https://therealmjp.github.io/posts/shadow-maps/
#### GTAO
- Temporal supersampling
- Half resolution
- During downsampling depth take farthest value
    - reduces occlusion on thin objects without affecting others
    - reduces halo
- During ray marching take farthest depth from Gather
    - reduces occlusion on thin objects without affecting others

https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf
https://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pptx
#### TAA
- 8x subpixel jittering using Halton(2, 3)
- neighborhood clipping in YCoCg
- disocclusion detection via depth desting
    - closest depth in current neighborhood
    - single depth from history
        - produses less flickering for some reason
- closest velocity
- distance to clamp
- history sampled with 5-tap Catmull-Rom

#### Filmic tone mapping
- Lottes's curve with Bart Wronski's fixes
- Cross talk uses max(rgb)
    - blends to pure white at white point

#### Eye adaptation
- log-average
- based on pure (without affecting surface) diffuse light

#### Normal mapping
- world space

#### Reverse-Z
- depth buffer and shadow maps
- comparison function: GEQUAL

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
"Z" to cycle toggle TAA
"F1" to toggle ambient occlusion
"F2" to show only ambient occlusion
"V" and "B" to decrease/increase size of kernel for ambient occlusion
"I" and "O" to decrease/increase rate of change of temporal supersampling for ambient occlusion

Scroll mouse wheel to change FOV.


##### Sponza scene
From https://github.com/SaschaWillems/VulkanSponza
Textures were converted to tga.