#include "Shader.h"

#include <iostream> // std::cout
#include <fstream>	// std::ifstream
#include <sstream>  // std::stringstream
#include <assert.h> // assert

void CheckCompileErrors(const GLU shaderName, const std::string shaderTypeName) {
	GLI success;
	GLC infoLog[1024];
	if (shaderTypeName == "PROGRAM") {
		glGetProgramiv(shaderName, GL_LINK_STATUS, &success);
		if (!success) {
			glGetProgramInfoLog(shaderName, 1024, nullptr, infoLog);
			std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << shaderTypeName << "\n" << infoLog
				<< "\n -- --------------------------------------------------- -- " << std::endl;
		}
	}
	else {
		glGetShaderiv(shaderName, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(shaderName, 1024, nullptr, infoLog);
			std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << shaderTypeName << "\n" << infoLog
				<< "\n -- --------------------------------------------------- -- " << std::endl;
		}
	}
	
}

GLint CreateCompileShader(const std::string& rPathShader, const GLE shaderType, const char* defines = nullptr) {
	const std::ifstream shaderFile(rPathShader, std::ios::in);
	assert(shaderFile.is_open());

	// read whole file
	std::stringstream stream;
	stream << shaderFile.rdbuf();
	std::string shaderCode = stream.str();

	// insert defines between 1st (#version) and 2nd (rest) line of shader
	if (defines != nullptr) {
		// Find "#version", which must be 1st statement of shader due to GLSL standard.
		// Empty lines can be above, so we search for 1st non empty line.
		const Size posVersion = shaderCode.find("#version");
		const Size posFirstNewLineAfterVersion = shaderCode.find("\n", posVersion);
		shaderCode.insert(posFirstNewLineAfterVersion + 1, defines);
	}

	//compile shader
	const GLI shaderName = glCreateShader(shaderType);
	const char* shaderCodeCstr = shaderCode.c_str();
	glShaderSource(shaderName, 1, &shaderCodeCstr, nullptr);
	glCompileShader(shaderName);
	if (shaderType == GL_VERTEX_SHADER)
		CheckCompileErrors(shaderName, "VERTEX");
	else if (shaderType == GL_FRAGMENT_SHADER)
		CheckCompileErrors(shaderName, "FRAGMENT");
	else if (shaderType == GL_GEOMETRY_SHADER)
		CheckCompileErrors(shaderName, "GEOMETRY");
	else if (shaderType == GL_COMPUTE_SHADER)
		CheckCompileErrors(shaderName, "COMPUTE");
	else
		assert(false && "WRONG SHADER TYPE!");
	return shaderName;
}

Shader::Shader(const std::string& rPathVs, const std::string& rPathFs, const std::string& rPathGs, const char* defines) {
	const bool hasGS = (rPathGs.empty()) ? false : true;

	const GLI  shaderNameVertex	  = CreateCompileShader(rPathVs, GL_VERTEX_SHADER, defines);
	const GLI  shaderNameFragment = CreateCompileShader(rPathFs, GL_FRAGMENT_SHADER, defines);
	GLI shaderNameGeometry;
	if (hasGS) shaderNameGeometry = CreateCompileShader(rPathGs, GL_GEOMETRY_SHADER, defines);
	
	m_id = glCreateProgram();
	glAttachShader(m_id, shaderNameVertex);
	glAttachShader(m_id, shaderNameFragment);
	if (hasGS)
		glAttachShader(m_id, shaderNameGeometry);
	glLinkProgram(m_id);
	CheckCompileErrors(m_id, "PROGRAM");

	glDeleteShader(shaderNameVertex);
	glDeleteShader(shaderNameFragment);
	if (hasGS)
		glDeleteShader(shaderNameGeometry);
}

Shader::Shader(const std::string& rPathCs) {
	const GLI shaderNameCompute = CreateCompileShader(rPathCs, GL_COMPUTE_SHADER);

	m_id = glCreateProgram();
	glAttachShader(m_id, shaderNameCompute);
	glLinkProgram(m_id);
	CheckCompileErrors(m_id, "PROGRAM");
	glDeleteShader(shaderNameCompute);
}

void Shader::IsUniformDefined(const std::string& name) const {
	if (glGetUniformLocation(m_id, name.c_str()) == -1)
		std::cout << "WARNING! uniform \t'" << name << " not found\n";
}
