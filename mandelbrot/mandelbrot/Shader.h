#pragma once
#include <string>
#include <unordered_map>

using std::string;

struct ShaderProgramSource
{
	string VertexSource;
	string FragmentSource;
};

class Shader
{
private:
	unsigned int m_ID;
	string m_FilePath;
	std::unordered_map<string, int> m_UniformLocationCache;
public:
	Shader();
	~Shader();

	void Bind() const;
	void Unbind() const;

	void SetUniform1i(const string& name, int value);
	void SetUniform4f(const string& name, float f0, float f1, float f2, float f3);
private:
	int GetUniformLocation(const string& name);
	struct ShaderProgramSource ParseShader(const string& filepath);
	unsigned int CompileShader(unsigned int type, const string& source);
	unsigned int CreateShader(const string& vertexShader, const string& fragmentShader);
};