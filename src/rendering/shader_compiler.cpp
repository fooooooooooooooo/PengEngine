#include "shader_compiler.h"

#include <core/logger.h>
#include <utils/io.h>

using namespace rendering;

PreprocessedShader ShaderCompiler::preprocess_shader(const std::string& path, ShaderType type) const
{
	Logger::log("Loading %s shader '%s'", strtools::cat(type).c_str(), path.c_str());
	const std::string shader_src = io::read_text_file(path);

	return preprocess_shader(path, type, shader_src);
}

PreprocessedShader ShaderCompiler::preprocess_shader(const std::string& /*path*/, ShaderType type, const std::string& src) const
{
	PreprocessedShader shader;
	shader.type = type;
	shader.contents = src;

	return shader;
}

GLuint ShaderCompiler::compile_shader(const PreprocessedShader& preprocessed_shader) const
{
	Logger::log("Compiling %s shader", strtools::cat(preprocessed_shader.type).c_str());

	const GLuint shader = glCreateShader(to_opengl(preprocessed_shader.type));
	const char* shader_src = preprocessed_shader.contents.c_str();
	glShaderSource(shader, 1, &shader_src, nullptr);
	glCompileShader(shader);

	return shader;
}

void ShaderCompiler::add_include_path(const std::string& include_path)
{
	_include_roots.push_back(include_path);
}
