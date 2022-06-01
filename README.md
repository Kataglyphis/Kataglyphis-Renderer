# GraphicsEngineVulkan
<h1 align="center">
  <br>
  <a href="https://jotrocken.blog/"><img src="images/logo.png" alt="OpenGLEngine" width="200"></a>
  <br>
  Cataglyphis Renderer 
  <br>
</h1>

<h1 align="center">
  <br>
  <a href="https://jotrocken.blog/"><img src="images/vulkan-logo.png" alt="OpenGLEngine" width="200"></a>
  <a href="https://jotrocken.blog/"><img src="images/Engine_logo.png" alt="OpenGLEngine" width="200"></a>
  <a href="https://jotrocken.blog/"><img src="images/glm_logo.png" alt="OpenGLEngine" width="200"></a>
</h1>

<h4 align="center">A graphics engine built on top of Vulkan <a href="https://jotrocken.blog/" target="_blank"></a>.</h4>

[![Linux build](https://github.com/Kataglyphis/GraphicsEngineVulkan/actions/workflows/Linux.yml/badge.svg)](https://github.com/Kataglyphis/GraphicsEngineVulkan/actions/workflows/Linux.yml)
[![Windows build](https://github.com/Kataglyphis/GraphicsEngineVulkan/actions/workflows/Windows.yml/badge.svg)](https://github.com/Kataglyphis/GraphicsEngineVulkan/actions/workflows/Windows.yml)

<p align="center">
  <a href="#key-features">Key Features</a> •
  <a href="#how-to-use">How To Use</a> •
  <a href="#download">Download</a> •
  <a href="#credits">Credits</a> •
  <a href="#related">Related</a> •
  <a href="#license">License</a>
</p>

<!-- TABLE OF CONTENTS -->
<details open="open">
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
      <ul>
        <li><a href="#built-with">Built With</a></li>
      </ul>
      <ul>
        <li><a href="#key-features">Key Features</a></li>
      </ul>
    </li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#installation">Installation</a></li>
        <li><a href="#shaders">Shaders</a></li>
      </ul>
    </li>
    <li><a href="#usage">Usage</a></li>
    <li><a href="#roadmap">Roadmap</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
    <li><a href="#acknowledgements">Acknowledgements</a></li>
  </ol>
</details>

<!-- ABOUT THE PROJECT -->
## About The Project

[![Kataglyphis Engine][product-screenshot]](https://jotrocken.blog/)

This project provides me a solid Vulkan renderer starting point
for implementing modern established rendering techniques and getting quickly
started in own research topics. 

### Key Features

<!-- ❌  -->
|          Feature                    |   Implement Status |
| ------------------------------------| :----------------: |
| Rasterizer                          |         ✔️         |
| Raytracing                          |         ✔️         |
| Path tracing                        |         ✔️         |
| PBR support (UE4,disney,... etc.)   |         ✔️         |
| .obj Model loading                  |         ✔️         |
| Mip Mapping                         |         ✔️         |

### Built With

* [Vulkan 1.3](https://www.vulkan.org/)
* [glm](https://github.com/g-truc/glm)
* [glfw](https://www.glfw.org/)
* [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)
* [stb](https://github.com/nothings/stb)
* [vma](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
* [tinygltf](https://github.com/syoyo/tinygltf)
* [doxygen](https://www.doxygen.nl/index.html)
* [gtest](https://github.com/google/googletest)
* [cmake](https://cmake.org/)

### Useful tools (you might also considering :) )

* [cppcheck](https://cppcheck.sourceforge.io/)
* [renderdoc](https://renderdoc.org/)
* [nsightgraphics](https://developer.nvidia.com/nsight-graphics)

<!-- GETTING STARTED -->
## Getting Started

You might only clone the repo, run cmake and you are good to go.
I only guarantee a successful build for windows hence it is frequently tested.

### Prerequisites

You will need [Vulkan 1.3] (https://vulkan.lunarg.com/). If you want to build documantaries you will need [Doxygen] (https://www.doxygen.nl/index.html).

### Installation

1. Clone the repo
   ```sh
   git clone --recurse-submodules git@github.com:Kataglyphis/GraphicsEngineVulkan.git
   ```


<!-- USAGE EXAMPLES -->
## Usage

_For more examples, please refer to the [Documentation](https://jotrockenmitlocken.de/)_


<!-- ROADMAP -->
## Roadmap
Upcoming :)
<!-- See the [open issues](https://github.com/othneildrew/Best-README-Template/issues) for a list of proposed features (and known issues). -->



<!-- CONTRIBUTING -->
## Contributing

Contributions are what make the open source community such an amazing place to be learn, inspire, and create. Any contributions you make are **greatly appreciated**.

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the Branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request


<!-- LICENSE -->
## License

Distributed under the BSD 3-Clause "New" or "Revised" License. See `LICENSE` for more information.


<!-- CONTACT -->
## Contact

Jonas Heinle - [@Cataglyphis_](https://twitter.com/Cataglyphis_) - jonasheinle@googlemail.com

Project Link: [https://github.com/Kataglyphis/GraphicsEngineVulkan](https://github.com/Kataglyphis/GraphicsEngineVulkan)



<!-- ACKNOWLEDGEMENTS -->
## Acknowledgements

You will find important links to information in the code.
But here in general some good sources of information:

Thanks for free 3D Models: 
* [Morgan McGuire, Computer Graphics Archive, July 2017 (https://casual-effects.com/data)](http://casual-effects.com/data/)
* [Viking room](https://sketchfab.com/3d-models/viking-room-a49f1b8e4f5c4ecf9e1fe7d81915ad38)

## Literature 

Some very helpful literature, tutorials, etc. 

Vulkan
* [Udemy course by Ben Cook](https://www.udemy.com/share/102M903@JMHgpMsdMW336k2s5Ftz9FMx769wYAEQ7p6GMAPBsFuVUbWRgq7k2uY6qBCG6UWNPQ==/)
* [Vulkan Tutorial](https://vulkan-tutorial.com/)
* [Vulkan Raytracing Tutorial](https://developer.nvidia.com/rtx/raytracing/vkray)
* [Vulkan Tutorial; especially chapter about integrating imgui](https://frguthmann.github.io/posts/vulkan_imgui/)
* [NVidia Raytracing tutorial with Vulkan](https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/)
* [Blog from Sascha Willems](https://www.saschawillems.de/)

Physically Based Shading
* [The Bible: PBR book](https://pbr-book.org/3ed-2018/Reflection_Models/Microfacet_Models)
* [Real shading in Unreal engine 4](https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf)
* [Physically Based Shading at Disney](https://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf)
* [RealTimeRendering](https://www.realtimerendering.com/)
* [Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs](https://hal.inria.fr/hal-01024289/)
* [Sampling the GGX Distribution of Visible Normals](https://pdfs.semanticscholar.org/63bc/928467d760605cdbf77a25bb7c3ad957e40e.pdf)

Path tracing
*[NVIDIA Path tracing Tutorial](https://github.com/nvpro-samples/vk_mini_path_tracer/blob/main/vk_mini_path_tracer/main.cpp)

<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[contributors-shield]: https://img.shields.io/github/contributors/othneildrew/Best-README-Template.svg?style=for-the-badge
[contributors-url]: https://github.com/othneildrew/Best-README-Template/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/othneildrew/Best-README-Template.svg?style=for-the-badge
[forks-url]: https://github.com/othneildrew/Best-README-Template/network/members
[stars-shield]: https://img.shields.io/github/stars/othneildrew/Best-README-Template.svg?style=for-the-badge
[stars-url]: https://github.com/othneildrew/Best-README-Template/stargazers
[issues-shield]: https://img.shields.io/github/issues/othneildrew/Best-README-Template.svg?style=for-the-badge
[issues-url]: https://github.com/othneildrew/Best-README-Template/issues
[license-shield]: https://img.shields.io/github/license/othneildrew/Best-README-Template.svg?style=for-the-badge
[license-url]: https://github.com/othneildrew/Best-README-Template/blob/master/LICENSE.txt
[linkedin-shield]: https://img.shields.io/badge/-LinkedIn-black.svg?style=for-the-badge&logo=linkedin&colorB=555
[linkedin-url]: https://www.linkedin.com/in/jonas-heinle-0b2a301a0/
[product-screenshot]: images/Screenshot.png



