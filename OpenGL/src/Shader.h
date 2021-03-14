#pragma once
#include "types.h"

#include <string>	// std::string
#include <unordered_set>

class Shader
{
public:
	// graphic
	Shader(std::string fileNameVs, std::string fileNameFs, std::string fileNameGs = "", const std::string& rDefines = "");
	// compute
	Shader(std::string fileNameCs);
	
	void Use() const {
		glUseProgram(m_id);
	}

	// utility uniform functions
	// ------------------------------------------------------------------------
	void SetBool(const std::string &name, Bool value) const {
		glProgramUniform1i(m_id, GetUniformLocation(name), (GLI)value);
		IsUniformDefined(name);
	}

	void SetInt(const std::string &name, GLI value) const {
		glProgramUniform1i(m_id, GetUniformLocation(name), value);
		IsUniformDefined(name);
	}
	void SetUInt(const std::string& name, GLU value) const {
		glProgramUniform1ui(m_id, GetUniformLocation(name), value);
		IsUniformDefined(name);
	}
	void SetFloat(const std::string &name, GLF value) const {
		glProgramUniform1f(m_id, GetUniformLocation(name), value);
		IsUniformDefined(name);
	}
	void SetFloatArr(const std::string& name, const GLF* vals, const GLS count) const {
		glProgramUniform1fv(m_id, GetUniformLocation(name), count, vals);
		IsUniformDefined(name);
	}

	void SetVec2(const std::string &name, const Vec2 &value) const {
		glProgramUniform2fv(m_id, GetUniformLocation(name), 1, &value[0]);
		IsUniformDefined(name);
	}

	void SetVec3(const std::string &name, const Vec3 &value) const {
		glProgramUniform3fv(m_id, GetUniformLocation(name), 1, &value[0]);
		IsUniformDefined(name);
	}

	void SetVec4(const std::string &name, const Vec4 &value) const {
		glProgramUniform4fv(m_id, GetUniformLocation(name), 1, &value[0]);
		IsUniformDefined(name);
	}

	void SetMat2(const std::string &name, const Mat2 &mat) const {
		glProgramUniformMatrix2fv(m_id, GetUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
		IsUniformDefined(name);
	}

	void SetMat3(const std::string &name, const Mat3 &mat) const {
		glProgramUniformMatrix3fv(m_id, GetUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
		IsUniformDefined(name);
	}

	void SetMat4(const std::string &name, const Mat4 &mat) const {
		glProgramUniformMatrix4fv(m_id, GetUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
		IsUniformDefined(name);
	}

	void SetVec3Arr(const std::string& name, const Vec3* vals, const GLS count) const {
		glProgramUniform3fv(m_id, GetUniformLocation(name), count, &((*vals)[0]));
		IsUniformDefined(name);
	}

	void SetVec4Arr(const std::string& name, const Vec4* vals, const GLS count) const {
		glProgramUniform4fv(m_id, GetUniformLocation(name), count, &((*vals)[0]));
		IsUniformDefined(name);
	}

	void SetMat4Arr(const std::string& name, const Mat4* mats, GLS count) const {
		glProgramUniformMatrix4fv(m_id, GetUniformLocation(name), count, GL_FALSE, &(*mats)[0][0]);
		IsUniformDefined(name);
	}

private:
	GLI GetUniformLocation(const std::string& name) const {
		return glGetUniformLocation(m_id, name.c_str());
	}

	void IsUniformDefined(const std::string& name) const;

	GLU m_id;
	std::string m_prettyName;
	mutable std::unordered_set<std::string> m_mapUniformNotFound;
};