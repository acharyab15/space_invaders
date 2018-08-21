#include <cstdio>
#include <cstdint>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

// Buffer represents pixels on the screen
struct Buffer
{
    size_t width, height;
    // Using uint32_t allows to store 4 8-bit color values for each pixel
    uint32_t* data;
};

// Sets the left most 24 bits to the r,g,b values respectively
// the right-most 8 bits are set to 255 (but not used)
uint32_t rgb_to_uint32(uint8_t r, uint8_t g, uint8_t b)
{
	return (r << 24) | (g << 16) | (b << 8) | 255;
}

// Clear the buffer to a certain color
// Iterate over all pixels and set each pixel to the give color
void buffer_clear(Buffer* buffer, uint32_t color)
{ 
	for(size_t i=0; i< buffer->width * buffer->height; ++i)
	{
		buffer->data[i] = color;
	}
}

int main(int argc, char* argv[]) {

	glfwSetErrorCallback(error_callback);
	GLFWwindow* window;
	if(!glfwInit())
	{
		return -1;
	}

	// Tell GLFW that a context that is at least version 3.3 is needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	// first NULL -- specifying a monitor for full-screen mode
	// second NULL -- sharing context between different windows
	window=glfwCreateWindow(640, 480, "Space Invaders", NULL, NULL);
	if(!window)
	{
		// destroy resources if any problems
		glfwTerminate();
		return -1;
	}
	// make subsequent OpenGL calls apply to the current context
	glfwMakeContextCurrent(window);
	
	// Initialize GLEW aftering making current context
	GLenum err = glewInit();
	if(err != GLEW_OK)
	{
		fprintf(stderr, "Error initializing GLEW.\n");
		glfwTerminate();
		return -1;
	}

	// Query the OpenGL version we got.
	int glVersion[2] = {-1, 1};
	glGetIntegerv(GL_MAJOR_VERSION, &glVersion[0]);
	glGetIntegerv(GL_MINOR_VERSION, &glVersion[1]);

	printf("Using OpenGL: %d.%d\n", glVersion[0], glVersion[1]);

	// infinite game loop to process input and update and redraw game
	// set the buffer clear color for glClear to red
	glClearColor(1.0, 0.0, 0.0, 1.0);

	// Create graphics buffer
	uint32_t clear_color = rgb_to_uint32(0, 128, 0);
	Buffer buffer;
	buffer.width = buffer_width;
	buffer.height = buffer_height;
	buffer.data = new uint32_t[buffer.width * buffer.height];
	buffer_clear(&buffer, clear_color); // clear_color = green





	while (!glfwWindowShouldClose(window))
	{
		glClear(GL_COLOR_BUFFER_BIT);
		// front buffer is used for displaying, back buffer is used for drawing
		// swapping buffers at each iteration
		glfwSwapBuffers(window);
		// processing any pending events
		glfwPollEvents();

	}
		glfwDestroyWindow(window);
		glfwTerminate();
		return 0;

}
