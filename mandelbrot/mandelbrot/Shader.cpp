#include "Shader.h"
#include <GL/glew.h>

#include<iostream>
#include <fstream>
#include <sstream>

using namespace std;

Shader::Shader()	
	: m_ID(0)
{

	string shader = "#shader vertex\n"
		"#version 330 core\n"
		"layout(location = 0) in vec4 position;\n"
	"layout(location = 1) in vec2 texCoord;\n"
	"out vec2 v_TexCoord;\n"
	"void main()\n"
	"{\n"
		"gl_Position = position;\n"
		"v_TexCoord = texCoord;\n"
	"}\n"
	"\n"
	"#shader fragment\n"
		"#version 330 core\n"
		"layout(location = 0) out vec4 color;\n"
	"in vec2 v_TexCoord;\n"
	"uniform vec4 u_Color;\n"
	"uniform sampler2D u_Texture;\n"
	"void main()\n"
	"{\n"
		"vec4 texColor = texture(u_Texture, v_TexCoord);\n"
		"color = texColor;\n"
	"}\n";

	ShaderProgramSource source = ParseShader(shader);
	m_ID = CreateShader(source.VertexSource, source.FragmentSource);
}

Shader::~Shader()
{
	glDeleteProgram(m_ID);
}

ShaderProgramSource Shader::ParseShader(const string& filepath)
{
	enum class ShaderType
	{
		NONE = -1, VERTEX = 0, FRAGMENT = 1
	};

	stringstream iss(filepath);
	string line;

	stringstream ss[2];
	ShaderType type = ShaderType::NONE;

	while (getline(iss, line))
	{
		if (line.find("#shader") != string::npos)
		{
			if (line.find("vertex") != string::npos)
				type = ShaderType::VERTEX;
			else if (line.find("fragment") != string::npos)
				type = ShaderType::FRAGMENT;
		}
		else
		{
			ss[(int)type] << line << '\n';
		}
	}

	return { ss[0].str(), ss[1].str() };
}


unsigned int Shader::CompileShader(unsigned int type, const string& source)
{
	unsigned int id = glCreateShader(type);
	const char* src = source.c_str();
	glShaderSource(id, 1, &src, nullptr);
	glCompileShader(id);


	int result;
	glGetShaderiv(id, GL_COMPILE_STATUS, &result);

	if (result == GL_FALSE)
	{
		int length;
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
		char* message = (char*)alloca(length * sizeof(char));
		glGetShaderInfoLog(id, length, &length, message);
		cout << "Failed to compile "<< (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << "shader" << endl << message << endl;
		glDeleteShader(id);
		return 0;
	}

	return id;
}

unsigned int Shader::CreateShader(const string& vertexShader, const string& fragmentShader)
{
	// create a shader program
	unsigned int program = glCreateProgram();
	unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexShader);
	unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShader);

	glAttachShader(program, vs);
	glAttachShader(program, fs);

	glLinkProgram(program);

	GLint program_linked;

	glGetProgramiv(program, GL_LINK_STATUS, &program_linked);
	if (program_linked != GL_TRUE)
	{
		GLsizei log_length = 0;
		GLchar message[1024];
		glGetProgramInfoLog(program, 1024, &log_length, message);
		cout << "Failed to link program" << endl;
		cout << message << endl;
	}

	glValidateProgram(program);

	glDeleteShader(vs);
	glDeleteShader(fs);

	return program;
}

void Shader::Bind() const
{
	glUseProgram(m_ID);
}

void Shader::Unbind() const
{
	glUseProgram(0);
}

int Shader::GetUniformLocation(const string& name)
{
	if (m_UniformLocationCache.find(name) != m_UniformLocationCache.end())
		return m_UniformLocationCache[name];

	int location = glGetUniformLocation(m_ID, name.c_str());
	if (location == -1)
		cout << "Waring: uniform " << name << " not found" << endl;
	
	m_UniformLocationCache[name] = location;
	return location;
}

void Shader::SetUniform1i(const string& name, int value)
{
	glUniform1i(GetUniformLocation(name), value);
}


void Shader::SetUniform4f(const string& name, float v0, float v1, float v2, float v3)
{
	glUniform4f(GetUniformLocation(name), v0, v1, v2, v3);
}





