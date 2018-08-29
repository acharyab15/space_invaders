#include <cstdio>
#include <cstdint>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Buffer represents pixels on the screen
struct Buffer
{
    size_t width, height;
    // Using uint32_t allows to store 4 8-bit color values for each pixel
    uint32_t* data;
};

// A blob of heap-allocated data, along with width and height of the sprite
// Sprite represented as a bitmap -- each pixel represented by a single bit 1 == on
struct Sprite
{
    size_t width, height;
    uint8_t* data;
};

// Position x,y in pixels from the bottom left corner of window
struct Alien 
{
    size_t x,y;
    uint8_t type;
};

// Position x,y in pixels from the bottom left corner of window
// Number of lvies of the player
struct Player
{
    size_t x,y;
    size_t life;
};

// Height and width of the game in pixels,
struct Game 
{
    size_t width, height;
    size_t num_aliens;
    Alien* aliens;
    Player player;
};

struct SpriteAnimation
{
    // if we should loop over animation or play it only once
    bool loop;
    size_t num_frames;
    size_t frame_duration;
    // time between successive frames
    size_t time;
    Sprite** frames;
};

void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

// Goes over sprite pixels and draws "on" pixels 
// at specified coordinates if within buffer bounds
void buffer_draw_sprite(Buffer* buffer, const Sprite& sprite, size_t x, size_t y, uint32_t color)
{
    for(size_t xi = 0; xi < sprite.width; ++xi)
    {
	for(size_t yi = 0; yi < sprite.height; ++yi)
	{
	    if(sprite.data[yi * sprite.width + xi] && 
		    (sprite.height - 1 + y - yi) < buffer-> height &&
		    (x + xi) < buffer->width)
	    {
		buffer->data[(sprite.height - 1 + y - yi) * buffer->width + (x + xi)] = color;
	    }
	}
    }
}


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

void validate_shader(GLuint shader, const char* file = 0)
{
    static const unsigned int BUFFER_SIZE = 512;
    char buffer[BUFFER_SIZE];
    GLsizei length = 0;

    glGetShaderInfoLog(shader, BUFFER_SIZE, &length, buffer);

    if(length > 0)
    {
	printf("Shader %d(%s) compile error: %s\n", 
		shader, (file ? file: ""), buffer);
    }
}


bool validate_program(GLuint program)
{
    static const GLsizei BUFFER_SIZE = 512;
    GLchar buffer[BUFFER_SIZE];
    GLsizei length = 0;

    glGetProgramInfoLog(program, BUFFER_SIZE, &length, buffer);

    if(length > 0)
    {
	printf("Program %d link error: %s\n", program, buffer);
	return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    const size_t buffer_width = 224;
    const size_t buffer_height = 256;

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

    // Turn V-Sync on to syncrhonize video card updates
    // with monitor refresh rate. For 60Hz refresh rate, framerate of game is 60
    glfwSwapInterval(1);

    // infinite game loop to process input and update and redraw game
    // set the buffer clear color for glClear to red
    glClearColor(1.0, 0.0, 0.0, 1.0);

    // Create graphics buffer
    Buffer buffer;
    buffer.width = buffer_width;
    buffer.height = buffer_height;
    buffer.data = new uint32_t[buffer.width * buffer.height];
    buffer_clear(&buffer, 0); 

    // Texture holds image data 
    // as well as information about formatting of the data
    GLuint buffer_texture;
    glGenTextures(1, &buffer_texture);

    // Specify image format and behavior of sampling of the texture
    glBindTexture(GL_TEXTURE_2D, buffer_texture);
    // Image should 8-bit rgb format to represent texture internally
    glTexImage2D(
	    GL_TEXTURE_2D, 0, GL_RGB8,
	    buffer.width, buffer.height, 0, 
	    // each pixel is in rgba format 
	    // and represented as 4 unsigned 8-bit integers
	    GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, buffer.data
	    );
    // Tell gpu to not apply any filtering when rading pixels
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // Tell gpu to read value at the edges if it tries to read beyond texture bounds
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


    // Vertex Array objects store format of vertex data along with vertex data
    GLuint fullscreen_triangle_vao;
    glGenVertexArrays(1, &fullscreen_triangle_vao);



    // Generate a quad the covers the screen
    const char* vertex_shader =
	"\n"
	"#version 330\n"
	"\n"
	"noperspective out vec2 TexCoord;\n"
	"\n"
	"void main(void){\n"
	"\n"
	"    TexCoord.x = (gl_VertexID == 2)? 2.0: 0.0;\n"
	"    TexCoord.y = (gl_VertexID == 1)? 2.0: 0.0;\n"
	"    \n"
	"    gl_Position = vec4(2.0 * TexCoord - 1.0, 0.0, 1.0);\n"
	"}\n";

    // Sample the buffer texture and output the result of the sampling
    // Output of the vertex_shader, TexCoord, is an input to the fragment shader
    const char* fragment_shader =
	"\n"
	"#version 330\n"
	"\n"
	"uniform sampler2D buffer;\n"
	"noperspective in vec2 TexCoord;\n"
	"\n"
	"out vec3 outColor;\n"
	"\n"
	"void main(void){\n"
	"    outColor = texture(buffer, TexCoord).rgb;\n"
	"}\n";


    GLuint shader_id = glCreateProgram();

    // Create vertex shader
    {
	GLuint shader_vp = glCreateShader(GL_VERTEX_SHADER);

	glShaderSource(shader_vp, 1, &vertex_shader, 0);
	glCompileShader(shader_vp);
	validate_shader(shader_vp, vertex_shader);
	glAttachShader(shader_id, shader_vp);

	glDeleteShader(shader_vp);
    }

    // Create fragment shader
    { 
	GLuint shader_fp = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(shader_fp, 1, &fragment_shader, 0);
	glCompileShader(shader_fp);
	validate_shader(shader_fp, fragment_shader);
	glAttachShader(shader_id, shader_fp);

	glDeleteShader(shader_fp);
    }

    // Program is linked using this 
    glLinkProgram(shader_id);

    if(!validate_program(shader_id))
    {
	fprintf(stderr, "Error while validating shader.\n");
	glfwTerminate();
	glDeleteVertexArrays(1, &fullscreen_triangle_vao);
	delete[] buffer.data;
	return -1;
    }

    glUseProgram(shader_id);

    // Get the location of the uniform in the shader and 
    // set the uniform to texture unit '0'
    GLint location = glGetUniformLocation(shader_id, "buffer");
    glUniform1i(location, 0);



    //OpenGL setup
    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);


    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(fullscreen_triangle_vao);

    // Prepare game
    Sprite alien_sprite0;
    alien_sprite0.width = 11;
    alien_sprite0.height = 8;
    alien_sprite0.data = new uint8_t[11 * 8]
    {
	0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
	    0,0,0,1,0,0,0,1,0,0,0, // ...@...@...
	    0,0,1,1,1,1,1,1,1,0,0, // ..@@@@@@@..
	    0,1,1,0,1,1,1,0,1,1,0, // .@@.@@@.@@.
	    1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
	    1,0,1,1,1,1,1,1,1,0,1, // @.@@@@@@@.@
	    1,0,1,0,0,0,0,0,1,0,1, // @.@.....@.@
	    0,0,0,1,1,0,1,1,0,0,0  // ...@@.@@...
    };

    Sprite alien_sprite1;
    alien_sprite1.width = 11;
    alien_sprite1.height = 8;
    alien_sprite1.data = new uint8_t[88]
    {
	0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
	    1,0,0,1,0,0,0,1,0,0,1, // @..@...@..@
	    1,0,1,1,1,1,1,1,1,0,1, // @.@@@@@@@.@
	    1,1,1,0,1,1,1,0,1,1,1, // @@@.@@@.@@@
	    1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
	    0,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@.
	    0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
	    0,1,0,0,0,0,0,0,0,1,0  // .@.......@.
    };

    Sprite player_sprite;
    player_sprite.width = 11;
    player_sprite.height = 7;
    player_sprite.data = new uint8_t[player_sprite.width * player_sprite.height]
    {
	0,0,0,0,0,1,0,0,0,0,0, // .....@.....
	    0,0,0,0,1,1,1,0,0,0,0, // ....@@@....
	    0,0,0,0,1,1,1,0,0,0,0, // ....@@@....
	    0,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@.
	    1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
	    1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
	    1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
    };

    SpriteAnimation* alien_animation = new SpriteAnimation;

    alien_animation->loop = true;
    alien_animation->num_frames = 2;
    alien_animation->frame_duration = 10;
    alien_animation->time = 0;

    alien_animation->frames = new Sprite*[2];
    alien_animation->frames[0] = &alien_sprite0;
    alien_animation->frames[1] = &alien_sprite1;

    Game game;
    game.width = buffer_width;
    game.height = buffer_height;
    game.num_aliens = 55;
    game.aliens = new Alien[game.num_aliens];

    game.player.x = 112 - 5;
    game.player.y = 32;

    game.player.life = 3;

    // Initialize all the alien positions to something reasonable
    for(size_t yi = 0; yi < 5; ++yi) {
	for(size_t xi = 0; xi < 11; ++xi) {
	    game.aliens[yi * 11 + xi].x = 16 * xi + 20;
	    game.aliens[yi * 11 + xi].y = 17 * yi + 128;
	}
    }

    // variable that controls player direction of movement
    int player_move_dir = 1;

    uint32_t clear_color = rgb_to_uint32(0, 128, 0);

    while (!glfwWindowShouldClose(window))
    {
	buffer_clear(&buffer, clear_color); // clear_color = green
	// Draw the player and all aliens
	for(size_t ai = 0; ai < game.num_aliens; ++ai)
	{
	    const Alien& alien = game.aliens[ai];
	    size_t current_frame = alien_animation->time / alien_animation->frame_duration;
	    const Sprite& sprite = *alien_animation->frames[current_frame];
	    buffer_draw_sprite(&buffer, sprite, alien.x, alien.y, rgb_to_uint32(128, 0, 0));
	}
	buffer_draw_sprite(&buffer, player_sprite, game.player.x, game.player.y, rgb_to_uint32(128, 0, 0));

	// Update animations by advancing time
	++alien_animation->time;
	// If it has reached its end, set its time back to 0 if is a looping animation; otherwise delete it
	if(alien_animation->time == alien_animation->num_frames * alien_animation->frame_duration)
	{
	    if(alien_animation->loop) alien_animation->time=0;
	    else
	    { 
		delete alien_animation;
		alien_animation = nullptr;
	    }
	}

	glTexSubImage2D(
		GL_TEXTURE_2D, 0, 0, 0,
		buffer.width, buffer.height,
		GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
		buffer.data
		);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	// front buffer is used for displaying, back buffer is used for drawing
	// swapping buffers at each iteration
	glfwSwapBuffers(window);

	// Basic collision detection of player sprite with the wall
	if(game.player.x + player_sprite.width + player_move_dir >= game.width -1)
	{
	    game.player.x = game.width - player_sprite.width - player_move_dir -1;
	    player_move_dir *= -1;
	}
	else if((int)game.player.x + player_move_dir <= 0)
	{
	    game.player.x = 0;
	    player_move_dir *= -1;
	}
	else game.player.x += player_move_dir;



	// processing any pending events
	glfwPollEvents();


    }
    glfwDestroyWindow(window);
    glfwTerminate();

    glDeleteVertexArrays(1, &fullscreen_triangle_vao);

    delete[] alien_sprite0.data;
    delete[] alien_sprite1.data;
    delete[] alien_animation->frames;
    delete[] buffer.data;
    delete[] game.aliens;

    delete alien_animation;

    return 0;

}
