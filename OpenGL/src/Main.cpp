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

#include <array>						// std::array
#include <random>						// std::random_device, std::mt19937, std::uniform_real_distribution
#include <iostream>						// std::cout, fprintf

#include "../models/AreaTex.h"			// SMAA
#include "../models/SearchTex.h"		// SMAA

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
// debug shader globals
Bool	g_showShadowMap = false;
I32		g_cascadeIdx = 0;

Vec3	g_wsPosSun(217, 265, -80);
F32		g_sizeFilterShadow = 15;
F32		g_widthLight = 800;
enum class Smaa { None = 0, x1 = 1, T2x = 2 };
Smaa	g_smaa = Smaa::T2x;
Bool	g_enableSmaaSubsambleIndicies = false;

F32		g_wsSizeKernelAo = 3.5;
F32		g_rateOfChangeAo = 0.11;
Bool	g_enableAO = true;
Bool	g_showAO = false;

I32 main() {
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_DEPTH_BITS, 0);
	glfwWindowHint(GLFW_STENCIL_BITS, 0);
	if constexpr (!g_kVSync)
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
	auto CreateConfigureFrameBuffer = [](std::vector<GLU> aCollorAtt, GLU depthAtt = 0) {
		std::vector<GLU> aAttachment;
		for (Size i = 0; i < aCollorAtt.size(); i++)
			aAttachment.push_back(GL_COLOR_ATTACHMENT0 + i);
		GLU fbo;
		glCreateFramebuffers(1, &fbo);
		glNamedFramebufferDrawBuffers(fbo, aAttachment.size(), aAttachment.data());
		for (Size i = 0; i < aAttachment.size(); i++)
			glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0+i, aCollorAtt[i], 0);
		if (depthAtt != 0)
			glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, depthAtt, 0);
		return fbo;
	};

	const GLU fboGeometry = CreateConfigureFrameBuffer({ bufDiffuseSpec, bufNormal, bufVelocity }, bufDepth);
	const GLU fboDeffered = CreateConfigureFrameBuffer({ bufHdr, bufDiffuseLight });
	GLU fboBack;
	glCreateFramebuffers(1, &fboBack);
	glNamedFramebufferDrawBuffer(fboBack, GL_COLOR_ATTACHMENT0);
	glNamedFramebufferTexture(fboBack, GL_COLOR_ATTACHMENT0, bufLdrSrgb, 0);

	auto AssertFBOIsComplete = [](GLU fbo) {
		if (glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			assert(false && "Framebuffer not complete!\n");
	};

	AssertFBOIsComplete(fboGeometry);
	AssertFBOIsComplete(fboDeffered);
	AssertFBOIsComplete(fboBack);

	// create framebuffer for CSM
	// --------------------------
	GLU bufDepthShadow;
	GLS sShadowMap = 2048;
	glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &bufDepthShadow);
	glTextureStorage3D(bufDepthShadow, 1, GL_DEPTH_COMPONENT16, sShadowMap, sShadowMap, g_kNumCascades);
	GLU fboShadowMap;
	glCreateFramebuffers(1, &fboShadowMap);
	glNamedFramebufferTexture(fboShadowMap, GL_COLOR_ATTACHMENT0, GL_NONE, 0);
	glNamedFramebufferTextureLayer(fboShadowMap, GL_DEPTH_ATTACHMENT, bufDepthShadow, 0, 0);
	AssertFBOIsComplete(fboShadowMap);
	
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
	AssertFBOIsComplete(fboSsao);

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
	AssertFBOIsComplete(fboDepthDownsample);

	// SMAA
	// ----
	GLU bufSmaaEdge;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufSmaaEdge);
	glTextureStorage2D(bufSmaaEdge, 1, GL_RG8, g_kWScreen, g_kHScreen);
	GLU bufSmaaBlend;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufSmaaBlend);
	glTextureStorage2D(bufSmaaBlend, 1, GL_RGBA8, g_kWScreen, g_kHScreen);
	GLU bufStencil;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufStencil);
	glTextureStorage2D(bufStencil, 1, GL_STENCIL_INDEX8, g_kWScreen, g_kHScreen);
	GLU bufSmaaCurrentSrgb;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufSmaaCurrentSrgb);
	glTextureStorage2D(bufSmaaCurrentSrgb, 1, GL_SRGB8_ALPHA8, g_kWScreen, g_kHScreen);
	GLU bufSmaaPreviousSrgb;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufSmaaPreviousSrgb);
	glTextureStorage2D(bufSmaaPreviousSrgb, 1, GL_SRGB8_ALPHA8, g_kWScreen, g_kHScreen);

	GLU fboSmaa;
	glCreateFramebuffers(1, &fboSmaa);
	glNamedFramebufferDrawBuffer(fboSmaa, GL_COLOR_ATTACHMENT0);
	glNamedFramebufferTexture(fboSmaa, GL_COLOR_ATTACHMENT0, bufSmaaEdge, 0);
	glNamedFramebufferTexture(fboSmaa, GL_STENCIL_ATTACHMENT, bufStencil, 0);
	AssertFBOIsComplete(fboSmaa);

	GLU bufSmaaArea;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufSmaaArea);
	glTextureStorage2D(bufSmaaArea, 1, GL_RG8, 160, 560);
	glTextureSubImage2D(bufSmaaArea, 0, 0, 0, 160, 560, GL_RG, GL_UNSIGNED_BYTE, areaTexBytes);
	GLU bufSmaaSearch;
	glCreateTextures(GL_TEXTURE_2D, 1, &bufSmaaSearch);
	glTextureStorage2D(bufSmaaSearch, 1, GL_R8, 64, 16);
	glTextureSubImage2D(bufSmaaSearch, 0, 0, 0, 64, 16, GL_RED, GL_UNSIGNED_BYTE, searchTexBytes);

	GLU samplerSMAA;
	glCreateSamplers(1, &samplerSMAA);
	glSamplerParameteri(samplerSMAA, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glSamplerParameteri(samplerSMAA, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(samplerSMAA, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(samplerSMAA, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Shaders
	// -------
	const std::string macroDefineAlphaMasked = "#define ALPHA_MASKED";
	Shader passDirectShadow("src/shaders/shadow.vert", "src/shaders/shadow.frag");
	Shader passDirectShadowAlphaMasked("src/shaders/shadow.vert", "src/shaders/shadow.frag", "", macroDefineAlphaMasked);
	Shader passGeometry("src/shaders/geometry.vert", "src/shaders/geometry.frag");
	Shader passGeometryAlphaMasked("src/shaders/geometry.vert", "src/shaders/geometry.frag", "", macroDefineAlphaMasked);
	Shader passShading("src/shaders/uv.vert", "src/shaders/shading.frag");
	Shader passExposureTone("src/shaders/uv.vert", "src/shaders/exposureToneMap.frag");
	Shader passPassThrough("src/shaders/uv.vert", "src/shaders/passThrough.frag");

	Shader passDepthVelocityDownsample("src/shaders/uv.vert", "src/shaders/depthVelocityDownsample.frag");
	Shader passSsao("src/shaders/uv.vert", "src/shaders/gtao.frag");
	Shader passSsaoSpatialDenoiser("src/shaders/uv.vert", "src/shaders/gtaoSpatialDenoiser.frag");
	Shader passSsaoTemporalDenoiser("src/shaders/uv.vert", "src/shaders/gtaoTemporalDenoiser.frag");

	const std::string macroDefineSmaa = std::string("#define g_kWScreen ") + std::to_string(g_kWScreen) + "\n"
		"#define g_kHScreen " + std::to_string(g_kHScreen) + "\n#define SMAA_REPROJECTION 1";
	Shader passSmaaEdge("src/shaders/smaaEdgeDetection.vert", "src/shaders/smaaEdgeDetection.frag", "", macroDefineSmaa);
	Shader passSmaaBlending("src/shaders/smaaBlendingWeightCalculation.vert", "src/shaders/smaaBlendingWeightCalculation.frag", "", macroDefineSmaa);
	Shader passSmaaNeighborhood("src/shaders/smaaNeighborhoodBlending.vert", "src/shaders/smaaNeighborhoodBlending.frag", "", macroDefineSmaa);
	Shader passSmaaTemporal("src/shaders/uv.vert", "src/shaders/smaaTemporalResolve.frag", "", macroDefineSmaa);

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

		if constexpr (!g_kVSync)
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
			// flipped y values (from SMAA.h) due to clip origin in lower left corner
			// premultiplied by 2
			const Vec2 aJitter[2] = { Vec2( 0.5, 0.5),
									  Vec2(-0.5,-0.5) };
			return aJitter[frameCount % 2] * Vec2(1.f / g_kWScreen, 1.f / g_kHScreen);
		};
		auto JitterProjection = [&GetJitter](const Mat4& proj, const U64 frameCount) {
			const Vec2 jitter = GetJitter(frameCount);
			if (g_smaa == Smaa::T2x)
				return glm::translate(glm::identity<Mat4>(), Vec3(jitter, 0)) * proj;
			else
				return proj;
		};
		const float nearPlane = 0.1;
		const Mat4 projection = JitterProjection(CalculateInfReversedZProj(g_camera, (F32)g_kWScreen / (F32)g_kHScreen, nearPlane), frameCount);
		const Mat4 view = g_camera.GetViewMatrix();
		auto SetUniformsBasics = [&](Shader& shader) {
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
				passSsao.SetFloat("WsRadius", g_wsSizeKernelAo);
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
				passSsaoTemporalDenoiser.SetFloat("RateOfChange", g_rateOfChangeAo);
				passSsaoTemporalDenoiser.SetFloat("Near", nearPlane);
				passSsaoTemporalDenoiser.SetVec2("Scaling", Vec2(g_kWScreen / 2, g_kHScreen / 2));
				RenderQuad();
			}
			glViewport(0, 0, g_kWScreen, g_kHScreen);
			std::swap(bufDepthHalfResCurr, bufDepthHalfResPrev);
		}
		// deffered pass + generate mipmaps for bufDiffuseLight
		// ----------------------------------------------------
		{
			const U32 kNumPointLights = 4;
			const std::array<Vec3, kNumPointLights> aWsPointLightPosition = {
				Vec3(0, 10, 0),
				Vec3(-50, 10, -50),
				Vec3(50, 10, 50),
				Vec3(50, 10, -50)
			};
			const std::array<Vec3, kNumPointLights> aLightColor = {
				Vec3(50),
				Vec3(50),
				Vec3(50),
				Vec3(50)
			};

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
			passShading.SetVec3Arr("AWsPointLightPosition", aWsPointLightPosition.data(), aWsPointLightPosition.size());
			passShading.SetVec3Arr("APointLightColor", aLightColor.data(), aLightColor.size());
			passShading.SetMat4("InvViewProj", glm::inverse(projection* view));
			passShading.SetFloat("Near", nearPlane);
			passShading.SetBool("EnableAO", g_enableAO);
			glBindTextureUnit(0, bufDiffuseSpec);
			glBindTextureUnit(1, bufNormal);
			glBindTextureUnit(2, bufDepth);
			glBindTextureUnit(3, bufDepthShadow);
			glBindTextureUnit(4, bufDepthShadow);
			glBindTextureUnit(5, bufRandomAngles); // lack of sampler is intentional
			glBindTextureUnit(6, bufSsaoAccCurr);
			glBindSampler(0, samplerPointClamp);
			glBindSampler(1, samplerPointClamp);
			glBindSampler(2, samplerPointClamp);
			glBindSampler(3, samplerShadowPCF);
			glBindSampler(4, samplerShadowDepth);
			glBindSampler(6, samplerPointClamp);
			
			glDisable(GL_DEPTH_TEST); // also disables depth writes
			RenderQuad();
			glEnable(GL_DEPTH_TEST);
			
			glGenerateTextureMipmap(bufDiffuseLight);
		}
		std::swap(bufSsaoAccCurr, bufSsaoAccPrev);
		// apply exposure, tone mapping and gamma correction
		// -------------------------------------------------
		{
			glBindFramebuffer(GL_FRAMEBUFFER, fboBack);

			if (g_showShadowMap) {
				glBindTextureUnit(2, bufDepthShadow);
				glBindSampler(2, samplerShadowDepth);
				passExposureTone.SetUInt("IdxCascade", g_cascadeIdx);
			} else if (g_showAO) {
				glBindTextureUnit(0, bufSsaoAccCurr);
				glBindSampler(0, samplerPointClamp);
			} else {
				glBindTextureUnit(0, bufHdr);
				glBindTextureUnit(1, bufDiffuseLight);
				glBindSampler(0, samplerPointClamp);
				glBindSampler(1, samplerTrilinearClamp);
			}
			passExposureTone.SetFloat("Exposure", g_exposure);
			passExposureTone.SetFloat("LevelLastMipMap", log2f(g_kWScreen));
			passExposureTone.SetBool("ShowShadowMap", g_showShadowMap);
			passExposureTone.SetBool("ShowAO", g_showAO);
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
		
		// SMAA
		// ----
		if (g_smaa != Smaa::None && !g_showShadowMap && !g_showAO) {
			glClearTexImage(bufSmaaEdge, 0, GL_RG, GL_UNSIGNED_BYTE, nullptr);
			glClearTexImage(bufSmaaBlend, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			glClearTexImage(bufStencil, 0, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, nullptr);

			// 1x
			{
				// edge detection
				{
					glEnable(GL_STENCIL_TEST);
					glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
					glStencilFunc(GL_ALWAYS, 0x01, 0x01);
					glStencilMask(0x01);
					glBindFramebuffer(GL_FRAMEBUFFER, fboSmaa);
					glNamedFramebufferTexture(fboSmaa, GL_COLOR_ATTACHMENT0, bufSmaaEdge, 0);
					glBindTextureUnit(0, bufLdr);
					glBindSampler(0, samplerPointClamp);
					passSmaaEdge.Use();
					RenderQuad();
				}
				// blending weight calculation
				{
					glStencilFunc(GL_EQUAL, 0x01, 0x01);
					glStencilMask(0);
					glNamedFramebufferTexture(fboSmaa, GL_COLOR_ATTACHMENT0, bufSmaaBlend, 0);
					glBindTextureUnit(0, bufSmaaEdge);
					glBindTextureUnit(1, bufSmaaArea);
					glBindTextureUnit(2, bufSmaaSearch);
					for (int i = 0; i <= 2; i++)
						glBindSampler(i, samplerSMAA);
					auto GetSubsampleIndices = [](U64 frame) {
						const Vec4 indicies[2] = { Vec4(1, 1, 1, 0),
												   Vec4(2, 2, 2, 0) };
						if (g_enableSmaaSubsambleIndicies)
							return indicies[frame % 2];
						else
							return Vec4(0);
					};
					passSmaaBlending.Use();
					passSmaaBlending.SetVec4("SubsampleIndices", GetSubsampleIndices(frameCount));
					RenderQuad();
					glDisable(GL_STENCIL_TEST);
				}
				// neighborhood blending
				{
					glNamedFramebufferTexture(fboSmaa, GL_COLOR_ATTACHMENT0, bufSmaaCurrentSrgb, 0);
					glBindTextureUnit(0, bufLdrSrgb);
					glBindTextureUnit(1, bufSmaaBlend);
					glBindTextureUnit(2, bufVelocity);
					glBindSampler(0, samplerSMAA);
					glBindSampler(1, samplerSMAA);
					glBindSampler(2, samplerPointClamp);
					passSmaaNeighborhood.Use();
					glEnable(GL_FRAMEBUFFER_SRGB);
					RenderQuad();
					glDisable(GL_FRAMEBUFFER_SRGB);
				}
			}

			// T2x
			if (g_smaa == Smaa::T2x) {
				// temporal resolve
				{
					glBindFramebuffer(GL_FRAMEBUFFER, 0);
					glBindTextureUnit(0, bufSmaaCurrentSrgb);
					glBindTextureUnit(1, bufSmaaPreviousSrgb);
					glBindTextureUnit(2, bufVelocity);
					glBindSampler(0, samplerPointClamp);
					glBindSampler(1, samplerSMAA);
					glBindSampler(2, samplerPointClamp);

					passSmaaTemporal.Use();
					glEnable(GL_FRAMEBUFFER_SRGB);
					RenderQuad();
					glDisable(GL_FRAMEBUFFER_SRGB);
				}
				std::swap(bufSmaaCurrentSrgb, bufSmaaPreviousSrgb);
			} else {
				// pass through to back buffer
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glBindTextureUnit(0, bufSmaaCurrentSrgb);
				glBindSampler(0, samplerPointClamp);
				passPassThrough.Use();
				glEnable(GL_FRAMEBUFFER_SRGB);
				RenderQuad();
				glDisable(GL_FRAMEBUFFER_SRGB);
			}
			
		}
		else
		// pass through to back buffer
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glBindTextureUnit(0, bufLdr);
			glBindSampler(0, samplerPointClamp);
			passPassThrough.Use();
			RenderQuad();
		}

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		if constexpr (g_kVSync)
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
		g_smaa = Smaa((static_cast<I32>(g_smaa) + 1) % 3);
	if (key == GLFW_KEY_X)
		g_enableSmaaSubsambleIndicies = !g_enableSmaaSubsambleIndicies;
	if (key == GLFW_KEY_F1)
		g_enableAO = !g_enableAO;
	if (key == GLFW_KEY_F2)
		g_showAO = !g_showAO;
}

void CallbackMessage(GLE source, GLE type, GLU id, GLE severity, GLS length,
	const GLC* message, const void* userParam)
{
	if (type == 0x8251) // silence message about static draw
		return;
	if (type == 0x824e) // warning about undefined behaviour that shadow map is simultaneously used with and without PCF, it works on NVidia and AMD anyway
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
		g_wsSizeKernelAo -= 0.1;
	if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS)
		g_wsSizeKernelAo += 0.1;
	g_wsSizeKernelAo = glm::clamp<F32>(g_wsSizeKernelAo, 0, 50);
	
	if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
		g_rateOfChangeAo -= 0.01;
	if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
		g_rateOfChangeAo += 0.01;
	g_rateOfChangeAo = glm::clamp<F32>(g_rateOfChangeAo, 0.01, 1);
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