#include <glad/glad.h>					// OGL stuff
#include <GLFW/glfw3.h>					// GLFW stuff

#define GLM_FORCE_DEPTH_ZERO_TO_ONE		// because we use glClipControl(..., GL_ZERO_TO_ONE)
#define GLM_FORCE_XYZW_ONLY				// easier debuging 
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "types.h"
#include "Shader.h"						// Shader
#include "Camera.h"						// Camera
#include "Model.h"						// Model
#include "error.h"						// PrintErrorAndAbort

#include <array>						// std::array
#include <random>						// std::random_device, std::mt19937, std::uniform_real_distribution
#include <iostream>						// std::cout, fprintf

// settings
const U32 g_kWScreen = 1920;
const U32 g_kHScreen = 1080;
const U32 g_kNumCascades = 4;
const Bool g_kVSync = true;

std::array<Mat4, g_kNumCascades>
CalculateCascadeViewProj(const std::array<F32, g_kNumCascades + 1> & limitsCascade, const Camera & g_camera, const F32 sShadowMap, const Vec3 dirLight);
std::array<F32, g_kNumCascades + 1>
CalculateVsLimitsCascade(F32 nearPlane, F32 farPlane);
Vec4
CalculateToneMappingParamsLottes(F32 whitePoint);

void CallbackMouse(GLFWwindow* window, F64 xpos, F64 ypos);
void CallbackScroll(GLFWwindow* window, F64 xoffset, F64 yoffset);
void CallbackKeyboard(GLFWwindow* window, I32 key, I32 scancode, I32 action, I32 mods);
void CallbackMessage(GLE source, GLE type, GLU id, GLE severity, GLS length, const GLC* message, const void* userParam);
void ProcessInput(GLFWwindow* window, F32 deltaTime);
void RenderQuad();

Camera	g_camera(Vec3(0.0f, 0.0f, 3.0f));

F32		g_exposure = 0.35;
Bool	g_enableNormalMapping = true;
F32		g_bias = 0.0;
F32		g_scaleNormalOffsetBias = 0;

Bool	g_showShadowMap = false;
I32		g_cascadeIdx = 0;

Vec3	g_wsPosSun(217, 265, -80);
F32		g_sizeFilterShadow = 15;
F32		g_widthLight = 800;

F32		g_wsSizeKernelAO = 3.5;
F32		g_rateOfChangeAO = 0.11;
Bool	g_enableAO = true;
Bool	g_showAO = false;

F32		g_rateOfChangeTAA = 0.05;
Bool	g_tAA = true;

int main() {
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_DEPTH_BITS, 0);
	glfwWindowHint(GLFW_STENCIL_BITS, 0);
	if (g_kVSync)
		glfwWindowHint(GLFW_DOUBLEBUFFER, GL_TRUE);
	else
		glfwWindowHint(GLFW_DOUBLEBUFFER, GL_FALSE);
	GLFWwindow* window = glfwCreateWindow(g_kWScreen, g_kHScreen, "LearnOpenGL", nullptr, nullptr);
	if (window == nullptr)
		PrintErrorAndAbort("Failed to create GLFW window");

	glfwMakeContextCurrent(window);
	if (g_kVSync)
		glfwSwapInterval(1);
	glfwSetCursorPosCallback(window, CallbackMouse);
	glfwSetScrollCallback(window, CallbackScroll);
	glfwSetKeyCallback(window, CallbackKeyboard);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
		PrintErrorAndAbort("Failed to initialize GLAD");

	// configure global opengl state
	// -----------------------------
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(CallbackMessage, nullptr);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_GEQUAL);					// 1 near, 0 far
	glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
	//glDepthRange(0, 1);					// it's default
	glClearDepth(0.f);						
	
	glClearColor(0.5f, 0.8f, 1.6f, 1.0f);	// color of sky, I don't draw skybox (I'm lazy)
	glEnable(GL_CULL_FACE);
	glPolygonOffset(-2.5, -8);				// slope scale and constant depth bias for shadow map rendering
	
	// create textures for render targets
	GLU bufHdr;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufHdr);
	glTextureStorage2D(bufHdr, 1, GL_RGBA16F, g_kWScreen, g_kHScreen); // not using A16F
	GLU bufLdr;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufLdr);
	glTextureStorage2D(bufLdr, 1, GL_RGBA8, g_kWScreen, g_kHScreen); // not using A8
	GLU bufLdrSrgb;
	glGenTextures(1, &bufLdrSrgb);
	glTextureView(bufLdrSrgb, GL_TEXTURE_2D, bufLdr, GL_SRGB8_ALPHA8, 0, 1, 0, 1);
	GLU bufDiffuseLight;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufDiffuseLight);
	glTextureStorage2D(bufDiffuseLight, log2f(std::max(g_kWScreen, g_kHScreen))+1, GL_R16F, g_kWScreen, g_kHScreen);
	GLU bufDiffuseLightSingleValue;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufDiffuseLightSingleValue);
	glTextureStorage2D(bufDiffuseLightSingleValue, 1, GL_R16F, 1, 1);
	GLU bufDiffuseSpec;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufDiffuseSpec);
	glTextureStorage2D(bufDiffuseSpec, 1, GL_RGBA8, g_kWScreen, g_kHScreen);
	GLU bufNormal;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufNormal);
	glTextureStorage2D(bufNormal, 1, GL_RGB10_A2, g_kWScreen, g_kHScreen); // not using A2
	GLU bufDepth;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufDepth);
	glTextureStorage2D(bufDepth,1, GL_DEPTH_COMPONENT32F, g_kWScreen, g_kHScreen);
	GLU bufVelocity;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufVelocity);
	glTextureStorage2D(bufVelocity, 1, GL_RG16F, g_kWScreen, g_kHScreen);
	
	// create and configure framebuffers
	// ---------------------------------
	auto CreateConfigureFrameBuffer = [](std::vector<GLU> aCollorAtt, GLU depthAtt = 0, Bool isDepthLayerd = false) {
		std::vector<GLU> aAttachment;
		for (Size i = 0; i < aCollorAtt.size(); i++)
			aAttachment.push_back(GL_COLOR_ATTACHMENT0 + i);
		GLU fbo;
		glCreateFramebuffers(1, &fbo);
		glNamedFramebufferDrawBuffers(fbo, aAttachment.size(), aAttachment.data());
		for (Size i = 0; i < aAttachment.size(); i++)
			glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0+i, aCollorAtt[i], 0);
		if (depthAtt != 0) {
			if (isDepthLayerd)
				glNamedFramebufferTextureLayer(fbo, GL_DEPTH_ATTACHMENT, depthAtt, 0, 0);
			else
				glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, depthAtt, 0);
		}

		if (glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			PrintErrorAndAbort("Framebuffer not complete!");

		return fbo;
	};

	const GLU fboGeometry = CreateConfigureFrameBuffer({ bufDiffuseSpec, bufNormal, bufVelocity }, bufDepth);
	const GLU fboDeffered = CreateConfigureFrameBuffer({ bufHdr, bufDiffuseLight });
	const GLU fboBack = CreateConfigureFrameBuffer({ bufLdrSrgb });

	// create framebuffer for CSM
	// --------------------------
	GLU bufDepthShadow;
	GLS sShadowMap = 2048;
	glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &bufDepthShadow);
	glTextureStorage3D(bufDepthShadow, 1, GL_DEPTH_COMPONENT16, sShadowMap, sShadowMap, g_kNumCascades);
	const GLU fboShadowMap = CreateConfigureFrameBuffer({}, bufDepthShadow, true);
	
	// create samplers for shadow mapping
	GLU samplerShadowDepth;
	glCreateSamplers(1, &samplerShadowDepth);
	glSamplerParameteri(samplerShadowDepth, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glSamplerParameteri(samplerShadowDepth, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glSamplerParameteri(samplerShadowDepth, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(samplerShadowDepth, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLU samplerShadowPCF;
	glCreateSamplers(1, &samplerShadowPCF);
	glSamplerParameteri(samplerShadowPCF, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glSamplerParameteri(samplerShadowPCF, GL_TEXTURE_COMPARE_FUNC, GL_GEQUAL);
	glSamplerParameteri(samplerShadowPCF, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(samplerShadowPCF, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// CSM invariants
	// --------------
	const std::array<F32, g_kNumCascades + 1> aVsLimitsCascade = CalculateVsLimitsCascade(1, 1000);
	const std::array<F32, g_kNumCascades> aVsFarCascade	= [](const std::array<F32, g_kNumCascades + 1>& rAVsLimCascade) {
		std::array<F32, g_kNumCascades> aVsFarCascade;
		for (Size i = 0; i < g_kNumCascades; i++)
			aVsFarCascade[i] = rAVsLimCascade[i + 1];
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

	// SSAO
	GLU bufSsao;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufSsao);
	glTextureStorage2D(bufSsao, 1, GL_R8, g_kWScreen/2, g_kHScreen/2);
	const GLU fboSsao = CreateConfigureFrameBuffer({ bufSsao });

	GLU bufSsaoSpatiallyDenoised;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufSsaoSpatiallyDenoised);
	glTextureStorage2D(bufSsaoSpatiallyDenoised, 1, GL_R8, g_kWScreen / 2, g_kHScreen / 2);

	GLU bufSsaoAccCurr;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufSsaoAccCurr);
	glTextureStorage2D(bufSsaoAccCurr, 1, GL_R8, g_kWScreen / 2, g_kHScreen / 2);

	GLU bufSsaoAccPrev;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufSsaoAccPrev);
	glTextureStorage2D(bufSsaoAccPrev, 1, GL_R8, g_kWScreen / 2, g_kHScreen / 2);

	GLU bufDepthHalfResCurr;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufDepthHalfResCurr);
	glTextureStorage2D(bufDepthHalfResCurr, 1, GL_R32F, g_kWScreen / 2, g_kHScreen / 2);

	GLU bufDepthHalfResPrev;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufDepthHalfResPrev);
	glTextureStorage2D(bufDepthHalfResPrev, 1, GL_R32F, g_kWScreen / 2, g_kHScreen / 2);

	GLU bufVelocityHalfRes;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufVelocityHalfRes);
	glTextureStorage2D(bufVelocityHalfRes, 1, GL_RG16F, g_kWScreen / 2, g_kHScreen / 2);
	const GLU fboDepthDownsample = CreateConfigureFrameBuffer({ bufDepthHalfResCurr, bufVelocityHalfRes });

	// TAA
	// ----
	GLU bufLdrSrgbAccCurr;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufLdrSrgbAccCurr);
	glTextureStorage2D(bufLdrSrgbAccCurr, 1, GL_SRGB8_ALPHA8, g_kWScreen, g_kHScreen);
	
	GLU bufLdrSrgbAccPrev;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufLdrSrgbAccPrev);
	glTextureStorage2D(bufLdrSrgbAccPrev, 1, GL_SRGB8_ALPHA8, g_kWScreen, g_kHScreen);

	GLU bufDepthPrev;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufDepthPrev);
	glTextureStorage2D(bufDepthPrev, 1, GL_DEPTH_COMPONENT32F, g_kWScreen, g_kHScreen);

	const GLU fboTaa = CreateConfigureFrameBuffer({ bufLdrSrgbAccCurr });

	// Shaders
	// -------
	const std::string macroDefineAlphaMasked = "#define ALPHA_MASKED";
	const Shader passDirectShadow("shadow.vert", "shadow.frag");
	const Shader passDirectShadowAlphaMasked("shadow.vert", "shadow.frag", "", macroDefineAlphaMasked);
	const Shader passGeometry("geometry.vert", "geometry.frag");
	const Shader passGeometryAlphaMasked("geometry.vert", "geometry.frag", "", macroDefineAlphaMasked);
	const Shader passShading("uv.vert", "shading.frag");
	const Shader passExposureTone("uv.vert", "exposureToneMap.frag");
	const Shader passEyeAdaptation("eyeAdaptation.comp");
	const Shader passPassThrough("uv.vert", "passThrough.frag");

	const Shader passDepthVelocityDownsample("uv.vert", "depthVelocityDownsample.frag");
	const Shader passSsao("uv.vert", "gtao.frag");
	const Shader passSsaoSpatialDenoiser("uv.vert", "gtaoSpatialDenoiser.frag");
	const Shader passSsaoTemporalDenoiser("uv.vert", "gtaoTemporalDenoiser.frag");

	const Shader passTaa("uv.vert", "taa.frag");

	GLU samplerAnisoRepeat;
	glCreateSamplers(1, &samplerAnisoRepeat);
	glSamplerParameterf(samplerAnisoRepeat, GL_TEXTURE_MAX_ANISOTROPY, 16);
	glSamplerParameteri(samplerAnisoRepeat, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(samplerAnisoRepeat, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(samplerAnisoRepeat, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glSamplerParameteri(samplerAnisoRepeat, GL_TEXTURE_WRAP_T, GL_REPEAT);
	GLU samplerPointClamp;
	glCreateSamplers(1, &samplerPointClamp);
	glSamplerParameteri(samplerPointClamp, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glSamplerParameteri(samplerPointClamp, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glSamplerParameteri(samplerPointClamp, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(samplerPointClamp, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLU samplerTrilinearClamp;
	glCreateSamplers(1, &samplerTrilinearClamp);
	glSamplerParameteri(samplerTrilinearClamp, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(samplerTrilinearClamp, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(samplerTrilinearClamp, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(samplerTrilinearClamp, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLU samplerLinearClamp;
	glCreateSamplers(1, &samplerLinearClamp);
	glSamplerParameteri(samplerLinearClamp, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glSamplerParameteri(samplerLinearClamp, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(samplerLinearClamp, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(samplerLinearClamp, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


	const Model sceneSponza("sponza/sponza.dae");
	Mat4 modelViewProjPrevSponza = glm::identity<Mat4>();
	F64 frameTimePrev = 0;
	U64 frameCount = -1;

	// render loop
	// -----------
	while (!glfwWindowShouldClose(window)) {
		frameCount++;
		const F64 frameTimeCurr = glfwGetTime();
		const F64 deltaTime = frameTimeCurr - frameTimePrev;
		frameTimePrev = frameTimeCurr;
		ProcessInput(window, deltaTime);

		if (!g_kVSync)
			std::cout << 1. / deltaTime << "\n";
		
		// CSM logic
		// ---------
		const Vec3 wsDirLight = glm::normalize(-g_wsPosSun); // sun looks at Vec3(0, 0, 0)
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
				
				passDirectShadowAlphaMasked.SetMat4("ModelLightProj", aLightProj[i] * modelSponza);
				passDirectShadowAlphaMasked.Use();
				glBindSampler(0, samplerPointClamp); // alpha mask
				sceneSponza.DrawWithMaskOnly();
			}
			glDisable(GL_POLYGON_OFFSET_FILL);
			glViewport(0, 0, g_kWScreen, g_kHScreen);
		}
		auto GetJitter = [](const U64 frameCount) {
			auto HaltonSeq = [](I32 prime, I32 idx) {
				F32 r = 0;
				F32 f = 1;
				while (idx > 0) {
					f /= prime;
					r += f * (idx % prime);
					idx /= prime;
				}
				return r;
			};
			F32 u = HaltonSeq(2, (frameCount % 8) + 1) - 0.5f;
			F32 v = HaltonSeq(3, (frameCount % 8) + 1) - 0.5f;
			if (g_tAA)
				return Vec2(u, v) * Vec2(1./g_kWScreen, 1./g_kHScreen) * 2.f;
			else
				return Vec2(0);
		};
		auto JitterProjection = [&GetJitter](const Mat4& proj, const U64 frameCount) {
			const Vec2 jitter = GetJitter(frameCount);
			return glm::translate(glm::identity<Mat4>(), Vec3(jitter, 0)) * proj;
		};
		const float nearPlane = 0.1;
		const Mat4 projection = JitterProjection(CalculateInfReversedZProj(g_camera, (F32)g_kWScreen / (F32)g_kHScreen, nearPlane), frameCount);
		const Mat4 view = g_camera.GetViewMatrix();
		auto SetUniformsBasics = [&](const Shader& shader) {
			shader.SetMat4("ModelViewProj", projection * view * modelSponza);
			shader.SetMat4("ModelViewProjPrev", modelViewProjPrevSponza);
			shader.SetMat3("NormalMatrix", glm::transpose(glm::inverse(Mat3(modelSponza))));
			shader.SetBool("EnableNormalMapping", g_enableNormalMapping);
			shader.SetVec2("JitterCurr", GetJitter(frameCount));
			shader.SetVec2("JitterPrev", GetJitter(frameCount - 1));
		};
		// geometry pass
		// -------------
		{
			glBindFramebuffer(GL_FRAMEBUFFER, fboGeometry);
			glNamedFramebufferTexture(fboGeometry, GL_DEPTH_ATTACHMENT, bufDepth, 0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear color as well, because I don't render skybox
			glClearTexImage(bufVelocity, 0, GL_RG, GL_UNSIGNED_BYTE, nullptr);
			passGeometry.Use();
			for (GLU i = 1; i <= 3; i++) // diffuse, specular, normal
				glBindSampler(i, samplerAnisoRepeat);
			glBindSampler(4, samplerPointClamp); // mask
			SetUniformsBasics(passGeometry);
			sceneSponza.Draw();

			passGeometryAlphaMasked.Use();
			SetUniformsBasics(passGeometryAlphaMasked);
			sceneSponza.DrawWithMask();	

			modelViewProjPrevSponza = projection * view * modelSponza;
		}
		// ssao
		// ----
		{
			// downsample depth & velocity
			{
				glViewport(0, 0, g_kWScreen/2, g_kHScreen/2);
				glBindFramebuffer(GL_FRAMEBUFFER, fboDepthDownsample);
				glNamedFramebufferTexture(fboDepthDownsample, GL_COLOR_ATTACHMENT0, bufDepthHalfResCurr, 0);
				passDepthVelocityDownsample.Use();
				glBindTextureUnit(0, bufDepth);
				glBindTextureUnit(1, bufVelocity);
				glBindSampler(0, samplerPointClamp);
				glBindSampler(1, samplerPointClamp);
				RenderQuad();
			}
			// main
			{
				glBindFramebuffer(GL_FRAMEBUFFER, fboSsao);
				glNamedFramebufferTexture(fboSsao, GL_COLOR_ATTACHMENT0, bufSsao, 0);
				glBindTextureUnit(0, bufDepthHalfResCurr);
				glBindSampler(0, samplerPointClamp);

				auto GetRadRodationTemporal = [](const U64 frameCount) {
					const F32 aRotation[] = { 60, 300, 180, 240, 120, 0 };
					return aRotation[frameCount % 6] / 360 * 2 * 3.14159265358979323846f;
				};

				passSsao.Use();
				passSsao.SetMat4("InvProj", glm::inverse(projection));
				passSsao.SetFloat("WsRadius", g_wsSizeKernelAO);
				passSsao.SetFloat("RadRotationTemporal", GetRadRodationTemporal(frameCount));
				passSsao.SetVec4("Scaling", Vec4(g_kWScreen / 2, g_kHScreen / 2, 1. / (g_kWScreen / 2), 1. / (g_kHScreen / 2)));
				RenderQuad();
			}
			// spatial denoiser
			{
				glNamedFramebufferTexture(fboSsao, GL_COLOR_ATTACHMENT0, bufSsaoSpatiallyDenoised, 0);
				glBindTextureUnit(0, bufSsao);
				glBindTextureUnit(1, bufDepthHalfResCurr);
				glBindSampler(0, samplerPointClamp);
				glBindSampler(1, samplerPointClamp);
				passSsaoSpatialDenoiser.Use();
				passSsaoSpatialDenoiser.SetFloat("Near", nearPlane);
				RenderQuad();
			}
			// temporal denoiser
			{
				glNamedFramebufferTexture(fboSsao, GL_COLOR_ATTACHMENT0, bufSsaoAccCurr, 0);
				glBindTextureUnit(0, bufSsaoSpatiallyDenoised);
				glBindTextureUnit(1, bufSsaoAccPrev);
				glBindTextureUnit(2, bufVelocityHalfRes);
				glBindTextureUnit(3, bufDepthHalfResCurr);
				glBindTextureUnit(4, bufDepthHalfResPrev);
				glBindSampler(0, samplerPointClamp);
				glBindSampler(1, samplerLinearClamp);
				glBindSampler(2, samplerPointClamp);
				glBindSampler(3, samplerPointClamp);
				glBindSampler(4, samplerPointClamp);
				passSsaoTemporalDenoiser.Use();
				passSsaoTemporalDenoiser.SetFloat("RateOfChange", g_rateOfChangeAO);
				passSsaoTemporalDenoiser.SetFloat("Near", nearPlane);
				passSsaoTemporalDenoiser.SetVec2("Scaling", Vec2(g_kWScreen / 2, g_kHScreen / 2));
				RenderQuad();
			}
			glViewport(0, 0, g_kWScreen, g_kHScreen);
			std::swap(bufDepthHalfResCurr, bufDepthHalfResPrev);
			std::swap(bufSsaoAccCurr, bufSsaoAccPrev);
		}
		// deffered pass
		// -------------
		{
			glBindFramebuffer(GL_FRAMEBUFFER, fboDeffered);
			passShading.Use();
			passShading.SetVec3("WsPosCamera", g_camera.GetWsPosition());

			passShading.SetVec3("WsDirLight", -wsDirLight);	// notice "-"
			passShading.SetVec3("ColorDirLight", Vec3(3));
			passShading.SetFloatArr("AVsFarCascade", aVsFarCascade.data(), g_kNumCascades);
			passShading.SetMat4("ReferenceShadowMatrix", referenceMatrix);
			passShading.SetVec3Arr("AScaleCascade", aScaleCascade.data(), aScaleCascade.size());
			passShading.SetVec3Arr("AOffsetCascade", aOffsetCascade.data(), aOffsetCascade.size());
			passShading.SetFloat("WidthLight", g_widthLight);

			passShading.SetFloat("Bias", g_bias);
			passShading.SetFloat("ScaleNormalOffsetBias", g_scaleNormalOffsetBias);
			passShading.SetFloat("SizeFilter", g_sizeFilterShadow);
			passShading.SetMat4("InvViewProj", glm::inverse(projection* view));
			passShading.SetFloat("Near", nearPlane);
			passShading.SetBool("EnableAO", g_enableAO);
			glBindTextureUnit(0, bufDiffuseSpec);
			glBindTextureUnit(1, bufNormal);
			glBindTextureUnit(2, bufDepth);
			glBindTextureUnit(3, bufDepthShadow);
			glBindTextureUnit(4, bufDepthShadow);
			glBindTextureUnit(5, bufRandomAngles); // lack of sampler is intentional
			glBindTextureUnit(6, bufSsaoAccPrev); // swap above
			glBindSampler(0, samplerPointClamp);
			glBindSampler(1, samplerPointClamp);
			glBindSampler(2, samplerPointClamp);
			glBindSampler(3, samplerShadowPCF);
			glBindSampler(4, samplerShadowDepth);
			glBindSampler(6, samplerPointClamp);
			
			glDisable(GL_DEPTH_TEST); // also disables depth writes
			RenderQuad();
			glEnable(GL_DEPTH_TEST);
		}
		// eye adaptation
		// --------------
		{
			glGenerateTextureMipmap(bufDiffuseLight);
			passEyeAdaptation.Use();
			passEyeAdaptation.SetFloat("DeltaTime", deltaTime);
			glBindImageTexture(0, bufDiffuseLight, log2f(g_kWScreen), false, 0, GL_READ_ONLY, GL_R16F);
			glBindImageTexture(1, bufDiffuseLightSingleValue, 0, false, 0, GL_READ_WRITE, GL_R16F);
			glDispatchCompute(1, 1, 1);
		}
		// apply exposure, tone mapping and gamma correction
		// -------------------------------------------------
		{
			glBindFramebuffer(GL_FRAMEBUFFER, fboBack);

			glBindSampler(3, 0); // to avoid warning about PCF sampler binded (above) do depth texture and using non shadow sampler in shader (0x824e)
			if (g_showShadowMap) {
				glBindTextureUnit(3, bufDepthShadow);
				glBindSampler(3, samplerShadowDepth);
				passExposureTone.SetUInt("IdxCascade", g_cascadeIdx);
			} else if (g_showAO) {
				glBindTextureUnit(0, bufSsaoAccPrev); // swap above
				glBindSampler(0, samplerPointClamp);
			} else {
				glBindTextureUnit(0, bufHdr);
				glBindTextureUnit(1, bufDiffuseLight);
				glBindTextureUnit(2, bufDiffuseLightSingleValue);
				glBindSampler(0, samplerPointClamp);
				glBindSampler(1, samplerPointClamp);
				glBindSampler(2, samplerPointClamp);
			}
			passExposureTone.SetBool("ShowShadowMap", g_showShadowMap);
			passExposureTone.SetBool("ShowAO", g_showAO);
			passExposureTone.SetFloat("Exposure", g_exposure);
			const F32 kWhitePoint = 10;
			passExposureTone.SetVec4("ParamsLottes", CalculateToneMappingParamsLottes(kWhitePoint));
			passExposureTone.SetFloat("WhitePoint", kWhitePoint);
			// cross talk curve (x^2 / (x+CrossTalkCoefficient)) will reach 1 at white point
			passExposureTone.SetFloat("CrossTalkCoefficient", kWhitePoint * (kWhitePoint - 1));

			passExposureTone.Use();
			glEnable(GL_FRAMEBUFFER_SRGB);
			RenderQuad();
			glDisable(GL_FRAMEBUFFER_SRGB);
		}
		// TAA
		// ---
		if (g_tAA) {
			glBindFramebuffer(GL_FRAMEBUFFER, fboTaa);
			glNamedFramebufferTexture(fboTaa, GL_COLOR_ATTACHMENT0, bufLdrSrgbAccCurr, 0);
			passTaa.Use();
			passTaa.SetFloat("RateOfChange", g_rateOfChangeTAA);
			passTaa.SetVec4("Scaling", Vec4(g_kWScreen, g_kHScreen, 1. / g_kWScreen, 1. / g_kHScreen));
			passTaa.SetFloat("Near", nearPlane);
			glBindTextureUnit(0, bufLdrSrgb);
			glBindTextureUnit(1, bufLdrSrgbAccPrev);
			glBindTextureUnit(2, bufVelocity);
			glBindTextureUnit(3, bufDepth);
			glBindTextureUnit(4, bufDepthPrev);
			glBindSampler(0, samplerPointClamp);
			glBindSampler(1, samplerLinearClamp);
			glBindSampler(2, samplerPointClamp);
			glBindSampler(3, samplerPointClamp);
			glBindSampler(4, samplerPointClamp);
			glEnable(GL_FRAMEBUFFER_SRGB);
			RenderQuad();
			glDisable(GL_FRAMEBUFFER_SRGB);

			std::swap(bufLdrSrgbAccCurr, bufLdrSrgbAccPrev);
			std::swap(bufDepth, bufDepthPrev);
		}
		// pass through to back buffer
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			if (g_tAA)
				glBindTextureUnit(0, bufLdrSrgbAccPrev); // swap above
			else
				glBindTextureUnit(0, bufLdrSrgb);
			glBindSampler(0, samplerPointClamp);
			passPassThrough.Use();
			glEnable(GL_FRAMEBUFFER_SRGB);
			RenderQuad();
			glDisable(GL_FRAMEBUFFER_SRGB);
		}

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		if (g_kVSync)
			glfwSwapBuffers(window);
		else
			glFlush();
		
		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}

std::array<Mat4, g_kNumCascades> CalculateCascadeViewProj(const std::array<F32, g_kNumCascades + 1> & aLimitCascade, const Camera & camera, const F32 sShadowMap, const Vec3 wsDirLight) {
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
		for (Vec4& pos : aFrustrumCorner)
			pos = invView * pos;

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
			camera.GetWsWorldUp()
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

Vec4 CalculateToneMappingParamsLottes(F32 whitePoint)
{
	// from https://github.com/Opioid/tonemapper/blob/master/tonemapper.py
	const F32 a = 1.1; // contrast
	const F32 d = 0.97; // shoulder
	const F32 ad = a * d;
	const F32 mid_in = 0.3;
	const F32 mid_out = 0.18;

	const F32 midi_pow_a = powf(mid_in, a);
	const F32 midi_pow_ad = powf(mid_in, ad);
	const F32 hdrm_pow_a = powf(whitePoint, a);
	const F32 hdrm_pow_ad = powf(whitePoint, ad);
	const F32 u = hdrm_pow_ad * mid_out - midi_pow_ad * mid_out;
	const F32 v = midi_pow_ad * mid_out;

	const F32 b = -((-midi_pow_a + (mid_out * (hdrm_pow_ad * midi_pow_a - hdrm_pow_a * v)) / u) / v);
	const F32 c = (hdrm_pow_ad * midi_pow_a - hdrm_pow_a * v) / u;

	return Vec4(a, b, c, d);
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
	if (action != GLFW_PRESS)
		return;

	if (key == GLFW_KEY_G)
		g_showShadowMap = !g_showShadowMap;

	if (key == GLFW_KEY_H)
		g_cascadeIdx = (g_cascadeIdx + 1) % g_kNumCascades;

	if (key == GLFW_KEY_N)
		g_enableNormalMapping = !g_enableNormalMapping;
	if (key == GLFW_KEY_Z)
		g_tAA = !g_tAA;
	if (key == GLFW_KEY_F1)
		g_enableAO = !g_enableAO;
	if (key == GLFW_KEY_F2)
		g_showAO = !g_showAO;
}

void CallbackMessage(GLE source, GLE type, GLU id, GLE severity, GLS length,
	const GLC* message, const void* userParam)
{
	if (type == 0x8251) // message about static draw
		return;
	if (type == 0x824c) // "(program) object is not successfully linked, or it not a program object"
		return;			// I print shader errors myself
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
		g_wsPosSun.x -= 1;
	if (glfwGetKey(window, GLFW_KEY_KP_6) == GLFW_PRESS)
		g_wsPosSun.x += 1;

	if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
		g_bias -= 0.00001f;
	if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)
		g_bias += 0.00001f;

	if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS)
		g_scaleNormalOffsetBias -= 1.f;
	if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)
		g_scaleNormalOffsetBias += 1.f;

	if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS)
		g_sizeFilterShadow -= 0.1f;
	if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
		g_sizeFilterShadow += 0.1f;
	g_sizeFilterShadow = glm::clamp(g_sizeFilterShadow, 1.f, 200.f);

	if (glfwGetKey(window, GLFW_KEY_COMMA) == GLFW_PRESS)
		g_widthLight -= 20;
	if (glfwGetKey(window, GLFW_KEY_PERIOD) == GLFW_PRESS)
		g_widthLight += 20;
	g_widthLight = glm::clamp(g_widthLight, 0.1f, 200000.f);

	if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS)
		g_wsSizeKernelAO -= 0.1;
	if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS)
		g_wsSizeKernelAO += 0.1;
	g_wsSizeKernelAO = glm::clamp<F32>(g_wsSizeKernelAO, 0, 50);
	
	if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
		g_rateOfChangeAO -= 0.01;
	if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
		g_rateOfChangeAO += 0.01;
	g_rateOfChangeAO = glm::clamp<F32>(g_rateOfChangeAO, 0.01, 1);
}

// renders a quad over whole image
// -------------------------------
void RenderQuad()
{
	static GLU VAO = 0;
	static GLU VBO;
	if (VAO == 0) {
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