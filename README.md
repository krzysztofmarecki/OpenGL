# OpenGL

Various graphic effects implemented in OpenGL.

![](https://s6.ifotos.pl/img/Beztytulu_qapprpw.png)

#### Cascade shadow mapping
- stable &#42;
- projection base cascade selection &#42;
- smooth transition across cascades &#42;
- Optimized fixed size PCF kernel using GatherCmp &#42;
- partitioning using mix between logarithmic and linear
- hard coded near and far (no bounding boxes)

&#42; Based on https://therealmjp.github.io/posts/shadow-maps/

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
- forward rendering
- separate passes for alpha masked geometry
- lightning model: Blinn-Phong

# Build Instructions
The repository contains Visual Studio 2017 project and solution file and all external dependencies.
Requires C++17 compiler and OpenGL 4.5.

##### Controls
Press
"4" and "6" on numpad to move the sun
"N" to toggle normal mapping
"G" to show shadow maps (white - if nothing was rendered at pixel, black otherwise)
"H" to switch between visualized shadow maps
"Q" and "E" to change exposure.
"R" and "T" to decrease/increase bias (constant value added to normalized depth)
"Y" and "U" to decrease/increase scale of normal offset bias
Scroll mouse wheel to change FOV.

##### Sponza scene
From https://github.com/SaschaWillems/VulkanSponza
Textures were converted to tga.