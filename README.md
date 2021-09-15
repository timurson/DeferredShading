# DeferredShading
Experimentation with doing deferred rendering using OpenGL.  I implemented this project almost entirely from scratch starting with a basic GLFW OpenGL framework and Assimp (Open Asset Import Library) for model loading.
I was already familiar with a concept of deferred shading from attending various GDC lectures in the past, but have not had a chance to really experiment with deferred lighting until now. Attempting to simulate thousands of local area lights in the scene is a pretty fascinating endeavor and I am really glad I had an opportunity to do that in this project.

## Features:
*  Support for rendering into multiple render targets using OpenGL’s frame buffer objects that I ended up wrapping into a custom class for ease of use.
*  Support for over a thousand (possibly more) local area lights that are being rendered as spherical volumes.
*  Blinn-Phong lighting calculation is being applied during the initial deferred global light with shadows pass and the final area lights rendering stage.
*  Used various Stanford 3D Models: Bunny, Lucy, Dragon, and others.
*  Utilizing Dear ImGui graphical user interface library for easy real-time debugging and configuration

![Alt Text](https://github.com/timurson/DeferredShading/blob/master/Image1.PNG)
![Alt Text](https://github.com/timurson/DeferredShading/blob/master/Image2.PNG)


## External Libraries Used:

[GLFW](https://www.glfw.org/download.html)
[GLAD](https://glad.dav1d.de/)
[GLM](https://glm.g-truc.net/0.9.8/index.html)
[Deam ImGui](https://github.com/ocornut/imgui)
[Assimp](http://assimp.org/index.php/downloads)
[STB](https://github.com/nothings)
[GLSW](https://prideout.net/blog/old/blog/index.html@p=11.html)

# License
Copyright (C) 2021 Roman Timurson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

