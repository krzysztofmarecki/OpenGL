﻿#include <glad/glad.h>					// OGL stuff
#include <GLFW/glfw3.h>					// GLFW stuff

#define GLM_FORCE_DEPTH_ZERO_TO_ONE		// because we use glClipControl(..., GL_ZERO_TO_ONE)
#define GLM_FORCE_XYZW_ONLY				// easier debuging 
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "types.h"
#include "Shader.h"						// Shader
#include "Camera.h"						// Camera
#include "Model.h"						// Model

#include <array>						// std::array
#include <random>						// std::random_device, std::mt19937, std::uniform_real_distribution

// settings
const U32 g_kWScreen = 1280;
const U32 g_kHScreen = 720;
const U32 g_kNumCascades = 4;
const Bool g_kVSyncOff = false;

std::array<Mat4, g_kNumCascades>
CalculateCascadeViewProj(const std::array<F32, g_kNumCascades + 1> & limitsCascade, const Camera & g_camera, const F32 sShadowMap, const Vec3 dirLight);
std::array<F32, g_kNumCascades + 1>
CalculateVsLimitsCascade(F32 nearPlane, F32 farPlane);

void CallbackMouse(GLFWwindow* window, F64 xpos, F64 ypos);
void CallbackScroll(GLFWwindow* window, F64 xoffset, F64 yoffset);
void CallbackKeyboard(GLFWwindow* window, I32 key, I32 scancode, I32 action, I32 mods);
void CallbackMessage(GLE source, GLE type, GLU id, GLE severity, GLS length, const GLC* message, const void* userParam);
void ProcessInput(GLFWwindow* window, F32 deltaTime);
void RenderQuad();

Camera	g_camera(Vec3(0.0f, 0.0f, 3.0f));

F32		g_exposure = 1.0f;
Bool	g_normalMapping = true;
F32		g_bias = 0;
F32		g_scaleNormalOffsetBias = 0;
// debug shader globals
Bool	g_debug = false;
I32		g_debugCascadeIdx = 0;

Vec3	g_posSun(217, 265, -80);
F32		g_SizeFilter = 9;
I32		g_numDiscSamples = 9;
I32 main()
{
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	if constexpr (g_kVSyncOff)
		glfwWindowHint(GLFW_DOUBLEBUFFER, GL_FALSE);
	GLFWwindow* window = glfwCreateWindow(g_kWScreen, g_kHScreen, "LearnOpenGL", nullptr, nullptr);
	if (window == nullptr)
		assert(false && "Failed to create GLFW window");

	glfwMakeContextCurrent(window);
	glfwSetCursorPosCallback(window, CallbackMouse);
	glfwSetScrollCallback(window, CallbackScroll);
	glfwSetKeyCallback(window, CallbackKeyboard);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
		assert(false && "Failed to initialize GLAD");

	// configure global opengl state
	// -----------------------------
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(CallbackMessage, nullptr);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_GEQUAL);					// // 1 near, 0 far
	glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
	//glDepthRange(0, 1);					// it's default
	glClearDepth(0.f);						
	
	glClearColor(0.5f, 0.8f, 1.6f, 1.0f);	// color of sky, I don't draw skybox (I'm lazy)
	glEnable(GL_CULL_FACE);
	glPolygonOffset(-2, -8);				// slope scale and constant depth bias for shadow map rendering
	
	// configure floating point framebuffer
	// ------------------------------------
	GLU fboHDR;
	glCreateFramebuffers(1, &fboHDR);
	GLU bufColor;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufColor);
	glTextureStorage2D(bufColor, 1, GL_RGB16F, g_kWScreen, g_kHScreen);
	GLU bufDiffuseLight;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufDiffuseLight);
	glTextureStorage2D(bufDiffuseLight, log2f(std::max(g_kWScreen, g_kHScreen))+1, GL_R16F, g_kWScreen, g_kHScreen);
	GLU attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glNamedFramebufferDrawBuffers(fboHDR, 2, attachments);
	GLU bufDepth;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufDepth);
	glTextureStorage2D(bufDepth,1, GL_DEPTH_COMPONENT32F, g_kWScreen, g_kHScreen);
	// attach buffers
	glNamedFramebufferTexture(fboHDR, GL_COLOR_ATTACHMENT0, bufColor, 0);
	glNamedFramebufferTexture(fboHDR, GL_COLOR_ATTACHMENT1, bufDiffuseLight, 0);
	glNamedFramebufferTexture(fboHDR, GL_DEPTH_ATTACHMENT, bufDepth, 0);
	auto AssertFBOIsComplete = [](GLU fbo) {
#ifndef NDEBUG
		if (glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			assert(false && "Framebuffer not complete!\n");
#endif // !NDEBUG
	};
	AssertFBOIsComplete(fboHDR);
	
	// create framebuffer for CSM
	// --------------------------
	GLU fboShadowMap;
	glCreateFramebuffers(1, &fboShadowMap);
	
	GLU bufDepthShadow;
	GLS sShadowMap = 2048;
	glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &bufDepthShadow);
	glTextureStorage3D(bufDepthShadow, 1, GL_DEPTH_COMPONENT16, sShadowMap, sShadowMap, g_kNumCascades);
	glTextureParameteri(bufDepthShadow, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTextureParameteri(bufDepthShadow, GL_TEXTURE_COMPARE_FUNC, GL_GEQUAL);
	glNamedFramebufferTexture(fboShadowMap, GL_COLOR_ATTACHMENT0, GL_NONE, 0);
	glNamedFramebufferTextureLayer(fboShadowMap, GL_DEPTH_ATTACHMENT, bufDepthShadow, 0, 0);
	AssertFBOIsComplete(fboShadowMap);
	
	// CSM invariants
	// --------------
	const std::array<F32, g_kNumCascades + 1> aVsLimitsCascade = CalculateVsLimitsCascade(1, 1000);
	const std::array<F32, g_kNumCascades> aVsFarCascade	= [](const std::array<F32, g_kNumCascades + 1>& rLimCascades) {
		std::array<F32, g_kNumCascades> aVsFarCascade;
		for (Size i = 0; i < g_kNumCascades; i++) {
			aVsFarCascade[i] = rLimCascades[i + 1];
		}
		return aVsFarCascade;
	}(aVsLimitsCascade);
	
	// Generate random float rotation texture
	const I32 kSizeRandomAngle = 16;
	const std::vector<F32> aRadRandomAngles = [](const I32 size) {
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<F32> dis(0, 2 * 3.14159265358979323846f);

		std::vector<F32> aRadRandomAngles;
		aRadRandomAngles.reserve(size);
		for (I32 i = 0; i < size; i++)
			aRadRandomAngles.push_back(dis(gen));
		return aRadRandomAngles;
	}(kSizeRandomAngle * kSizeRandomAngle * kSizeRandomAngle);
	GLU bufRandomAngles;
	glCreateTextures(GL_TEXTURE_3D, 1, &bufRandomAngles);
	glTextureStorage3D(bufRandomAngles, 1, GL_R16F, kSizeRandomAngle, kSizeRandomAngle, kSizeRandomAngle);
	glTextureSubImage3D(bufRandomAngles, 0, 0, 0, 0, kSizeRandomAngle, kSizeRandomAngle, kSizeRandomAngle, GL_RED, GL_FLOAT, aRadRandomAngles.data());

	// Shaders
	// -------
	const Char* macroDefineTransparency = "#define TRANSPARENCY\n";
	Shader passDirectShadow("src/shaders/shadow.vert", "src/shaders/shadow.frag");
	Shader passDirectShadowTransp("src/shaders/shadow.vert", "src/shaders/shadow.frag", "", macroDefineTransparency);
	Shader passGeometry("src/shaders/geometry.vert", "src/shaders/geometry.frag");
	Shader passGeometryTransp("src/shaders/geometry.vert", "src/shaders/geometry.frag", "", macroDefineTransparency);
	Shader passExposureToneGamma("src/shaders/final.vert", "src/shaders/final.frag");

	GLU samplerAniso;
	glCreateSamplers(1, &samplerAniso);
	glSamplerParameterf(samplerAniso, GL_TEXTURE_MAX_ANISOTROPY, 16);
	glSamplerParameteri(samplerAniso, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(samplerAniso, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	const Model sceneSponza("sponza/sponza.dae");
	F64 lastFrame = 0;

	// render loop
	// -----------
	while (!glfwWindowShouldClose(window))
	{
		const F64 currentFrame = glfwGetTime();
		const F64 deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		ProcessInput(window, deltaTime);

		if constexpr (g_kVSyncOff)
			std::cout << 1. / deltaTime << "\n";
		
		// CSM logic
		// ---------
		const Vec3 wsDirLight = glm::normalize(-g_posSun); // sun looks at Vec3(0, 0, 0)
		const std::array<Mat4, g_kNumCascades> aLightProj = CalculateCascadeViewProj(aVsLimitsCascade, g_camera, sShadowMap, wsDirLight);
		// moves from <-1,1> NDC to <0,1> UV space
		// by scaling by 0.5 in x and y to <-0.5, 0.5>
		// and then translating by <0.5, 0.5> in x and y to <0, 1>
		const Mat4 texScaleBias(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.5f, 0.0f, 0.0f, // add "-" if you want have glClipControl(GL_UPPER_LEFT, (...))
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f
		);
		// equivalent to:
		//const Mat4 texScaleBias = glm::scale(glm::translate(glm::identity<Mat4>(), Vec3(0.5, 0.5, 0)), Vec3(0.5, 0.5, 1));
		
		// create reference matrix from 1st cascade
		const Mat4 referenceMatrix = texScaleBias * aLightProj[0];
		// determine scale and offset for each cascade
		std::array<Vec3, g_kNumCascades> aScaleCascade;
		std::array<Vec3, g_kNumCascades> aOffsetCascade;
		for (U32 i = 0; i < g_kNumCascades; i++) {
			const Mat4 invShadowMatrix = glm::inverse(texScaleBias * aLightProj[i]);
			Vec4 zeroCorner = referenceMatrix * invShadowMatrix * Vec4(0, 0, 0, 1);
			Vec4 oneCorner  = referenceMatrix * invShadowMatrix * Vec4(1, 1, 1, 1);
			zeroCorner /= zeroCorner.w;
			oneCorner /= oneCorner.w;

			aScaleCascade [i] = Vec3(1) / Vec3(oneCorner - zeroCorner);
			aOffsetCascade[i] = -Vec3(zeroCorner);
		}

		const Mat4 modelSponza = glm::identity<Mat4>();
		// CSM rendering
		// -------------
		{
			glEnable(GL_POLYGON_OFFSET_FILL);
			glViewport(0, 0, sShadowMap, sShadowMap);
			glBindFramebuffer(GL_FRAMEBUFFER, fboShadowMap);
			for (Size i = 0; i < aLightProj.size(); i++) {
				glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, bufDepthShadow, 0, i);
				glClear(GL_DEPTH_BUFFER_BIT);
				
				passDirectShadow.SetMat4("ModelLightProj", aLightProj[i] * modelSponza);
				passDirectShadow.Use();
				sceneSponza.DrawGeometryOnly();
				
				passDirectShadowTransp.SetMat4("ModelLightProj", aLightProj[i] * modelSponza);
				passDirectShadowTransp.Use();
				sceneSponza.DrawGeometryWithMaskOnlyTransp();
			}
			glDisable(GL_POLYGON_OFFSET_FILL);
			glViewport(0, 0, g_kWScreen, g_kHScreen);
		}
		// forward pass, lightning + generate bufDiffuseLight with mipmaps
		// -----------------------------------------------
		{
			glBindFramebuffer(GL_FRAMEBUFFER, fboHDR);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear color as well, because I don't render skybox

			const Mat4 projection = CalculateInfReversedZProj(g_camera, (F32)g_kWScreen / (F32)g_kHScreen, 0.1f);
			const Mat4 view = g_camera.GetViewMatrix();

			const U32 kNumPointLights = 4;
			const std::array<Vec3, kNumPointLights> aWsPointLightPosition = {
				Vec3(0, 10, 0),
				Vec3(-50, 10, -50),
				Vec3(50, 10, 50),
				Vec3(50, 10, -50)
			};
			const std::array<Vec3, kNumPointLights> aLightColor = {
				Vec3(500),
				Vec3(500),
				Vec3(500),
				Vec3(500)
			};
			
			glBindTextureUnit(0, bufDepthShadow);
			for (GLU i = 1; i < 5; i++) // diffuse, specular, normal, mask
				glBindSampler(i, samplerAniso);
			glBindTextureUnit(5, bufRandomAngles); // lack of sampler is intentional

			auto SetUniformsGeoPass = [&](Shader& shader) {
				shader.SetMat4("Model", modelSponza);
				shader.SetMat4("Projection", projection);
				shader.SetMat4("View", view);
				
				shader.SetVec3("ViewPos", g_camera.GetPosition());

				shader.SetMat3("NormalMatrix", glm::transpose(glm::inverse(Mat3(modelSponza))));
				shader.SetBool("NormalMapping", g_normalMapping);
				
				shader.SetVec3("WsLightDir", -wsDirLight);	// notice "-"
				shader.SetVec3("DirLightColor", Vec3(3));
				shader.SetFloatArr("AVsFarCascade", aVsFarCascade.data(), g_kNumCascades);
				shader.SetMat4("ReferenceShadowMatrix", referenceMatrix);
				shader.SetVec3Arr("AScaleCascade", aScaleCascade.data(), aScaleCascade.size());
				shader.SetVec3Arr("AOffsetCascade", aOffsetCascade.data(), aOffsetCascade.size());
				
				shader.SetFloat("Bias", g_bias);
				shader.SetFloat("ScaleNormalOffsetBias", g_scaleNormalOffsetBias);
				shader.SetFloat("SizeFilter", g_SizeFilter);
				shader.SetInt("NumDiscSamples", g_numDiscSamples);
				shader.SetVec3Arr("AWsPointLightPosition", aWsPointLightPosition.data(), aWsPointLightPosition.size());
				shader.SetVec3Arr("APointLightColor", aLightColor.data(), aLightColor.size());
			};

			SetUniformsGeoPass(passGeometry);
			passGeometry.Use();
			sceneSponza.Draw();
			
			SetUniformsGeoPass(passGeometryTransp);
			passGeometryTransp.Use();
			sceneSponza.DrawTransp();

			for (GLU i = 1; i < 5; i++) // diffuse, specular, normal, mask
				glBindSampler(i, 0);

			glGenerateTextureMipmap(bufDiffuseLight);
		}
		// final pass, apply exposure, tone mapping and gamma correction
		// --------------------------------------------------------------------------------------------------
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glClear(GL_DEPTH_BUFFER_BIT);

			// OGL complains about wrong sampler state 
			// related to depth compare if I don't bind shadow map whether g_debug is true or false
			// (in shader it's used only if true), so I just bind it.
			glBindTextureUnit(2, bufDepthShadow);

			if (g_debug) {
				passExposureToneGamma.SetUInt("IdxCascade", g_debugCascadeIdx);
			} else {
				glBindTextureUnit(0, bufColor);
				glBindTextureUnit(1, bufDiffuseLight);
				passExposureToneGamma.SetFloat("ManualExposure", g_exposure);
				passExposureToneGamma.SetFloat("LevelLastMipMap", log2f(g_kWScreen));
			}
			passExposureToneGamma.SetBool("Debug", g_debug);
			passExposureToneGamma.Use();
			RenderQuad();
		}

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		if constexpr (g_kVSyncOff)
			glFlush();
		else
			glfwSwapBuffers(window);
		
		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}

std::array<Mat4, g_kNumCascades> CalculateCascadeViewProj(const std::array<F32, g_kNumCascades + 1> & aLimitCascade, const Camera & camera, const F32 sShadowMap, const Vec3 wsDirLight)
{
	std::array<Mat4, g_kNumCascades> aViewProj;

	const Mat4 invView = glm::inverse(camera.GetViewMatrix());
	F32 aspectRatioWbyH = (F32)g_kWScreen / (F32)g_kHScreen;
	F32 tanHalfHFOV = tanf(glm::radians(camera.GetDegVertFOV() * aspectRatioWbyH / 2));
	F32 tanHalfVFOV = tanf(glm::radians(camera.GetDegVertFOV() / 2));

	for (Size i = 0; i < aViewProj.size(); i++) {
		F32 xn = aLimitCascade[i] * tanHalfHFOV;
		F32 xf = aLimitCascade[i + 1] * tanHalfHFOV;
		F32 yn = aLimitCascade[i] * tanHalfVFOV;
		F32 yf = aLimitCascade[i + 1] * tanHalfVFOV;

		// camera view space
		Vec4 aFrustrumCorner[8] = {
			// near face
			Vec4(xn, yn, aLimitCascade[i], 1.0),
			Vec4(-xn, yn, aLimitCascade[i], 1.0),
			Vec4(xn, -yn, aLimitCascade[i], 1.0),
			Vec4(-xn, -yn, aLimitCascade[i], 1.0),

			// far face
			Vec4(xf, yf, aLimitCascade[i + 1], 1.0),
			Vec4(-xf, yf, aLimitCascade[i + 1], 1.0),
			Vec4(xf, -yf, aLimitCascade[i + 1], 1.0),
			Vec4(-xf, -yf,aLimitCascade[i + 1], 1.0)
		};

		// camera view space => world space
		for (Vec4& pos : aFrustrumCorner) {
			pos = invView * pos;
		}
		// based on https://github.com/TheRealMJP/Shadows/blob/master/Shadows/MeshRenderer.cpp
		Vec3 wsFrustrumCenter(0.f);
		for (I32 j = 0; j < 8; j++)
			wsFrustrumCenter += Vec3(aFrustrumCorner[j]);
		wsFrustrumCenter /= 8.f;

		F32 radius = 0.0f;
		for (const Vec4& frustCorn : aFrustrumCorner) {
			F32 dist = glm::distance(Vec3(frustCorn), wsFrustrumCenter);
			radius = std::max(dist, radius);
		}

		// normally we would want to make some bounding box checks to determine tight near and far
		const F32 nearPlane = -500;
		const F32 farPlane = 2000;
		Mat4 proj = glm::ortho<F32>(-radius, radius, -radius, radius, farPlane, nearPlane); // near and far swapped because
																					// we use reverse z (1 near 0 far)
		const Vec3 wsPosSunCascade = wsFrustrumCenter - wsDirLight * radius;
		const Mat4 view = glm::lookAt(
			wsPosSunCascade,
			wsFrustrumCenter,
			camera.GetWorldUp()
		);

		// stabilize (ShaderX6 version)
		// explanation of alghorithm:
		// https://www.gamedev.net/forums/topic/497259-stable-cascaded-shadow-maps/ 
		// in case link goes dead:
		// Projection matrix gives us coords in <-1,1>, so by multiplying by 0.5 we go to <-0.5, 0.5>
		// which isn't <0, 1> but has the same length and that's enough.
		// Then we multiply by size of shadow map, to go to this "fake texture space"
		// (real texture space is <0, size>, not <-size/2, size/2>).
		// Then we round, take offset, go back to <-1, 1> and finally add translation in x and y
		// so we don't have subpixel movement
		const Vec2 fakeTextureSpaceShadowOrigin = (proj * view * Vec4(0, 0, 0, 1)) / 2.f * sShadowMap;
		const Vec2 fakeTextureSpaceRoundedOrigin = glm::round(fakeTextureSpaceShadowOrigin);
		const Vec2 ndcRoundedOffset = (fakeTextureSpaceRoundedOrigin - fakeTextureSpaceShadowOrigin) * 2.f / sShadowMap;
		
		proj[3][0] += ndcRoundedOffset.x;
		proj[3][1] += ndcRoundedOffset.y;
		// equivalent to:
		//Mat4 translate = glm::translate(glm::identity<Mat4>(), Vec3(ndcRoundedOffset, 0));
		//proj = translate * proj;

		aViewProj[i] = proj * view;
	} // for (Size i = 0; i < aViewProj.size(); i++)
	return aViewProj;
}

std::array<F32, g_kNumCascades + 1> CalculateVsLimitsCascade(const F32 nearPlane, const F32 farPlane)
{
	std::array<F32, g_kNumCascades + 1> limitsCascade;

	const F32 stepLog = powf(farPlane / nearPlane, 1.f / g_kNumCascades); // x^(1/n) === root_n_base(x);
	const F32 stepUni = (farPlane - nearPlane) / g_kNumCascades;
	F32 distLog = nearPlane;
	F32 distUni = nearPlane;
	limitsCascade[0] = -nearPlane;	// note "-" (because it's a view space)
	for (Size i = 1; i < limitsCascade.size(); i++) {
		distLog *= stepLog;
		distUni += stepUni;
		limitsCascade[i] = -glm::mix(distUni, distLog, 0.8f); // note "-" (because it's a view space)
	}

	return limitsCascade;
}

void CallbackMouse(GLFWwindow* window, F64 xpos, F64 ypos) {
	static F32 lastX = xpos;
	static F32 lastY = ypos;

	F32 xoffset = xpos - lastX;
	F32 yoffset = lastY - ypos; // reversed since y-coordinates range from bottom to top
	lastX = xpos;
	lastY = ypos;

	g_camera.ProcessMouseMovement(xoffset, yoffset);
}

void CallbackScroll(GLFWwindow* window, F64 xoffset, F64 yoffset) {
	g_camera.ProcessMouseScroll(yoffset);
}

void CallbackKeyboard(GLFWwindow* window, I32 key, I32 scancode, I32 action, I32 mods) {
	if (key == GLFW_KEY_G && action == GLFW_PRESS) {
		g_debug = !g_debug;
	}
	if (key == GLFW_KEY_H && action == GLFW_PRESS) {
		g_debugCascadeIdx = (g_debugCascadeIdx + 1) % g_kNumCascades;
	}
	if (key == GLFW_KEY_N && action == GLFW_PRESS) {
		g_normalMapping = !g_normalMapping;
	}

	if (key == GLFW_KEY_O && action == GLFW_PRESS) {
		g_numDiscSamples--;
	}
	if (key == GLFW_KEY_P && action == GLFW_PRESS) {
		g_numDiscSamples++;
	}
	g_numDiscSamples = glm::clamp<I32>(g_numDiscSamples, 1, 24);
}

void CallbackMessage(GLE source, GLE type, GLU id, GLE severity, GLS length,
	const GLC* message, const void* userParam)
{
	if (type == 0x8251) // silence message about static draw
		return;
	fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
		(type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
		type, severity, message);
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void ProcessInput(GLFWwindow* window, F32 deltaTime)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	using CM = Camera::Movement;
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		g_camera.ProcessKeyboard(CM::FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		g_camera.ProcessKeyboard(CM::BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		g_camera.ProcessKeyboard(CM::LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		g_camera.ProcessKeyboard(CM::RIGHT, deltaTime);

	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
		g_exposure -= 0.05f;
		g_exposure = std::max(0.f, g_exposure);
	}
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
		g_exposure += 0.05f;
	}

	if (glfwGetKey(window, GLFW_KEY_KP_4) == GLFW_PRESS)
		g_posSun.x -= 1;
	if (glfwGetKey(window, GLFW_KEY_KP_6) == GLFW_PRESS)
		g_posSun.x += 1;

	if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
		g_bias -= 0.00001f;
	if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)
		g_bias += 0.00001f;

	if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS)
		g_scaleNormalOffsetBias -= 1.f;
	if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)
		g_scaleNormalOffsetBias += 1.f;

	if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS)
		g_SizeFilter -= 0.1f;
	if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
		g_SizeFilter += 0.1f;
	g_SizeFilter = glm::clamp(g_SizeFilter, 1.f, 200.f);
}

// renders a quad over whole image
// -------------------------------
void RenderQuad()
{
	static GLU VAO = 0;
	static GLU VBO;
	if (VAO == 0)
	{
		const F32 quadVertices[] = {
			// x y z U V
			-1,  1, 0, 0, 1,
			-1, -1, 0, 0, 0,
			 1,  1, 0, 1, 1,
			 1, -1, 0, 1, 0,
		};
		
		glCreateBuffers(1, &VBO);
		glNamedBufferData(VBO, sizeof(quadVertices), &quadVertices[0], GL_STATIC_DRAW);

		glCreateVertexArrays(1, &VAO);
		glVertexArrayVertexBuffer(VAO, 0, VBO, 0, 5 * sizeof(quadVertices[0]));

		glEnableVertexArrayAttrib(VAO, 0);
		glEnableVertexArrayAttrib(VAO, 1);

		glVertexArrayAttribFormat(VAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
		glVertexArrayAttribFormat(VAO, 1, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(quadVertices[0]));

		glVertexArrayAttribBinding(VAO, 0, 0);
		glVertexArrayAttribBinding(VAO, 1, 0);
	}
	glBindVertexArray(VAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}