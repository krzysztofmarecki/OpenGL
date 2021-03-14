#include "Shader.h"
#include "error.h"		// PrintErrorAndAbort

#include <iostream>		// std::cout
#include <fstream>		// std::ifstream
#include <sstream>		// std::stringstream
#include <assert.h>		// assert
#include <filesystem>	// std::path

void CheckCompileErrors(const GLU idShader, const std::string& rNameStageShader, const std::string& rPrettyNameShader) {
	GLI success;
	Bool program = rNameStageShader == "PROGRAM";
	if (program)
		glGetProgramiv(idShader, GL_LINK_STATUS, &success);
	else
		glGetShaderiv(idShader, GL_COMPILE_STATUS, &success);

	if (!success) {
		GLC infoLog[1024];
		if (program)
			glGetProgramInfoLog(idShader, 1024, nullptr, infoLog);
		else
			glGetShaderInfoLog(idShader, 1024, nullptr, infoLog);
		std::cout << (program ? "LINK" : "COMPILE") << " ERROR for shader "
			<< rPrettyNameShader << " for stage: " << rNameStageShader << "\n" << infoLog << "\n";
	}
}

std::string ReadFile(const std::string& rPathFile) {
	const std::ifstream file(rPathFile, std::ios::in);
	if (!file.is_open())
		PrintErrorAndAbort("CANNOT OPEN: " + rPathFile);

	// read whole file
	std::stringstream stream;
	stream << file.rdbuf();
	return stream.str();
}
std::string ReadFile(const std::filesystem::path& rPathFile) {
	return ReadFile(rPathFile.string());
}

std::string RemoveComments(std::string str) {
	for (Size commentBegin = str.find("//"); commentBegin != str.npos; commentBegin = str.find("//", commentBegin)) {
		const Size commentEnd = str.find("\n", commentBegin);
		str.erase(commentBegin, commentEnd - commentBegin); // don't remove "\n"
	}
	for (Size commentBegin = str.find("/*"); commentBegin != str.npos; commentBegin = str.find("/*", commentBegin)) {
		const Size commentEnd = str.find("*/", commentBegin);
		str.erase(commentBegin, commentEnd - commentBegin + 2);
	}
	return str;
}

GLI CreateCompileShader(const std::string& rFileName, const GLE shaderType, std::string defines, std::string shaderPrettyName) {
	const std::string shaderPath = "src/shaders/" + rFileName;
	std::string shaderCode = RemoveComments(ReadFile(shaderPath));

	// handle "#define"
	if (!defines.empty()) {
		// Insert defines between 1st (#version) and 2nd (rest) line of shader.
		// Find "#version", which must be 1st statement of shader due to GLSL standard.
		// Empty lines can be above, so we search for 1st non empty line.
		const Size posVersion = shaderCode.find("#version");
		const Size posFirstNewLineAfterVersion = shaderCode.find("\n", posVersion);
		if (defines.back() != '\n')
			defines.push_back('\n');
		shaderCode.insert(posFirstNewLineAfterVersion + 1, defines);
	}

	// handle "#include"
	const std::filesystem::path parentDirectiory = std::filesystem::path(shaderPath).parent_path();
	Size posInclude = 0;
	while ((posInclude = shaderCode.find("#include", posInclude)) != shaderCode.npos) {
		// get name of included file
		const Size posFirstQuote = shaderCode.find("\"", posInclude);
		const Size posSecondQuote = shaderCode.find("\"", posFirstQuote + 1);
		const std::string includedPathFile = shaderCode.substr(posFirstQuote + 1, posSecondQuote - (posFirstQuote + 1));

		std::string includedShaderCode = RemoveComments(ReadFile(parentDirectiory / includedPathFile));

		// if any, remove "#version xyz" from included file
		const Size includedPosVersion = includedShaderCode.find("#version");
		const Size includedPosFirstNewLineAfterVersion = includedShaderCode.find("\n", includedPosVersion);
		if (includedPosFirstNewLineAfterVersion != includedShaderCode.npos)
			includedShaderCode.erase(includedPosVersion, includedPosFirstNewLineAfterVersion - includedPosVersion + 1);

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
		CheckCompileErrors(shaderName, "VERTEX", shaderPrettyName);
	else if (shaderType == GL_FRAGMENT_SHADER)
		CheckCompileErrors(shaderName, "FRAGMENT", shaderPrettyName);
	else if (shaderType == GL_GEOMETRY_SHADER)
		CheckCompileErrors(shaderName, "GEOMETRY", shaderPrettyName);
	else if (shaderType == GL_COMPUTE_SHADER)
		CheckCompileErrors(shaderName, "COMPUTE", shaderPrettyName);
	else
		std::cout << "UNHANDLED SHADER TYPE!";
	return shaderName;
}

Shader::Shader(std::string fileNameVs, std::string fileNameFs, std::string fileNameGs, const std::string& rDefines) {
	const bool hasGS = (fileNameGs.empty()) ? false : true;
	
	m_prettyName = "(" + fileNameVs + ", " + fileNameFs;
	if (hasGS)
		m_prettyName += ", " + fileNameGs;
	m_prettyName += ", defines: " + rDefines + ")";

	const GLI  shaderNameVertex	  = CreateCompileShader(fileNameVs, GL_VERTEX_SHADER, rDefines, m_prettyName);
	const GLI  shaderNameFragment = CreateCompileShader(fileNameFs, GL_FRAGMENT_SHADER, rDefines, m_prettyName);
	GLI shaderNameGeometry;
	if (hasGS) shaderNameGeometry = CreateCompileShader(fileNameGs, GL_GEOMETRY_SHADER, rDefines, m_prettyName);
	
	m_id = glCreateProgram();
	glAttachShader(m_id, shaderNameVertex);
	glAttachShader(m_id, shaderNameFragment);
	if (hasGS)
		glAttachShader(m_id, shaderNameGeometry);
	glLinkProgram(m_id);
	CheckCompileErrors(m_id, "PROGRAM", m_prettyName);

	glDeleteShader(shaderNameVertex);
	glDeleteShader(shaderNameFragment);
	if (hasGS)
		glDeleteShader(shaderNameGeometry);
}

Shader::Shader(std::string fileNameCs) {
	m_prettyName = "(" + fileNameCs + ")";
	const GLI shaderNameCompute = CreateCompileShader(fileNameCs, GL_COMPUTE_SHADER, "", m_prettyName);

	m_id = glCreateProgram();
	glAttachShader(m_id, shaderNameCompute);
	glLinkProgram(m_id);
	CheckCompileErrors(m_id, "PROGRAM", m_prettyName);
	glDeleteShader(shaderNameCompute);
}

void Shader::IsUniformDefined(const std::string& name) const {
	if (glGetUniformLocation(m_id, name.c_str()) == -1) {
		auto it = m_mapUniformNotFound.find(name);
		// print only once
		if (it == m_mapUniformNotFound.end()) {
			std::cout << "WARNING! uniform \t'" << name << " not found\n";
			m_mapUniformNotFound.insert(name);
		}
	}
		
}
