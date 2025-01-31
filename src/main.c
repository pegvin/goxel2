/* Goxel 3D voxels editor
 *
 * copyright (c) 2015-2022 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "goxel.h"

#ifdef GLES2
#   define GLFW_INCLUDE_ES2
#endif

#include <GLFW/glfw3.h>
#include <getopt.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
	#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup") // Removes The Console from launching in Background
#endif

bool shouldRender = true;
static inputs_t     *g_inputs = NULL;
static GLFWwindow   *g_window = NULL;
static float        g_scale = 1;

int g_framebuffSize[2], g_WinSize[2]; // Frame buffer size, window size
double xpos, ypos;
float scale;
inputs_t inputs = {};

typedef struct {
	char *input;
	char *export_path; // File Path To Save To
	float scale; // UI Scale
} args_t;

#define OPT_HELP 1
#define OPT_VERSION 2

typedef struct {
	const char *name;
	int val;
	int has_arg;
	const char *arg_name;
	const char *help;
} gox_option_t;

static const gox_option_t OPTIONS[] = {
	{"export", 'e', required_argument, "FILENAME",
		.help="Export the image to a file"},
	{"scale", 's', required_argument, "FLOAT", .help="Set UI scale"},
	{"help", OPT_HELP, .help="Give this help list"},
	{"version", OPT_VERSION, .help="Print program version"},
	{}
};

static void on_glfw_error(int code, const char *msg) {
	fprintf(stderr, "glfw error %d (%s)\n", code, msg);
	assert(false);
}

void on_scroll(GLFWwindow *win, double x, double y) {
	g_inputs->mouse_wheel = y;
}

void on_char(GLFWwindow *win, unsigned int c) {
	inputs_insert_char(g_inputs, c);
}

void on_drop(GLFWwindow* win, int count, const char** paths) {
	int i;
	for (i = 0;  i < count;  i++)
		goxel_import_file(paths[i], NULL);
}

void on_close(GLFWwindow *win) {
	glfwSetWindowShouldClose(win, GLFW_FALSE);
	gui_query_quit();
}

static void parse_options(int argc, char **argv, args_t *args) {
	int i, c, option_index;
	const gox_option_t *opt;
	struct option long_options[ARRAY_SIZE(OPTIONS)] = {};

	for (i = 0; i < (int)ARRAY_SIZE(OPTIONS); i++) {
		opt = &OPTIONS[i];
		long_options[i] = (struct option) {
			opt->name,
			opt->has_arg,
			NULL,
			opt->val,
		};
	}

	while (true) {
		c = getopt_long(argc, argv, "e:s:", long_options, &option_index);
		if (c == -1) break;
		switch (c) {
		case 'e':
			args->export_path = optarg;
			break;
		case 's':
			args->scale = atof(optarg);
			break;
		case OPT_HELP: {
			const gox_option_t *opt;
			char buf[128];

			printf("Usage: goxel2 [OPTION...] [INPUT]\n");
			printf("a 3D voxel art editor\n");
			printf("\n");

			for (opt = OPTIONS; opt->name; opt++) {
				if (opt->val >= 'a')
					printf("  -%c, ", opt->val);
				else
					printf("      ");

				if (opt->has_arg)
					snprintf(buf, sizeof(buf), "--%s=%s", opt->name, opt->arg_name);
				else
					snprintf(buf, sizeof(buf), "--%s", opt->name);
				printf("%-23s %s\n", buf, opt->help);
			}
			printf("\n");
			printf("Report bugs to https://github.com/pegvin/goxel2/issues\n");
			exit(0);
		}
		case OPT_VERSION:
			printf("Goxel2 " GOXEL_VERSION_STR "\n");
			exit(0);
		case '?':
			exit(-1);
		}
	}
	if (optind < argc) {
		args->input = argv[optind];
	}
}

#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 2
	static void load_icon(GLFWimage *image, const char *path) {
		uint8_t *img;
		int w, h, bpp = 0, size;
		const void *data;
		data = assets_get(path, &size);
		assert(data);
		img = img_read_from_mem((const char*)data, size, &w, &h, &bpp);
		assert(img);
		assert(bpp == 4);
		image->width = w;
		image->height = h;
		image->pixels = img;
	}

	static void set_window_icon(GLFWwindow *window) {
		GLFWimage icons[7];
		int i;
		load_icon(&icons[0], "asset://data/icons/icon16.png");
		load_icon(&icons[1], "asset://data/icons/icon24.png");
		load_icon(&icons[2], "asset://data/icons/icon32.png");
		load_icon(&icons[3], "asset://data/icons/icon48.png");
		load_icon(&icons[4], "asset://data/icons/icon64.png");
		load_icon(&icons[5], "asset://data/icons/icon128.png");
		load_icon(&icons[6], "asset://data/icons/icon256.png");
		glfwSetWindowIcon(window, 7, icons);
		for (i = 0; i < 7; i++) free(icons[i].pixels);
	}
#else
	static void set_window_icon(GLFWwindow *window) {}
#endif

static void set_window_title(void *user, const char *title) {
	glfwSetWindowTitle(g_window, title);
}

void window_size_cb(GLFWwindow* window, int width, int height) {
	g_WinSize[0] = width;
	g_WinSize[1] = height;
}

void window_iconify_callback(GLFWwindow* window, int iconified) {
	shouldRender = !iconified;
}

void window_focus_callback(GLFWwindow* window, int focused) {
	shouldRender = focused;
}

int main(int argc, char **argv) {
	args_t args = {.scale = 1};
	GLFWwindow *window;
	GLFWmonitor *monitor;
	const GLFWvidmode *mode;
	int width = 640, height = 480, ret = 0;

	// Setup sys callbacks.
	sys_callbacks.set_window_title = set_window_title;
	parse_options(argc, argv, &args);

	g_scale = args.scale;

	glfwSetErrorCallback(on_glfw_error);
	glfwInit();
	glfwWindowHint(GLFW_SAMPLES, 4);

	monitor = glfwGetPrimaryMonitor();
	mode = glfwGetVideoMode(monitor);

	if (mode) {
		width = mode->width ?: 640;
		height = mode->height ?: 480;
	}

	window = glfwCreateWindow(width, height, "Goxel2", NULL, NULL);
	assert(window);
	g_window = window;

	glfwMakeContextCurrent(window);
	glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, false);
	glfwGetWindowSize(g_window, &g_WinSize[0], &g_WinSize[1]);
	set_window_icon(window);

	glfwSetScrollCallback(window, on_scroll);
	glfwSetDropCallback(window, on_drop);
	glfwSetCharCallback(window, on_char);
	glfwSetWindowCloseCallback(window, on_close);
	glfwSetWindowSizeCallback(g_window, window_size_cb);
	glfwSetWindowIconifyCallback(g_window, window_iconify_callback);
	glfwSetWindowFocusCallback(g_window, window_focus_callback);

#ifdef WIN32
	glewInit();
#endif

	goxel_init();
	// Run the unit tests in debug.
	if (DEBUG) {
		tests_run();
		goxel_reset();
	}

	if (args.input)
		goxel_import_file(args.input, NULL);

	if (args.export_path) {
		if (!args.input) {
			LOG_E("trying to export an empty image");
			ret = -1;
		} else {
			ret = goxel_export_to_file(args.export_path, NULL);
		}

		goxel_release();
		glfwDestroyWindow(window);
		glfwTerminate();
		return ret;
	}

	g_inputs = &inputs;
	int i;

	glfwSwapInterval(goxel.vsyncEnabled == true ? 1 : 0); // No Swap Interval, Swap ASAP

#ifndef NDEBUG
	char title[256];
#endif

	double prevTime = 0.0;
	double currTime = 0.0;
	double timeDiff;
	unsigned int counter = 0;

	goxel.delta_time = 0.0;
	goxel.frame_time = 0.0;
	goxel.fps = 0.0;

	while (!glfwWindowShouldClose(g_window)) {
		currTime = glfwGetTime();
		timeDiff = currTime - prevTime;
		counter++;

		if (timeDiff >= 1.0 / 30.0) {
			goxel.delta_time = timeDiff;
			goxel.fps = (1.0 / timeDiff) * counter;
			goxel.frame_time = (timeDiff / counter) * 1000;

#ifndef NDEBUG
			sprintf(title, "Goxel2 - %f fps | %f ms", goxel.fps, goxel.frame_time);
			glfwSetWindowTitle(window, title);
#endif

			prevTime = currTime;
			counter = 0;
		}

		glfwPollEvents();

		// If Window is Not Visible, Focused Or Iconified Don't Render Anything
		if (!shouldRender || !glfwGetWindowAttrib(g_window, GLFW_VISIBLE)) {
			glfwWaitEvents();
			glfwPollEvents();

			if (goxel.quit) {
				break;
			}
			continue;
		}

		// The input struct gets all the values in framebuffer coordinates,
		// On retina display, this might not be the same as the window
		// size.
		glfwGetFramebufferSize(window, &g_framebuffSize[0], &g_framebuffSize[1]);
		scale = g_scale * (float)g_framebuffSize[0] / g_WinSize[0];

		g_inputs->window_size[0] = g_WinSize[0];
		g_inputs->window_size[1] = g_WinSize[1];
		g_inputs->scale = scale;

		GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

		for (i = GLFW_KEY_SPACE; i <= GLFW_KEY_LAST; i++) {
			g_inputs->keys[i] = glfwGetKey(g_window, i) == GLFW_PRESS;
		}

		glfwGetCursorPos(g_window, &xpos, &ypos);
		vec2_set(g_inputs->touches[0].pos, xpos, ypos);

		g_inputs->touches[0].down[0] = glfwGetMouseButton(g_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
		g_inputs->touches[0].down[1] = glfwGetMouseButton(g_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
		g_inputs->touches[0].down[2] = glfwGetMouseButton(g_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

		goxel_iter(g_inputs);
		goxel_render();

		memset(g_inputs, 0, sizeof(*g_inputs));
		glfwSwapBuffers(g_window);

		if (goxel.quit) break;
	}

	goxel_release();
	glfwDestroyWindow(window);
	glfwTerminate();
	return ret;
}
