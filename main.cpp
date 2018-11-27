#include <cstdio>
#include <cstdint>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

bool game_running = false;
int move_dir = 0;
bool fire_pressed = 0;

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

enum AlienType: uint8_t
{
	ALIEN_DEAD = 0,
	ALIEN_TYPE_A = 1,
	ALIEN_TYPE_B = 2,
	ALIEN_TYPE_C = 3
};

// Position x,y in pixels from the bottom left corner of window
// Number of lvies of the player
struct Player
{
	size_t x,y;
	size_t life;
};

// For the projectiles
// sign of dir indicates the direction of travel
struct Bullet
{
	size_t x, y;
	int dir;
};

#define GAME_MAX_BULLETS 128

// Height and width of the game in pixels,
struct Game 
{
	size_t width, height;
	size_t num_aliens;
	size_t num_bullets;
	Alien* aliens;
	Player player;
	Bullet bullets[GAME_MAX_BULLETS];
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

// Callbacks for different keys
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	switch(key) {
		case GLFW_KEY_ESCAPE:
			if(action == GLFW_PRESS) game_running = false;
			break;
		case GLFW_KEY_RIGHT:
			if(action == GLFW_PRESS) move_dir += 1;
			else if(action == GLFW_RELEASE) move_dir -= 1;
			break;
		case GLFW_KEY_LEFT:
			if(action == GLFW_PRESS) move_dir -= 1;
			else if(action == GLFW_RELEASE) move_dir += 1;
			break;
		case GLFW_KEY_SPACE:
			if(action == GLFW_RELEASE) fire_pressed = true;
			break;
		default:
			break;
	}
}

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

// Draw the text as a sprite at specified coordinates and with specified color
void buffer_draw_text(Buffer *buffer, const Sprite& text_spritesheet, const char* text, 
				size_t x, size_t y, uint32_t color){
		size_t xp = x;
		// stride is size of one character sprite (7x5 = 35)
		size_t stride = text_spritesheet.width * text_spritesheet.height;
		Sprite sprite = text_spritesheet;
		// iterate through all the characters in the text until the null character
		for(const char* charp = text; *charp != '\0'; ++charp)
		{
			char character = *charp - 32;
			if (character < 0 || character >= 65) continue;
			sprite.data = text_spritesheet.data + character * stride;
			buffer_draw_sprite(buffer, sprite, xp, y, color);
			xp += sprite.width + 1;
		}
}

// Draw numbers
void buffer_draw_number(Buffer* buffer, const Sprite& number_spritesheet, size_t number,
				size_t x, size_t y, uint32_t color)
{
		uint8_t digits[64];
		size_t num_digits = 0;

		size_t current_number = number;
		// Getting digits of the number 
		do
		{
				digits[num_digits++] = current_number % 10;
				current_number = current_number / 10;
		}
		while(current_number > 0);
		size_t xp = x;
		size_t stride = number_spritesheet.width * number_spritesheet.height;
		Sprite sprite = number_spritesheet;
		for(size_t i = 0; i < num_digits; ++i)
		{
				uint8_t digit = digits[num_digits - i - 1];
				sprite.data = number_spritesheet.data + digit * stride;
				buffer_draw_sprite(buffer, sprite, xp, y, color);
				xp += sprite.width + 1;
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

bool sprite_overlap_check(
		const Sprite& sp_a, size_t x_a, size_t y_a,
		const Sprite& sp_b, size_t x_b, size_t y_b)
{
	if(x_a < x_b + sp_b.width && x_a + sp_a.width > x_b &&
			y_a < y_b + sp_b.height && y_a + sp_a.height > y_b)
	{
		return true;
	}

	return false;
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
	window=glfwCreateWindow(2*buffer_width, 2*buffer_height, "Space Invaders", NULL, NULL);
	if(!window)
	{
		// destroy resources if any problems
		glfwTerminate();
		return -1;
	}

	// Set GLFW key callback 
	glfwSetKeyCallback(window, key_callback);

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
    printf("Renderer used: %s\n", glGetString(GL_RENDERER));
    printf("Shading Language: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

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

	glBindVertexArray(fullscreen_triangle_vao);

	// Prepare game
	Sprite alien_sprites[6];

	alien_sprites[0].width = 8;
	alien_sprites[0].height = 8;
	alien_sprites[0].data = new uint8_t[64]
	{
		0,0,0,1,1,0,0,0, // ...@@...
		0,0,1,1,1,1,0,0, // ..@@@@..
		0,1,1,1,1,1,1,0, // .@@@@@@.
		1,1,0,1,1,0,1,1, // @@.@@.@@
		1,1,1,1,1,1,1,1, // @@@@@@@@
		0,1,0,1,1,0,1,0, // .@.@@.@.
		1,0,0,0,0,0,0,1, // @......@
		0,1,0,0,0,0,1,0  // .@....@.
	};

	alien_sprites[1].width = 8;
	alien_sprites[1].height = 8;
	alien_sprites[1].data = new uint8_t[64]
	{
		0,0,0,1,1,0,0,0, // ...@@...
		0,0,1,1,1,1,0,0, // ..@@@@..
		0,1,1,1,1,1,1,0, // .@@@@@@.
		1,1,0,1,1,0,1,1, // @@.@@.@@
		1,1,1,1,1,1,1,1, // @@@@@@@@
		0,0,1,0,0,1,0,0, // ..@..@..
		0,1,0,1,1,0,1,0, // .@.@@.@.
		1,0,1,0,0,1,0,1  // @.@..@.@
	};
	alien_sprites[2].width = 11;
	alien_sprites[2].height = 8;
	alien_sprites[2].data = new uint8_t[88]
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

	alien_sprites[3].width = 11;
	alien_sprites[3].height = 8;
	alien_sprites[3].data = new uint8_t[88]
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
	alien_sprites[4].width = 12;
	alien_sprites[4].height = 8;
	alien_sprites[4].data = new uint8_t[96]
	{
		0,0,0,0,1,1,1,1,0,0,0,0, // ....@@@@....
		0,1,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@@.
		1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
		1,1,1,0,0,1,1,0,0,1,1,1, // @@@..@@..@@@
		1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
		0,0,0,1,1,0,0,1,1,0,0,0, // ...@@..@@...
		0,0,1,1,0,1,1,0,1,1,0,0, // ..@@.@@.@@..
		1,1,0,0,0,0,0,0,0,0,1,1  // @@........@@
	};

	alien_sprites[5].width = 12;
	alien_sprites[5].height = 8;
	alien_sprites[5].data = new uint8_t[96]
	{
		0,0,0,0,1,1,1,1,0,0,0,0, // ....@@@@....
		0,1,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@@.
		1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
		1,1,1,0,0,1,1,0,0,1,1,1, // @@@..@@..@@@
		1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
		0,0,1,1,1,0,0,1,1,1,0,0, // ..@@@..@@@..
		0,1,1,0,0,1,1,0,0,1,1,0, // .@@..@@..@@.
		0,0,1,1,0,0,0,0,1,1,0,0  // ..@@....@@..
	};
	Sprite alien_death_sprite;
	alien_death_sprite.width = 13;
	alien_death_sprite.height = 7;
	alien_death_sprite.data = new uint8_t[alien_death_sprite.width * alien_death_sprite.height]
	{
		0,1,0,0,1,0,0,0,1,0,0,1,0, // .@..@...@..@.
		0,0,1,0,0,1,0,1,0,0,1,0,0, // ..@..@.@..@..
		0,0,0,1,0,0,0,0,0,1,0,0,0, // ...@.....@...
		1,1,0,0,0,0,0,0,0,0,0,1,1, // @@.........@@
		0,0,0,1,0,0,0,0,0,1,0,0,0, // ...@.....@...
		0,0,1,0,0,1,0,1,0,0,1,0,0, // ..@..@.@..@..
		0,1,0,0,1,0,0,0,1,0,0,1,0  // .@..@...@..@.
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

	Sprite text_spritesheet;
	text_spritesheet.width = 5;
	text_spritesheet.height = 7;
	// 65 5x7 characters starting from 'space' at 32 in ASCII to '`' ASCII 96
	text_spritesheet.data = new uint8_t[65 * 35]
	{
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,
        0,1,0,1,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,1,0,1,0,0,1,0,1,0,1,1,1,1,1,0,1,0,1,0,1,1,1,1,1,0,1,0,1,0,0,1,0,1,0,
        0,0,1,0,0,0,1,1,1,0,1,0,1,0,0,0,1,1,1,0,0,0,1,0,1,0,1,1,1,0,0,0,1,0,0,
        1,1,0,1,0,1,1,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,1,1,0,1,0,1,1,
        0,1,1,0,0,1,0,0,1,0,1,0,0,1,0,0,1,1,0,0,1,0,0,1,0,1,0,0,0,1,0,1,1,1,1,
        0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,
        1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,
        0,0,1,0,0,1,0,1,0,1,0,1,1,1,0,0,0,1,0,0,0,1,1,1,0,1,0,1,0,1,0,0,1,0,0,
        0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,1,1,1,1,1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
        0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,

        0,1,1,1,0,1,0,0,0,1,1,0,0,1,1,1,0,1,0,1,1,1,0,0,1,1,0,0,0,1,0,1,1,1,0,
        0,0,1,0,0,0,1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,1,1,0,
        0,1,1,1,0,1,0,0,0,1,0,0,0,0,1,0,0,1,1,0,0,1,0,0,0,1,0,0,0,0,1,1,1,1,1,
        1,1,1,1,1,0,0,0,0,1,0,0,0,1,0,0,0,1,1,0,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        0,0,0,1,0,0,0,1,1,0,0,1,0,1,0,1,0,0,1,0,1,1,1,1,1,0,0,0,1,0,0,0,0,1,0,
        1,1,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,1,1,1,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,1,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,

        0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,
        0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,
        0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,
        1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,
        0,1,1,1,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,
        0,1,1,1,0,1,0,0,0,1,1,0,1,0,1,1,1,0,1,1,1,0,1,0,0,1,0,0,0,1,0,1,1,1,0,

        0,0,1,0,0,0,1,0,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,0,0,0,1,
        1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,1,1,1,0,
        1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,
        1,1,1,1,1,1,0,0,0,0,1,0,0,0,0,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,1,1,1,1,1,
        1,1,1,1,1,1,0,0,0,0,1,0,0,0,0,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,1,0,1,1,1,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,
        0,1,1,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,1,1,0,
        0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,0,0,0,1,1,0,0,1,0,1,0,1,0,0,1,1,0,0,0,1,0,1,0,0,1,0,0,1,0,1,0,0,0,1,
        1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,1,1,1,
        1,0,0,0,1,1,1,0,1,1,1,0,1,0,1,1,0,1,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,
        1,0,0,0,1,1,0,0,0,1,1,1,0,0,1,1,0,1,0,1,1,0,0,1,1,1,0,0,0,1,1,0,0,0,1,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,1,0,1,1,0,0,1,1,0,1,1,1,1,
        1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,1,0,1,0,0,1,0,0,1,0,1,0,0,0,1,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,0,1,1,1,0,1,0,0,0,1,0,0,0,0,1,0,1,1,1,0,
        1,1,1,1,1,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,
        1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,
        1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,1,0,1,1,0,1,0,1,1,1,0,1,1,1,0,0,0,1,
        1,0,0,0,1,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,1,0,1,0,1,0,0,0,1,1,0,0,0,1,
        1,0,0,0,1,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,
        1,1,1,1,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,1,1,1,1,

        0,0,0,1,1,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,1,
        0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,
        1,1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,1,1,0,0,0,
        0,0,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
        0,0,1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

	// number_spritesheet is just a reference into text_spritesheet starting at position 16
	Sprite number_spritesheet = text_spritesheet;
	number_spritesheet.data += 16 *35;


	Sprite bullet_sprite;
	bullet_sprite.width = 1;
	bullet_sprite.height = 3;
	bullet_sprite.data = new uint8_t[3]
	{
		1, // @
			1, // @
			1  // @
	};

	SpriteAnimation alien_animation[3];

	for(size_t i=0; i<3; ++i)
	{
		alien_animation[i].loop = true;
		alien_animation[i].num_frames = 2;
		alien_animation[i].frame_duration = 10;
		alien_animation[i].time = 0;

		alien_animation[i].frames = new Sprite*[2];
		alien_animation[i].frames[0] = &alien_sprites[2 * i];
		alien_animation[i].frames[1] = &alien_sprites[2 * i +1];
	}

	Game game;
	game.width = buffer_width;
	game.height = buffer_height;
	game.num_aliens = 55;
    game.num_bullets = 0;
	game.aliens = new Alien[game.num_aliens];

	game.player.x = 112 - 5;
	game.player.y = 32;

	game.player.life = 3;

	// Initialize all the alien positions to something reasonable
	for(size_t yi = 0; yi < 5; ++yi)
    {
        for(size_t xi = 0; xi < 11; ++xi)
        {
            Alien& alien = game.aliens[yi * 11 + xi];
            alien.type = (5 - yi) / 2 + 1;

            const Sprite& sprite = alien_sprites[2 * (alien.type - 1)];

            alien.x = 16 * xi + 20 + (alien_death_sprite.width - sprite.width)/2;
            alien.y = 17 * yi + 128;
        }
}
	// Array of death counters
	uint8_t* death_counters = new uint8_t[game.num_aliens];
	for(size_t i = 0; i < game.num_aliens; ++i)
	{
		death_counters[i] = 10;
	}

	// set the game_running global to true
	game_running = true;
	size_t score = 0;
	size_t credits = 0;
	uint32_t clear_color = rgb_to_uint32(0, 128, 0);
    int player_move_dir = 0;

	while (!glfwWindowShouldClose(window) && game_running)
	{
		buffer_clear(&buffer, clear_color); // clear_color = green
		buffer_draw_text(
						&buffer,
						text_spritesheet, "SCORE",
						4, game.height -text_spritesheet.height - 7,
						rgb_to_uint32(128,0,0)
						);
		char credit_text[16];
		sprintf(credit_text, "CREDIT %02lu", credits);
		buffer_draw_text(
						&buffer,
						text_spritesheet, credit_text, 
						164, 7, 
						rgb_to_uint32(128, 0, 0)
						);

		buffer_draw_number(
						&buffer,
						number_spritesheet, score,
						4 + 2 * number_spritesheet.width, game.height - 2 * number_spritesheet.height - 12,
						rgb_to_uint32(128,0,0)
						);

		for(size_t i = 0; i < game.width; ++i)
		{
				buffer.data[game.width * 16 + i] = rgb_to_uint32(128, 0, 0);
		}

		// Draw the aliens  
		for(size_t ai = 0; ai < game.num_aliens; ++ai)
		{
			// draw the alien only if death counter is bigger than 0
			if(!death_counters[ai]) continue;

			const Alien& alien = game.aliens[ai];
			if(alien.type == ALIEN_DEAD)
			{
				buffer_draw_sprite(&buffer, alien_death_sprite, alien.x, alien.y, rgb_to_uint32(128, 0, 0));
			}
			else
			{
				const SpriteAnimation& animation = alien_animation[alien.type - 1];
				size_t current_frame = animation.time / animation.frame_duration;
				const Sprite& sprite = *animation.frames[current_frame];
				buffer_draw_sprite(&buffer, sprite, alien.x, alien.y, rgb_to_uint32(128,0,0));
			}
		}

		// Draw the bullets
		for(size_t bi = 0; bi < game.num_bullets; ++bi)
		{
			const Bullet& bullet = game.bullets[bi];
			const Sprite& sprite = bullet_sprite;
			buffer_draw_sprite(&buffer, sprite, bullet.x, bullet.y, rgb_to_uint32(128, 0, 0));
		}

		buffer_draw_sprite(&buffer, player_sprite, game.player.x, game.player.y, rgb_to_uint32(128, 0, 0));

		// Update animations
		for(size_t i = 0; i < 3; ++i)
		{
			++alien_animation[i].time;
			if(alien_animation[i].time == alien_animation[i].num_frames * alien_animation[i].frame_duration)
			{
				alien_animation[i].time = 0;
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


		// Simulate aliens. Decrease death counter every frame 
		for(size_t ai = 0; ai <game.num_aliens; ++ai)
		{
			const Alien& alien = game.aliens[ai];
			if(alien.type == ALIEN_DEAD && death_counters[ai])
			{
				--death_counters[ai];
			}
		}


		// Simulate bullets. Add dir, and remove projectiles that move out of game area
		for(size_t bi = 0; bi < game.num_bullets;)
		{
			game.bullets[bi].y += game.bullets[bi].dir;
			if(game.bullets[bi].y >= game.height ||
					game.bullets[bi].y < bullet_sprite.height)
			{
				game.bullets[bi] = game.bullets[game.num_bullets - 1];
				--game.num_bullets;
				continue;
			}

			// Check if a bullet its an alien that is alive 
			for(size_t ai = 0; ai < game.num_aliens; ++ai)
			{
				const Alien& alien = game.aliens[ai];
				if(alien.type == ALIEN_DEAD) continue;

				const SpriteAnimation& animation = alien_animation[alien.type - 1];
				size_t current_frame = animation.time / animation.frame_duration;
				const Sprite& alien_sprite = *animation.frames[current_frame];
				bool overlap = sprite_overlap_check(
						bullet_sprite, game.bullets[bi].x, game.bullets[bi].y,
						alien_sprite, alien.x, alien.y
						);
				if(overlap)
				{
					// Based on the alien type, add score between 10 - 40 points
					score += 10 * (4 - game.aliens[ai].type);
					game.aliens[ai].type = ALIEN_DEAD;
					// NOTE: Hack to recenter death sprite
					game.aliens[ai].x -= (alien_death_sprite.width - alien_sprite.width)/2;
					game.bullets[bi] = game.bullets[game.num_bullets - 1];
					--game.num_bullets;
					continue;
				}
			}
			++bi;	
		}

		// Simulate player
		// variable that controls player direction of movement
		int player_move_dir = move_dir;

		if(player_move_dir != 0){
			// Basic collision detection of player sprite with the wall
			if(game.player.x + player_sprite.width + player_move_dir >= game.width -1)
			{
				// can't go past boundary
				game.player.x = game.width - player_sprite.width;
			}
			else if((int)game.player.x + player_move_dir <= 0)
			{
				game.player.x = 0;
			}
			else game.player.x += player_move_dir;
		}
		
		// Process events
		if(fire_pressed && game.num_bullets < GAME_MAX_BULLETS)
		{
			game.bullets[game.num_bullets].x = game.player.x + player_sprite.width / 2;
			game.bullets[game.num_bullets].y = game.player.y + player_sprite.height;
			game.bullets[game.num_bullets].dir = 2;
			++game.num_bullets;
		}
		fire_pressed = false;


		// processing any pending events
		glfwPollEvents();
	}
	glfwDestroyWindow(window);
	glfwTerminate();

	glDeleteVertexArrays(1, &fullscreen_triangle_vao);
	for(size_t i = 0; i < 6; ++i)
	{
		delete[] alien_sprites[i].data;
	} 
	delete[] text_spritesheet.data;
	delete[] alien_death_sprite.data;
	for(size_t i = 0; i < 3; ++i)
	{
		delete[] alien_animation[i].frames;
	}
	delete[] buffer.data;
	delete[] game.aliens;
	delete[] death_counters;

	return 0;
}
