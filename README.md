# Space Invaders

Space invaders clone written using C++ and OpenGL

Code from the C++ [tutorial](http://nicktasios.nl/posts/space-invaders-from-scratch-part-1.html) by Nick Tasios.

You need to install GLFW:  
Ubuntu: sudo apt-get install glfw3 glew  
OSX: brew install glfw glew  

To compile:  
Linux: g++ -std=c++11 -o main -lglfw -lGLEW -lGL main.cpp  
OSX: g++ -std=c++11 -o main -lglfw -lglew -framework OpenGL main.cpp  

## Some concepts

*Shader:* A user defined program to run on some stage of a graphics processor. OpenGL defines a rendering pipeline, and shaders execute at different stages of the pipeline. Vertex and Fragment shaders are two most important type of shaders. Vertex handle the processing of vertex data to transform objects to screen-space coordinates. The objects processed by vertex shaders are broken down into fragments and fragment shaders processes these fragments.  

*Rendering Pipeline:* Sequence of steps taken by OpenGL when rendering objects.
