#include "peng_engine.h"

#include <stdio.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <utils/timing.h>

PengEngine::PengEngine()
	: _target_frametime(1000 / 60.0)
	, _last_frametime(0)
	, _last_main_frametime(0)
	, _last_render_frametime(0)
	, _last_opengl_frametime(0)
	, _last_draw_time(timing::clock::now())
	, _render_thread("RenderThread")
	, _glfw_window(nullptr)
{ }

void PengEngine::start()
{
	_executing = true;
	start_opengl();

	while (!shutting_down())
	{
		_last_frametime = timing::measure_ms([this] {
			tick_main();
			tick_render();
			tick_opengl();
			finalize_frame();
		});

		printf("Frametime = %.02fms\n", _last_frametime);
	}

	shutdown();
}

void PengEngine::request_shutdown()
{
	_executing = false;
}

void PengEngine::set_target_fps(double fps) noexcept
{
	set_target_frametime(1000 / fps);
}

void PengEngine::set_target_frametime(double frametime_ms) noexcept
{
	_target_frametime = frametime_ms;
}

bool PengEngine::shutting_down() const
{
	if (!_executing)
	{
		return true;
	}

	if (_glfw_window && glfwWindowShouldClose(_glfw_window))
	{
		return true;
	}

	return false;
}

void PengEngine::start_opengl()
{
	const GLint Width = 800;
	const GLint Height = 600;

	if (!glfwInit())
	{
		printf("GLFW initialization failed\n");
		return;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	_glfw_window = glfwCreateWindow(Width, Height, "PengEngine", nullptr, nullptr);
	if (!_glfw_window)
	{
		printf("GLFW window creation failed\n");
		return;
	}

	int32_t bufferWidth;
	int32_t bufferHeight;
	glfwGetFramebufferSize(_glfw_window, &bufferWidth, &bufferHeight);
	glfwMakeContextCurrent(_glfw_window);

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
	{
		printf("GLEW initialization failed\n");
		return;
	}

	glViewport(0, 0, bufferWidth, bufferHeight);
}

void PengEngine::shutdown()
{
	shutdown_opengl();
}

void PengEngine::shutdown_opengl()
{
	if (_glfw_window)
	{
		glfwDestroyWindow(_glfw_window);
		_glfw_window = nullptr;
	}

	glfwTerminate();
}

void PengEngine::tick_main()
{
	_last_main_frametime = timing::measure_ms([this] {

	});
}

void PengEngine::tick_render()
{
	_last_render_frametime = timing::measure_ms([this] {

	});
}

void PengEngine::tick_opengl()
{
	_last_opengl_frametime = timing::measure_ms([this] {
		glfwPollEvents();

		static double time = 0;
		time += _target_frametime;

		glClearColor(0.5f, 0.75 + static_cast<GLclampf>(std::sin(time / 500)) / 4, 0.5f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	});
}

void PengEngine::finalize_frame()
{
	timing::clock::time_point sync_point = 
		_last_draw_time 
		+ std::chrono::duration_cast<timing::clock::duration>(timing::duration_ms(_target_frametime));

	timing::sleep_until_precise(sync_point);

	_last_draw_time = sync_point;
	glfwSwapBuffers(_glfw_window);
}
