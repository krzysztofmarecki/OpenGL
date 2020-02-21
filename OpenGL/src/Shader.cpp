#include "Shader.h"

#include <iostream>		// std::cout
#include <fstream>		// std::ifstream
#include <sstream>		// std::stringstream
#include <assert.h>		// assert
#include <filesystem>	// std::path

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

std::string ReadFile(const std::string& rPathFile) {
	const std::ifstream file(rPathFile, std::ios::in);
	assert(file.is_open() && "CANNOT OPEN THE FILE!\n");

	// read whole file
	std::stringstream stream;
	stream << file.rdbuf();
	return stream.str();
}
std::string ReadFile(const std::filesystem::path& rPathFile) {
	return ReadFile(rPathFile.string());
}

GLI CreateCompileShader(const std::string& rPathShader, const GLE shaderType, const char* defines = nullptr) {
	std::string shaderCode = ReadFile(rPathShader);

	// handle "#define"
	if (defines != nullptr) {
		// Insert defines between 1st (#version) and 2nd (rest) line of shader.
		// Find "#version", which must be 1st statement of shader due to GLSL standard.
		// Empty lines can be above, so we search for 1st non empty line.
		const Size posVersion = shaderCode.find("#version");
		const Size posFirstNewLineAfterVersion = shaderCode.find("\n", posVersion);
		shaderCode.insert(posFirstNewLineAfterVersion + 1, defines);
	}

	// handle "#include"
	const std::filesystem::path parentDirectiory = std::filesystem::path(rPathShader).parent_path();
	Size posInclude = -1;
	while ((posInclude = shaderCode.find("#include", posInclude + 1)) != shaderCode.npos) {
		// get name of included file
		Size posFirstQuote = shaderCode.find("\"", posInclude);
		Size posSecondQuote = shaderCode.find("\"", posFirstQuote + 1);
		std::string includedPathFile = shaderCode.substr(posFirstQuote + 1, posSecondQuote - (posFirstQuote + 1));

		std::string includedShaderCode = ReadFile(parentDirectiory / includedPathFile);

		// if any, remove "#version xyz" from included file
		const Size includedPosVersion = includedShaderCode.find("#version");
		const Size includedPosFirstNewLineAfterVersion = includedShaderCode.find("\n", includedPosVersion);
		if (includedPosFirstNewLineAfterVersion != includedShaderCode.npos)
			includedShaderCode.erase(includedPosVersion, includedPosFirstNewLineAfterVersion - includedPosVersion); // without +1, because we want to leave \n in case of "//?" at the begining of line, which can be useful with VS addon "GLSL language integration"  https://github.com/danielscherzer/GLSL 

		// Remove "#include" from main file, so compiler don't complain.
		// In that place paste included file.
		shaderCode.erase(posInclude, posSecondQuote - posInclude + 1);
		shaderCode.insert(posInclude, includedShaderCode);
	}

	// compile shader
	const GLI shaderName = glCreateShader(shaderType);
	const Char* shaderCodeCstr = shaderCode.c_str();
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
