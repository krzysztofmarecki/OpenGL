#pragma once
#include <cstdint>		  // size specified std types
#include <glad/glad.h>	  // OGL types

#define GLM_FORCE_XYZW_ONLY				// easier debuging 
#include "glm/vec2.hpp"	  // glm::vec2
#include "glm/vec3.hpp"   // glm::vec3
#include "glm/vec4.hpp"   // glm::vec4
#include "glm/mat2x2.hpp" // glm::mat2
#include "glm/mat3x3.hpp" // glm::mat3
#include "glm/mat4x4.hpp" // glm::mat4

// general purpose
using U8  = std::uint8_t;
using U16 = std::uint16_t;
using U32 = std::uint32_t;
using U64 = std::uint64_t;

using I8  = std::int8_t;
using I16 = std::int16_t;
using I32 = std::int32_t;
using I64 = std::uint64_t;

using Size = std::size_t;
using Bool = bool;
using Char = char;
using Void = void;
using F32 = float;
using F64 = double;

// OGL related
using GLI = GLint;
using GLU = GLuint;
using GLS = GLsizei;
using GLF = GLfloat;
using GLB = GLboolean;
using GLC = GLchar;
using GLE = GLenum;

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;

using Mat2 = glm::mat2;
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;