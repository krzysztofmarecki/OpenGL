#pragma once
#include "types.h"

#include <string>	// std::string
#include <iostream> // std::cout

class Shader
{
public:
	// graphic
	Shader(const std::string& rPathVs, const std::string& rPathFs, const std::string& rPathGs = "", const char* defines = nullptr);

	// compute
	Shader(const std::string& rPathCs);
	
	void Use() {
		glUseProgram(m_id);
	}

	// utility uniform functions
	// ------------------------------------------------------------------------
	void SetBool(const std::string &name, Bool value) const {
		glProgramUniform1i(m_id, getUniformLocation(name), (GLI)value);
		isUniformDefined(name);
	}

	void SetInt(const std::string &name, GLI value) const {
		glProgramUniform1i(m_id, getUniformLocation(name), value);
		isUniformDefined(name);
	}
	void SetUInt(const std::string& name, GLU value) const {
		glProgramUniform1ui(m_id, getUniformLocation(name), value);
		isUniformDefined(name);
	}
	void SetFloat(const std::string &name, GLF value) const {
		glProgramUniform1f(m_id, getUniformLocation(name), value);
		isUniformDefined(name);
	}
	void SetFloatArr(const std::string& name, const GLF* vals, const GLS count) const {
		glProgramUniform1fv(m_id, getUniformLocation(name), count, vals);
		isUniformDefined(name);
	}

	void SetVec2(const std::string &name, const Vec2 &value) const {
		glProgramUniform2fv(m_id, getUniformLocation(name), 1, &value[0]);
		isUniformDefined(name);
	}

	void SetVec3(const std::string &name, const Vec3 &value) const {
		glProgramUniform3fv(m_id, getUniformLocation(name), 1, &value[0]);
		isUniformDefined(name);
	}

	void SetVec4(const std::string &name, const Vec4 &value) const {
		glProgramUniform4fv(m_id, getUniformLocation(name), 1, &value[0]);
		isUniformDefined(name);
	}

	void SetMat2(const std::string &name, const Mat2 &mat) const {
		glProgramUniformMatrix2fv(m_id, getUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
		isUniformDefined(name);
	}

	void SetMat3(const std::string &name, const Mat3 &mat) const {
		glProgramUniformMatrix3fv(m_id, getUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
		isUniformDefined(name);
	}

	void SetMat4(const std::string &name, const Mat4 &mat) const {
		glProgramUniformMatrix4fv(m_id, getUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
		isUniformDefined(name);
	}

	void SetVec3Arr(const std::string& name, const Vec3* vals, const GLS count) const {
		glProgramUniform3fv(m_id, getUniformLocation(name), count, &((*vals)[0]));
		isUniformDefined(name);
	}

	void SetVec4Arr(const std::string& name, const Vec4* vals, const GLS count) const {
		glProgramUniform4fv(m_id, getUniformLocation(name), count, &((*vals)[0]));
		isUniformDefined(name);
	}

	void setMat4Arr(const std::string& name, const Mat4* mats, GLS count) const {
		glProgramUniformMatrix4fv(m_id, getUniformLocation(name), count, GL_FALSE, &(*mats)[0][0]);
		isUniformDefined(name);
	}

private:
	GLI getUniformLocation(const std::string& name) const {
		return glGetUniformLocation(m_id, name.c_str());
	}

	void isUniformDefined(const std::string& name) const
	{
		if (glGetUniformLocation(m_id, name.c_str()) == -1)
			std::cout << "WARNING! uniform \t'" << name << " not found\n";
	}

	GLU m_id;
};