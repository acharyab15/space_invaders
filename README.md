# Space Invaders

Space invaders clone written using C++ and OpenGL

Code from the C++ [tutorial](http://nicktasios.nl/posts/space-invaders-from-scratch-part-1.html) by Nick Tasios.

You need to install GLFW:  
Ubuntu: sudo apt-get install glfw3 glew  
OSX: brew install glfw glew  

To compile:  
Linux: g++ -std=c++11 -o main -lglfw -lGLEW -lGL main.cpp  
OSX: g++ -std=c++11 -o main -lglfw -lglew -framework OpenGL main.cpp  
