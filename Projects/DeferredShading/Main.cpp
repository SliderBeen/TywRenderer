/*
*	Copyright 2015-2016 Tomas Mikalauskas. All rights reserved.
*	GitHub repository - https://github.com/TywyllSoftware/TywRenderer
*	This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <RendererPch\stdafx.h>
#include <iomanip>

//Triangle Includes
#include "Main.h"
#include <conio.h>

//Renderer Includes
#include <Renderer\VKRenderer.h>
#include <Renderer\Vulkan\VulkanTools.h>
#include <Renderer\Vulkan\VulkanTextureLoader.h>

//Model Loader Includes
#include <Renderer\MeshLoader\Model_local.h>
#include <Renderer\MeshLoader\ImageManager.h>
#include <Renderer\MeshLoader\Material.h>

//Vulkan Includes
#include <Renderer\Vulkan\VkBufferObject.h>
#include <Renderer\Vulkan\VulkanSwapChain.h>


//Font Rendering
#include <Renderer\ThirdParty\FreeType\VkFont.h>


//math
#include <External\glm\glm\gtc\matrix_inverse.hpp>

//Camera
#include <Renderer\Camera.h>



//Global variables
//====================================================================================
#if defined(_WIN32)
#define KEY_ESCAPE VK_ESCAPE 
#define KEY_F1 VK_F1
#define KEY_F2 VK_F2
#define KEY_W 0x57
#define KEY_A 0x41
#define KEY_S 0x53
#define KEY_D 0x44
#define KEY_P 0x50
#define KEY_SPACE 0x20
#define KEY_KPADD 0x6B
#define KEY_KPSUB 0x6D
#define KEY_B 0x42
#define KEY_F 0x46
#define KEY_L 0x4C
#define KEY_N 0x4E
#define KEY_O 0x4F
#define KEY_T 0x54
#elif defined(__ANDROID__)
// Dummy key codes 
#define KEY_ESCAPE 0x0
#define KEY_F1 0x1
#define KEY_F2 0x2
#define KEY_W 0x3
#define KEY_A 0x4
#define KEY_S 0x5
#define KEY_D 0x6
#define KEY_P 0x7
#define KEY_SPACE 0x8
#define KEY_KPADD 0x9
#define KEY_KPSUB 0xA
#define KEY_B 0xB
#define KEY_F 0xC
#define KEY_L 0xD
#define KEY_N 0xE
#define KEY_O 0xF
#define KEY_T 0x10
#elif defined(__linux__)
#define KEY_ESCAPE 0x9
#define KEY_F1 0x43
#define KEY_F2 0x44
#define KEY_W 0x19
#define KEY_A 0x26
#define KEY_S 0x27
#define KEY_D 0x28
#define KEY_P 0x21
#define KEY_SPACE 0x41
#define KEY_KPADD 0x56
#define KEY_KPSUB 0x52
#define KEY_B 0x38
#define KEY_F 0x29
#define KEY_L 0x2E
#define KEY_N 0x39
#define KEY_O 0x20
#define KEY_T 0x1C
#endif

// todo: Android gamepad keycodes outside of define for now
#define GAMEPAD_BUTTON_A 0x1000
#define GAMEPAD_BUTTON_B 0x1001
#define GAMEPAD_BUTTON_X 0x1002
#define GAMEPAD_BUTTON_Y 0x1003
#define GAMEPAD_BUTTON_L1 0x1004
#define GAMEPAD_BUTTON_R1 0x1005
#define GAMEPAD_BUTTON_START 0x1006


uint32_t	g_iDesktopWidth = 0;
uint32_t	g_iDesktopHeight = 0;
bool		g_bPrepared = false;

glm::vec3	g_Rotation = glm::vec3();
glm::vec2	g_MousePos;


// Use to adjust mouse rotation speed
float		g_RotationSpeed = 1.0f;
// Use to adjust mouse zoom speed
float		g_ZoomSpeed = 1.0f;
float       g_zoom = 1.0f;

VkClearColorValue g_DefaultClearColor = { { 0.5f, 0.5f, 0.5f, 1.0f } };


#define VERTEX_BUFFER_BIND_ID 0
// Set to "true" to enable Vulkan's validation layers
// See vulkandebug.cpp for details
#define ENABLE_VALIDATION false
// Set to "true" to use staging buffers for uploading
// vertex and index data to device local memory
// See "prepareVertices" for details on what's staging
// and on why to use it
#define USE_STAGING true

#define GAMEPAD_BUTTON_A 0x1000
#define GAMEPAD_BUTTON_B 0x1001
#define GAMEPAD_BUTTON_X 0x1002
#define GAMEPAD_BUTTON_Y 0x1003
#define GAMEPAD_BUTTON_L1 0x1004
#define GAMEPAD_BUTTON_R1 0x1005
#define GAMEPAD_BUTTON_START 0x1006

// Framebuffer properties
#if defined(__ANDROID__)
#define GBUFF_DIM 1024
#else
#define GBUFF_DIM 2048
#endif
#define GBUFF_FILTER VK_FILTER_LINEAR


//====================================================================================


//functions
//====================================================================================

bool GenerateEvents(MSG& msg);
void WIN_Sizing(WORD side, RECT *rect);
LRESULT CALLBACK HandleWindowMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

//====================================================================================



class Renderer final: public VKRenderer
{
public:
	VkFont*	m_VkFont;
	Camera  m_Camera;
	bool	m_bViewUpdated;
private:
	// The pipeline (state objects) is a static store for the 3D pipeline states (including shaders)
	// Other than OpenGL this makes you setup the render states up-front
	// If different render states are required you need to setup multiple pipelines
	// and switch between them
	// Note that there are a few dynamic states (scissor, viewport, line width) that
	// can be set from a command buffer and does not have to be part of the pipeline
	// This basic example only uses one pipeline
	VkPipeline pipeline;

	// The pipeline layout defines the resource binding slots to be used with a pipeline
	// This includes bindings for buffes (ubos, ssbos), images and sampler
	// A pipeline layout can be used for multiple pipeline (state objects) as long as 
	// their shaders use the same binding layout
	VkPipelineLayout pipelineLayout;

	// The descriptor set stores the resources bound to the binding points in a shader
	// It connects the binding points of the different shaders with the buffers and images
	// used for those bindings
	VkDescriptorSet descriptorSet;

	// The descriptor set layout describes the shader binding points without referencing
	// the actual buffers. 
	// Like the pipeline layout it's pretty much a blueprint and can be used with
	// different descriptor sets as long as the binding points (and shaders) match
	VkDescriptorSetLayout descriptorSetLayout;

	// Synchronization semaphores
	// Semaphores are used to synchronize dependencies between command buffers
	// We use them to ensure that we e.g. don't present to the swap chain
	// until all rendering has completed
	struct {
		// Swap chain image presentation
		VkSemaphore presentComplete;
		// Command buffer submission and execution
		VkSemaphore renderComplete;
		// Text overlay submission and execution
		VkSemaphore textOverlayComplete;
	} Semaphores;


	float zNear = 1.0f;
	float zFar = 256.0f;

	struct {
		glm::mat4 projectionMatrix;
		glm::mat4 modelMatrix;
		glm::mat4 viewMatrix;
		glm::mat4 normal;
		glm::vec4 viewPos;
		glm::vec4 instancePos[3];
	}m_uboVS;

	uint32_t numVerts = 0;
	uint32_t numUvs = 0;
	uint32_t numNormals = 0;

	std::vector<VkBufferObject_s>	listLocalBuffers;
	std::vector<VkDescriptorSet>	listDescriptros;
	std::vector<uint32_t>			meshSize;




	// Framebuffer for offscreen rendering
	struct FrameBufferAttachment {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
		VkFormat format;
	};
	struct FrameBuffer {
		int32_t width, height;
		VkFramebuffer frameBuffer;
		FrameBufferAttachment position;
		FrameBufferAttachment nm; //normal and diffuse
		FrameBufferAttachment depth;
		VkRenderPass renderPass;
	} offScreenFrameBuf;

	struct {
		VkBuffer buffer;
		VkDeviceMemory memory;
		VkDescriptorBufferInfo descriptor;
	}  uniformDataVS;


	// One sampler for the frame buffer color attachments
	VkSampler colorSampler;


	struct 
	{
		VkTools::UniformData quad;
		VkTools::UniformData fsLights;
		VkTools::UniformData vsFullScreen;
	}  uniformData;

	struct Light 
	{
		glm::vec4 position;
		glm::vec3 color;
		float radius;
	};

	struct 
	{
		Light lights[6];
		glm::vec4 viewPos;
	} uboFragmentLights;


	struct
	{
		glm::mat4 projection;
		glm::mat4 model;
	} uboFullScreen;

	VkPipeline			frameBufferPipeline;
	VkCommandBuffer		GBufferScreenCmdBuffer = VK_NULL_HANDLE;
	VkSemaphore			GBufferSemaphore = VK_NULL_HANDLE;
	VkPipelineLayout	frameBufferPipelineLayout;
	VkDescriptorSet		frameBufferDescriptorSet;

	VkBufferObject_s				quadVbo;
	VkBufferObject_s				quadIndexVbo;

	//plane and quad
	VkPipeline quadPipeline;
	VkPipelineLayout quadPipelineLayout;
	VkDescriptorSet  quadDescriptorSet;
	VkDescriptorSet  defferedModelDescriptorSet;

	struct {
		glm::mat4 mvp;
		glm::uint32_t textureType;
	} quadUniformData;

	RenderModelStatic staticModel;
public:
	Renderer();
	~Renderer();

	void BuildCommandBuffers() override;
	void UpdateUniformBuffers() override;
	void PrepareUniformBuffers() override;
	void PrepareVertices(bool useStagingBuffers) override;
	void VLoadTexture(std::string fileName, VkFormat format, bool forceLinearTiling, bool bUseCubeMap = false) override;
	void PreparePipeline() override;
	void SetupDescriptorSet() override;
	void SetupDescriptorSetLayout() override;
	void SetupDescriptorPool() override;
	void LoadAssets() override;
	void PrepareSemaphore() override;
	void StartFrame() override;


	void ChangeLodBias(float delta);
	void BeginTextUpdate();
	void CreateFrameBuffer();
	void CreateAttachement(VkFormat format,VkImageUsageFlagBits usage,FrameBufferAttachment *attachment,VkCommandBuffer layoutCmd);
	void PrepareFramebufferCommands();
	void GenerateQuad();
	void UpdateQuadUniformData(const glm::vec3& pos = glm::vec3(1.0, 1.0, 0.0));
	void UpdateUniformBuffersLights();
};

Renderer::Renderer():m_bViewUpdated(false)
{
	m_Camera.type = Camera::CameraType::firstperson;
	m_Camera.movementSpeed = 5.0f;
#ifndef __ANDROID__
	m_Camera.rotationSpeed = 0.25f;
#endif
	m_Camera.position = { 0.47f, -3.8f, -3.34f };
	m_Camera.setRotation(glm::vec3(-164.0f, -164.0f, 0.0f));
	m_Camera.setPerspective(60.0f, (float)1280 / (float)720, 0.1f, 256.0f);
}

Renderer::~Renderer()
{
	SAFE_DELETE(m_VkFont);

	vkDestroySampler(m_pWRenderer->m_SwapChain.device, colorSampler, nullptr);

	//Destroy model data
	staticModel.Clear(m_pWRenderer->m_SwapChain.device);


	//Delete memory
	for (int i = 0; i < listLocalBuffers.size(); i++)
	{
		VkBufferObject::DeleteBufferMemory(m_pWRenderer->m_SwapChain.device, listLocalBuffers[i], nullptr);
	}


	//Destroy Shader Module
	for (int i = 0; i < m_ShaderModules.size(); i++)
	{
		vkDestroyShaderModule(m_pWRenderer->m_SwapChain.device, m_ShaderModules[i], nullptr);
	}

	//Uniform Data
	vkDestroyBuffer(m_pWRenderer->m_SwapChain.device, uniformDataVS.buffer, nullptr);
	vkFreeMemory(m_pWRenderer->m_SwapChain.device, uniformDataVS.memory, nullptr);
	VkTools::DestroyUniformData(m_pWRenderer->m_SwapChain.device, &uniformData.quad);
	VkTools::DestroyUniformData(m_pWRenderer->m_SwapChain.device, &uniformData.fsLights);
	VkTools::DestroyUniformData(m_pWRenderer->m_SwapChain.device, &uniformData.vsFullScreen);

	//QUad
	VkBufferObject::DeleteBufferMemory(m_pWRenderer->m_SwapChain.device, quadVbo, nullptr);
	VkBufferObject::DeleteBufferMemory(m_pWRenderer->m_SwapChain.device, quadIndexVbo, nullptr);

	//Position
	vkDestroyImageView(m_pWRenderer->m_SwapChain.device, offScreenFrameBuf.position.view, nullptr);
	vkDestroyImage(m_pWRenderer->m_SwapChain.device, offScreenFrameBuf.position.image, nullptr);
	vkFreeMemory(m_pWRenderer->m_SwapChain.device, offScreenFrameBuf.position.mem, nullptr);

	//Normal and Diffuse
	vkDestroyImageView(m_pWRenderer->m_SwapChain.device, offScreenFrameBuf.nm.view, nullptr);
	vkDestroyImage(m_pWRenderer->m_SwapChain.device, offScreenFrameBuf.nm.image, nullptr);
	vkFreeMemory(m_pWRenderer->m_SwapChain.device, offScreenFrameBuf.nm.mem, nullptr);

	//Depth
	vkDestroyImageView(m_pWRenderer->m_SwapChain.device, offScreenFrameBuf.depth.view, nullptr);
	vkDestroyImage(m_pWRenderer->m_SwapChain.device, offScreenFrameBuf.depth.image, nullptr);
	vkFreeMemory(m_pWRenderer->m_SwapChain.device, offScreenFrameBuf.depth.mem, nullptr);


	//Destroy framebuffer
	vkDestroyFramebuffer(m_pWRenderer->m_SwapChain.device, offScreenFrameBuf.frameBuffer, nullptr);


	//Destroy FrameBufferPipeline
	vkDestroyPipeline(m_pWRenderer->m_SwapChain.device, frameBufferPipeline, nullptr);
	vkDestroyPipeline(m_pWRenderer->m_SwapChain.device, quadPipeline, nullptr);
	vkDestroyPipeline(m_pWRenderer->m_SwapChain.device, pipeline, nullptr);

	//Destroy PipelineLayout
	vkDestroyPipelineLayout(m_pWRenderer->m_SwapChain.device, frameBufferPipelineLayout, nullptr);
	vkDestroyPipelineLayout(m_pWRenderer->m_SwapChain.device, pipelineLayout, nullptr);

	//Destroy layout
	vkDestroyDescriptorSetLayout(m_pWRenderer->m_SwapChain.device, descriptorSetLayout, nullptr);

	//Destroy RenderPass
	vkDestroyRenderPass(m_pWRenderer->m_SwapChain.device, offScreenFrameBuf.renderPass, nullptr);

	//Destroy Command Buffer and Semaphore
	vkFreeCommandBuffers(m_pWRenderer->m_SwapChain.device, m_pWRenderer->m_CmdPool, 1, &GBufferScreenCmdBuffer);
	vkDestroySemaphore(m_pWRenderer->m_SwapChain.device, GBufferSemaphore, nullptr);

	//Release semaphores
	vkDestroySemaphore(m_pWRenderer->m_SwapChain.device, Semaphores.presentComplete, nullptr);
	vkDestroySemaphore(m_pWRenderer->m_SwapChain.device, Semaphores.renderComplete, nullptr);
	vkDestroySemaphore(m_pWRenderer->m_SwapChain.device, Semaphores.textOverlayComplete, nullptr);
}

void Renderer::PrepareSemaphore()
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = NULL;

	// This semaphore ensures that the image is complete
	// before starting to submit again
	VK_CHECK_RESULT(vkCreateSemaphore(m_pWRenderer->m_SwapChain.device, &semaphoreCreateInfo, nullptr, &Semaphores.presentComplete));

	// This semaphore ensures that all commands submitted
	// have been finished before submitting the image to the queue
	VK_CHECK_RESULT(vkCreateSemaphore(m_pWRenderer->m_SwapChain.device, &semaphoreCreateInfo, nullptr, &Semaphores.renderComplete));


	// This semaphore ensures that all commands submitted
	// have been finished before submitting the image to the queue
	VK_CHECK_RESULT(vkCreateSemaphore(m_pWRenderer->m_SwapChain.device, &semaphoreCreateInfo, nullptr, &Semaphores.textOverlayComplete));
}


void Renderer::UpdateUniformBuffersLights()
{
	timer += timerSpeed * frameTimer;
	if (timer > 1.0)
	{
		timer -= 1.0f;
	}


	// White
	uboFragmentLights.lights[0].position = glm::vec4(0.0f, -4.0f, 0.0f, 0.0f);
	uboFragmentLights.lights[0].color = glm::vec3(1.5f);
	uboFragmentLights.lights[0].radius = 25.0f;
	// Red
	uboFragmentLights.lights[1].position = glm::vec4(0.0f, -4.0f, 0.0f, 0.0f);
	uboFragmentLights.lights[1].color = glm::vec3(1.0f, 0.0f, 0.0f);
	uboFragmentLights.lights[1].radius = 25.0f;
	// Blue
	uboFragmentLights.lights[2].position = glm::vec4(0.0f, -4.0f, 0.0f, 0.0f);
	uboFragmentLights.lights[2].color = glm::vec3(0.0f, 0.0f, 2.5f);
	uboFragmentLights.lights[2].radius = 25.0f;
	// Yellow
	uboFragmentLights.lights[3].position = glm::vec4(0.0f, -4.0f, 0.0f, 0.0f);
	uboFragmentLights.lights[3].color = glm::vec3(1.0f, 1.0f, 0.0f);
	uboFragmentLights.lights[3].radius = 25.0f;
	// Green
	uboFragmentLights.lights[4].position = glm::vec4(0.0f, -4.0f, 0.0f, 0.0f);
	uboFragmentLights.lights[4].color = glm::vec3(0.0f, 1.0f, 0.2f);
	uboFragmentLights.lights[4].radius = 25.0f;
	// Yellow
	uboFragmentLights.lights[5].position = glm::vec4(0.0f, -4.0f, 0.0f, 0.0f);
	uboFragmentLights.lights[5].color = glm::vec3(1.0f, 0.7f, 0.3f);
	uboFragmentLights.lights[5].radius = 25.0f;

	//Update lights
	uboFragmentLights.lights[0].position.x = -4.0f + sin(glm::radians(-360.0f * timer)) * 5.0f;
	uboFragmentLights.lights[0].position.z = 0.0f - cos(glm::radians(-360.0f * timer)) * 5.0f;

	uboFragmentLights.lights[1].position.x = -4.0f + sin(glm::radians(-360.0f * timer + 135.0f)) * 5.0f;
	uboFragmentLights.lights[1].position.z = 0.0f + cos(glm::radians(360.0f * timer) + 45.0f) * 5.0f;

	uboFragmentLights.lights[2].position.x = -4.0f + sin(glm::radians(-360.0f * timer + 45.0f)) * 5.0f;
	uboFragmentLights.lights[2].position.z = 0.0f + cos(glm::radians(360.0f * timer + 45.0f)) * 5.0f;

	uboFragmentLights.lights[3].position.x = -4.0f + sin(glm::radians(-360.0f * timer + 135.0f)) * 5.0f;
	uboFragmentLights.lights[3].position.x = 0.0f - cos(glm::radians(360.0f * timer + 45.0f)) * 5.0f;

	uboFragmentLights.lights[4].position.x = -4.0f + sin(glm::radians(-360.0f * timer + 90.0f)) * 5.0f;
	uboFragmentLights.lights[4].position.z = 0.0f - cos(glm::radians(360.0f * timer + 45.0f)) * 5.0f;

	uboFragmentLights.lights[5].position.x = -4.0f + sin(glm::radians(-360.0f * timer + 135.0f)) * 5.0f;
	uboFragmentLights.lights[5].position.z = 0.0f - cos(glm::radians(-360.0f * timer - 45.0f)) * 5.0f;

	// Current view position
	uboFragmentLights.viewPos = glm::vec4(m_Camera.position, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);

	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(m_pWRenderer->m_SwapChain.device, uniformData.fsLights.memory, 0, sizeof(uboFragmentLights), 0, (void **)&pData));
	memcpy(pData, &uboFragmentLights, sizeof(uboFragmentLights));
	vkUnmapMemory(m_pWRenderer->m_SwapChain.device, uniformData.fsLights.memory);
}


void Renderer::GenerateQuad()
{
#define DIM 1.0f
#define NORMAL { 0.0f, 0.0f, 1.0f }
	std::vector<drawVert> vertData =
	{
		{ glm::vec3(DIM,DIM,    0),	   glm::vec3(0,0,1),  glm::vec3(0,0,0), glm::vec3(0,0,0),  glm::vec2(1,1) },
		{ glm::vec3(-DIM,DIM,  0),     glm::vec3(0,0,1),  glm::vec3(0,0,0), glm::vec3(0,0,0),  glm::vec2(0,1) },
		{ glm::vec3(-DIM, -DIM,  0),   glm::vec3(0,0,1),  glm::vec3(0,0,0), glm::vec3(0,0,0),  glm::vec2(0,0) },
		{ glm::vec3(DIM,-DIM,  0),     glm::vec3(0,0,1),  glm::vec3(0,0,0), glm::vec3(0,0,0),  glm::vec2(1,0) },
	};

	// Setup indices
	std::vector<uint32_t> indexBuffer = { 0, 1, 2, 2,3,0 };


	VkBufferObject_s staggingBufferVbo;
	VkBufferObject_s staggingIndexBufferVbo;

	//Create stagign buffer
	//Verts
	VkBufferObject::CreateBuffer(
		m_pWRenderer->m_SwapChain,
		m_pWRenderer->m_DeviceMemoryProperties,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		static_cast<uint32_t>(sizeof(drawVert) * vertData.size()),
		staggingBufferVbo,
		vertData.data());

	//Index
	VkBufferObject::CreateBuffer(
		m_pWRenderer->m_SwapChain,
		m_pWRenderer->m_DeviceMemoryProperties,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		static_cast<uint32_t>(sizeof(uint32_t) * indexBuffer.size()),
		staggingIndexBufferVbo,
		indexBuffer.data());


	//Create Local Copy
	//Verts
	VkBufferObject::CreateBuffer(
		m_pWRenderer->m_SwapChain,
		m_pWRenderer->m_DeviceMemoryProperties,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		static_cast<uint32_t>(sizeof(drawVert) * vertData.size()),
		quadVbo);

	//Index
	VkBufferObject::CreateBuffer(
		m_pWRenderer->m_SwapChain,
		m_pWRenderer->m_DeviceMemoryProperties,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		static_cast<uint32_t>(sizeof(uint32_t) * indexBuffer.size()),
		quadIndexVbo);


	//Create new command buffer
	VkCommandBuffer copyCmd = GetCommandBuffer(true);


	//Submit info to the queue
	VkBufferObject::SubmitBufferObjects(
		copyCmd,
		m_pWRenderer->m_Queue,
		*m_pWRenderer,
		static_cast<uint32_t>(sizeof(drawVert) * vertData.size()),
		staggingBufferVbo,
		quadVbo, (drawVertFlags::Vertex | drawVertFlags::Uv | drawVertFlags::Normal));


	copyCmd = GetCommandBuffer(true);
	VkBufferObject::SubmitBufferObjects(
		copyCmd,
		m_pWRenderer->m_Queue,
		*m_pWRenderer,
		static_cast<uint32_t>(sizeof(uint32_t) * indexBuffer.size()),
		staggingIndexBufferVbo,
		quadIndexVbo, drawVertFlags::None);
}

void Renderer::UpdateQuadUniformData(const glm::vec3& pos)
{
	static glm::mat4 perspective, modelMatrix;
	float AR = (float)g_iDesktopHeight / (float)g_iDesktopWidth;

	perspective = glm::ortho(2.5f / AR, 0.0f, 0.0f, 2.5f, -1.0f, 1.0f);
	modelMatrix = glm::scale(glm::mat4(), glm::vec3(0.3f, 0.3f, 0.3f));
	modelMatrix = glm::translate(modelMatrix, pos);
	quadUniformData.mvp = perspective  * modelMatrix;
	quadUniformData.textureType = 3;

	// Map uniform buffer and update it
	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(m_pWRenderer->m_SwapChain.device, uniformData.quad.memory, 0, sizeof(quadUniformData), 0, (void **)&pData));
	memcpy(pData, &quadUniformData, sizeof(quadUniformData));
	vkUnmapMemory(m_pWRenderer->m_SwapChain.device, uniformData.quad.memory);
}

void Renderer::PrepareFramebufferCommands()
{
	if (GBufferScreenCmdBuffer == VK_NULL_HANDLE)
	{
		VkCommandBufferAllocateInfo cmdBufAllocateInfo =
			VkTools::Initializer::CommandBufferAllocateInfo(
				m_pWRenderer->m_CmdPool,
				VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				1);

		VK_CHECK_RESULT(vkAllocateCommandBuffers(m_pWRenderer->m_SwapChain.device, &cmdBufAllocateInfo, &GBufferScreenCmdBuffer));
	}

	// Create a semaphore used to synchronize offscreen rendering and usage
	VkSemaphoreCreateInfo semaphoreCreateInfo = VkTools::Initializer::SemaphoreCreateInfo();
	VK_CHECK_RESULT(vkCreateSemaphore(m_pWRenderer->m_SwapChain.device, &semaphoreCreateInfo, nullptr, &GBufferSemaphore));

	VkCommandBufferBeginInfo cmdBufInfo = VkTools::Initializer::CommandBufferBeginInfo();

	std::array<VkClearValue, 3> clearValues;
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[2].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = VkTools::Initializer::RenderPassBeginInfo();
	renderPassBeginInfo.renderPass = offScreenFrameBuf.renderPass;
	renderPassBeginInfo.framebuffer = offScreenFrameBuf.frameBuffer;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = offScreenFrameBuf.width;
	renderPassBeginInfo.renderArea.extent.height = offScreenFrameBuf.height;
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues = clearValues.data();

	VK_CHECK_RESULT(vkBeginCommandBuffer(GBufferScreenCmdBuffer, &cmdBufInfo));


	VkViewport viewport = VkTools::Initializer::Viewport((float)offScreenFrameBuf.width, (float)offScreenFrameBuf.height, 0.0f, 1.0f);
	vkCmdSetViewport(GBufferScreenCmdBuffer, 0, 1, &viewport);

	VkRect2D scissor = VkTools::Initializer::Rect2D(offScreenFrameBuf.width, offScreenFrameBuf.height, 0, 0);
	vkCmdSetScissor(GBufferScreenCmdBuffer, 0, 1, &scissor);


	vkCmdBeginRenderPass(GBufferScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(GBufferScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, frameBufferPipeline);

	// Bind triangle vertex buffer (contains position and colors)
	VkDeviceSize offsets[1] = { 0 };
	for (int j = 0; j < listLocalBuffers.size(); j++)
	{
		// Bind descriptor sets describing shader binding points
		vkCmdBindDescriptorSets(GBufferScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, frameBufferPipelineLayout, 0, 1, &listDescriptros[j], 0, NULL);

		//Bind Buffer
		vkCmdBindVertexBuffers(GBufferScreenCmdBuffer, VERTEX_BUFFER_BIND_ID, 1, &listLocalBuffers[j].buffer, offsets);

		//Draw
		//vkCmdDrawIndirect(GBufferScreenCmdBuffer, nullptr , 0, 3, 0);
		vkCmdDraw(GBufferScreenCmdBuffer, meshSize[j], 3, 0, 0);
	}
	vkCmdEndRenderPass(GBufferScreenCmdBuffer);


	VK_CHECK_RESULT(vkEndCommandBuffer(GBufferScreenCmdBuffer));
}



void Renderer::CreateAttachement(
	VkFormat format,
	VkImageUsageFlagBits usage,
	FrameBufferAttachment *attachment,
	VkCommandBuffer layoutCmd)
{
	VkImageAspectFlags aspectMask = 0;
	VkImageLayout imageLayout;
	attachment->format = format;

	if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
	{
		aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
	if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
	{
		aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}


	assert(aspectMask > 0);

	VkImageCreateInfo image = VkTools::Initializer::ImageCreateInfo();
	image.imageType = VK_IMAGE_TYPE_2D;
	image.format = format;
	image.extent.width = offScreenFrameBuf.width;
	image.extent.height = offScreenFrameBuf.height;
	image.extent.depth = 1;
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

	VkMemoryAllocateInfo memAlloc = VkTools::Initializer::MemoryAllocateInfo();
	VkMemoryRequirements memReqs;

	VK_CHECK_RESULT(vkCreateImage(m_pWRenderer->m_SwapChain.device, &image, nullptr, &attachment->image));
	vkGetImageMemoryRequirements(m_pWRenderer->m_SwapChain.device, attachment->image, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = VkTools::GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_pWRenderer->m_DeviceMemoryProperties);

	VK_CHECK_RESULT(vkAllocateMemory(m_pWRenderer->m_SwapChain.device, &memAlloc, nullptr, &attachment->mem));
	VK_CHECK_RESULT(vkBindImageMemory(m_pWRenderer->m_SwapChain.device, attachment->image, attachment->mem, 0));

	VkImageViewCreateInfo imageView = VkTools::Initializer::ImageViewCreateInfo();
	imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageView.format = format;
	imageView.subresourceRange = {};
	imageView.subresourceRange.aspectMask = aspectMask;
	imageView.subresourceRange.baseMipLevel = 0;
	imageView.subresourceRange.levelCount = 1;
	imageView.subresourceRange.baseArrayLayer = 0;
	imageView.subresourceRange.layerCount = 1;
	imageView.image = attachment->image;
	VK_CHECK_RESULT(vkCreateImageView(m_pWRenderer->m_SwapChain.device, &imageView, nullptr, &attachment->view));
}


void Renderer::CreateFrameBuffer()
{

	//Create command buffer
	VkCommandBuffer layoutCmd;
	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		VkTools::Initializer::CommandBufferAllocateInfo(
			m_pWRenderer->m_CmdPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			1);

	VK_CHECK_RESULT(vkAllocateCommandBuffers(m_pWRenderer->m_SwapChain.device, &cmdBufAllocateInfo, &layoutCmd));

	VkCommandBufferBeginInfo cmdBufInfo = VkTools::Initializer::CommandBufferBeginInfo();

	//Command buffer created and started
	VK_CHECK_RESULT(vkBeginCommandBuffer(layoutCmd, &cmdBufInfo));


	offScreenFrameBuf.width = GBUFF_DIM;
	offScreenFrameBuf.height = GBUFF_DIM;

	//Normal and diffuse
	CreateAttachement(VK_FORMAT_R32G32B32A32_UINT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		&offScreenFrameBuf.nm,
		layoutCmd);

	//Position
	CreateAttachement(VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		&offScreenFrameBuf.position,
		layoutCmd);


	// Find a suitable depth format
	VkFormat attDepthFormat;
	VkBool32 validDepthFormat = VkTools::GetSupportedDepthFormat(m_pWRenderer->m_SwapChain.physicalDevice, &attDepthFormat);
	assert(validDepthFormat);

	//Depth
	CreateAttachement(attDepthFormat,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		&offScreenFrameBuf.depth,
		layoutCmd);

	//Free command buffer
	VK_CHECK_RESULT(vkEndCommandBuffer(layoutCmd));
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &layoutCmd;

	VK_CHECK_RESULT(vkQueueSubmit(m_pWRenderer->m_Queue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK_RESULT(vkQueueWaitIdle(m_pWRenderer->m_Queue));
	vkFreeCommandBuffers(m_pWRenderer->m_SwapChain.device, m_pWRenderer->m_CmdPool, 1, &layoutCmd);

	// Set up separate renderpass with references
	// to the color and depth attachments

	std::array<VkAttachmentDescription, 3> attachmentDescs = {};

	// Init attachment properties
	for (uint32_t i = 0; i < attachmentDescs.size(); ++i)
	{
		attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if (i == 2)
		{
			//depth buffer
			attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}

	// Formats
	attachmentDescs[0].format = offScreenFrameBuf.position.format;
	attachmentDescs[1].format = offScreenFrameBuf.nm.format;
	attachmentDescs[2].format = offScreenFrameBuf.depth.format;

	std::vector<VkAttachmentReference> colorReferences;
	colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 2;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pColorAttachments = colorReferences.data();
	subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
	subpass.pDepthStencilAttachment = &depthReference;

	// Use subpass dependencies for attachment layput transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pAttachments = attachmentDescs.data();
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = dependencies.data();

	VK_CHECK_RESULT(vkCreateRenderPass(m_pWRenderer->m_SwapChain.device , &renderPassInfo, nullptr, &offScreenFrameBuf.renderPass));

	std::array<VkImageView, 3> attachments;
	attachments[0] = offScreenFrameBuf.position.view;
	attachments[1] = offScreenFrameBuf.nm.view;
	attachments[2] = offScreenFrameBuf.depth.view;

	VkFramebufferCreateInfo fbufCreateInfo = {};
	fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbufCreateInfo.pNext = NULL;
	fbufCreateInfo.renderPass = offScreenFrameBuf.renderPass;
	fbufCreateInfo.pAttachments = attachments.data();
	fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	fbufCreateInfo.width = offScreenFrameBuf.width;
	fbufCreateInfo.height = offScreenFrameBuf.height;
	fbufCreateInfo.layers = 1;
	VK_CHECK_RESULT(vkCreateFramebuffer(m_pWRenderer->m_SwapChain.device, &fbufCreateInfo, nullptr, &offScreenFrameBuf.frameBuffer));

	// Create sampler to sample from the color attachments
	VkSamplerCreateInfo sampler = VkTools::Initializer::SamplerCreateInfo();
	sampler.magFilter = VK_FILTER_NEAREST;
	sampler.minFilter = VK_FILTER_NEAREST;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 0;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(m_pWRenderer->m_SwapChain.device, &sampler, nullptr, &colorSampler));
}


void Renderer::ChangeLodBias(float delta)
{


}


void Renderer::BeginTextUpdate()
{
	m_VkFont->BeginTextUpdate();

	std::stringstream ss;
	ss << std::fixed << std::setprecision(2) << "ms-" << (frameTimer * 1000.0f) <<  "-fps-" << lastFPS;
	m_VkFont->AddText(-0.22, -0.25, 0.001, 0.001, ss.str());

	m_VkFont->EndTextUpdate();
}


void Renderer::BuildCommandBuffers()
{
	//Prepare FrameBuffer Commands
	PrepareFramebufferCommands();


	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.pNext = NULL;

	// Set clear values for all framebuffer attachments with loadOp set to clear
	// We use two attachments (color and depth) that are cleared at the 
	// start of the subpass and as such we need to set clear values for both
	VkClearValue clearValues[2];
	clearValues[0].color = g_DefaultClearColor;
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = NULL;
	renderPassBeginInfo.renderPass = m_pWRenderer->m_RenderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = g_iDesktopWidth;
	renderPassBeginInfo.renderArea.extent.height = g_iDesktopHeight;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	for (int32_t i = 0; i < m_pWRenderer->m_DrawCmdBuffers.size(); ++i)
	{

		// Set target frame buffer
		renderPassBeginInfo.framebuffer = m_pWRenderer->m_FrameBuffers[i];

		VK_CHECK_RESULT(vkBeginCommandBuffer(m_pWRenderer->m_DrawCmdBuffers[i], &cmdBufInfo));

		// Start the first sub pass specified in our default render pass setup by the base class
		// This will clear the color and depth attachment
		vkCmdBeginRenderPass(m_pWRenderer->m_DrawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Update dynamic viewport state
		VkViewport viewport = {};
		viewport.height = (float)g_iDesktopHeight;
		viewport.width = (float)g_iDesktopWidth;
		viewport.minDepth = (float) 0.0f;
		viewport.maxDepth = (float) 1.0f;
		vkCmdSetViewport(m_pWRenderer->m_DrawCmdBuffers[i], 0, 1, &viewport);

		// Update dynamic scissor state
		VkRect2D scissor = {};
		scissor.extent.width = g_iDesktopWidth;
		scissor.extent.height = g_iDesktopHeight;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(m_pWRenderer->m_DrawCmdBuffers[i], 0, 1, &scissor);


		VkDeviceSize offsets[1] = { 0 };

		
		// Final composition as full screen quad
		{
			vkCmdBindPipeline(m_pWRenderer->m_DrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			vkCmdBindDescriptorSets(m_pWRenderer->m_DrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &defferedModelDescriptorSet, 0, NULL);
			vkCmdBindVertexBuffers(m_pWRenderer->m_DrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &quadVbo.buffer, offsets);
			vkCmdBindIndexBuffer(m_pWRenderer->m_DrawCmdBuffers[i], quadIndexVbo.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(m_pWRenderer->m_DrawCmdBuffers[i], 6, 1, 0, 0, 1);
		}
		


		//quad debug
		{
			vkCmdBindPipeline(m_pWRenderer->m_DrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, quadPipeline);
			vkCmdBindDescriptorSets(m_pWRenderer->m_DrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &quadDescriptorSet, 0, NULL);
			vkCmdBindVertexBuffers(m_pWRenderer->m_DrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &quadVbo.buffer, offsets);
			vkCmdBindIndexBuffer(m_pWRenderer->m_DrawCmdBuffers[i], quadIndexVbo.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(m_pWRenderer->m_DrawCmdBuffers[i], 6, 1, 0, 0, 0);
		}


		vkCmdEndRenderPass(m_pWRenderer->m_DrawCmdBuffers[i]);

		// Add a present memory barrier to the end of the command buffer
		// This will transform the frame buffer color attachment to a
		// new layout for presenting it to the windowing system integration 
		VkImageMemoryBarrier prePresentBarrier = {};
		prePresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		prePresentBarrier.pNext = NULL;
		prePresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		prePresentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		prePresentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		prePresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		prePresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		prePresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		prePresentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		prePresentBarrier.image = m_pWRenderer->m_SwapChain.buffers[i].image;

		VkImageMemoryBarrier *pMemoryBarrier = &prePresentBarrier;
		vkCmdPipelineBarrier(
			m_pWRenderer->m_DrawCmdBuffers[i],
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			0, nullptr,
			1, &prePresentBarrier);

		VK_CHECK_RESULT(vkEndCommandBuffer(m_pWRenderer->m_DrawCmdBuffers[i]));

	}
}

void Renderer::UpdateUniformBuffers()
{
	// Update matrices
	//m_uboVS.projectionMatrix = glm::perspective(glm::radians(60.0f), (float)m_WindowWidth / (float)m_WindowHeight, zNear, zFar);
	//m_uboVS.viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 5.0f, g_zoom) + g_CameraPos);

	//m_uboVS.modelMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.25f, 0.0f));
	//m_uboVS.modelMatrix = glm::rotate(m_uboVS.modelMatrix, glm::radians(g_Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	//m_uboVS.modelMatrix = glm::rotate(m_uboVS.modelMatrix, glm::radians(g_Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	//m_uboVS.modelMatrix = glm::rotate(m_uboVS.modelMatrix, glm::radians(g_Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
	

	//m_uboVS.normal = glm::inverseTranspose(m_uboVS.modelMatrix);
	//m_uboVS.viewPos = glm::vec4(0.0f, 0.0f, -15.0f, 0.0f);

	m_Camera.updateAspectRatio((float)m_WindowWidth / (float)m_WindowHeight);
	m_uboVS.projectionMatrix = m_Camera.matrices.perspective;
	m_uboVS.viewMatrix = m_Camera.matrices.view;
	m_uboVS.modelMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, g_zoom, 0.0f));
	m_uboVS.modelMatrix = glm::scale(m_uboVS.modelMatrix, glm::vec3(0.2, 0.2, 0.2));
	{
		// Map uniform buffer and update it
		uint8_t *pData;
		VK_CHECK_RESULT(vkMapMemory(m_pWRenderer->m_SwapChain.device, uniformDataVS.memory, 0, sizeof(m_uboVS), 0, (void **)&pData));
		memcpy(pData, &m_uboVS, sizeof(m_uboVS));
		vkUnmapMemory(m_pWRenderer->m_SwapChain.device, uniformDataVS.memory);
	}

	//FullScreen
	uboFullScreen.projection = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
	uboFullScreen.model = glm::mat4();
	{
		uint8_t *pData;
		VK_CHECK_RESULT(vkMapMemory(m_pWRenderer->m_SwapChain.device, uniformData.vsFullScreen.memory, 0, sizeof(uboFullScreen), 0, (void **)&pData));
		memcpy(pData, &uboFullScreen, sizeof(uboFullScreen));
		vkUnmapMemory(m_pWRenderer->m_SwapChain.device, uniformData.vsFullScreen.memory);
	}
}

void Renderer::PrepareUniformBuffers()
{
	// Prepare and initialize a uniform buffer block containing shader uniforms
	// In Vulkan there are no more single uniforms like in GL
	// All shader uniforms are passed as uniform buffer blocks 
	VkMemoryRequirements memReqs;

	// Vertex shader uniform buffer block
	VkBufferCreateInfo bufferInfo = {};
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = NULL;
	allocInfo.allocationSize = 0;
	allocInfo.memoryTypeIndex = 0;

	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(m_uboVS);
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	// Create a new buffer
	VK_CHECK_RESULT(vkCreateBuffer(m_pWRenderer->m_SwapChain.device, &bufferInfo, nullptr, &uniformDataVS.buffer));
	// Get memory requirements including size, alignment and memory type 
	vkGetBufferMemoryRequirements(m_pWRenderer->m_SwapChain.device, uniformDataVS.buffer, &memReqs);
	allocInfo.allocationSize = memReqs.size;
	// Get the memory type index that supports host visibile memory access
	// Most implementations offer multiple memory tpyes and selecting the 
	// correct one to allocate memory from is important
	// We also want the buffer to be host coherent so we don't have to flush 
	// after every update. 
	// Note that this may affect performance so you might not want to do this 
	// in a real world application that updates buffers on a regular base
	allocInfo.memoryTypeIndex = VkTools::GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_pWRenderer->m_DeviceMemoryProperties);
	// Allocate memory for the uniform buffer
	VK_CHECK_RESULT(vkAllocateMemory(m_pWRenderer->m_SwapChain.device, &allocInfo, nullptr, &(uniformDataVS.memory)));
	// Bind memory to buffer
	VK_CHECK_RESULT(vkBindBufferMemory(m_pWRenderer->m_SwapChain.device, uniformDataVS.buffer, uniformDataVS.memory, 0));

	// Store information in the uniform's descriptor
	uniformDataVS.descriptor.buffer = uniformDataVS.buffer;
	uniformDataVS.descriptor.offset = 0;
	uniformDataVS.descriptor.range = sizeof(m_uboVS);



	//prepare quad
	{
		VkMemoryRequirements memReqs;

		// Vertex shader uniform buffer block
		VkBufferCreateInfo bufferInfo = {};
		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.pNext = NULL;
		allocInfo.allocationSize = 0;
		allocInfo.memoryTypeIndex = 0;

		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = sizeof(quadUniformData);
		bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		// Create a new buffer
		VK_CHECK_RESULT(vkCreateBuffer(m_pWRenderer->m_SwapChain.device, &bufferInfo, nullptr, &uniformData.quad.buffer));
		// Get memory requirements including size, alignment and memory type 
		vkGetBufferMemoryRequirements(m_pWRenderer->m_SwapChain.device, uniformData.quad.buffer, &memReqs);
		allocInfo.allocationSize = memReqs.size;
		// Get the memory type index that supports host visibile memory access
		// Most implementations offer multiple memory tpyes and selecting the 
		// correct one to allocate memory from is important
		// We also want the buffer to be host coherent so we don't have to flush 
		// after every update. 
		// Note that this may affect performance so you might not want to do this 
		// in a real world application that updates buffers on a regular base
		allocInfo.memoryTypeIndex = VkTools::GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_pWRenderer->m_DeviceMemoryProperties);
		// Allocate memory for the uniform buffer
		VK_CHECK_RESULT(vkAllocateMemory(m_pWRenderer->m_SwapChain.device, &allocInfo, nullptr, &(uniformData.quad.memory)));
		// Bind memory to buffer
		VK_CHECK_RESULT(vkBindBufferMemory(m_pWRenderer->m_SwapChain.device, uniformData.quad.buffer, uniformData.quad.memory, 0));

		// Store information in the uniform's descriptor
		uniformData.quad.descriptor.buffer = uniformData.quad.buffer;
		uniformData.quad.descriptor.offset = 0;
		uniformData.quad.descriptor.range = sizeof(quadUniformData);
	}

	//prepare light
	{
		VkMemoryRequirements memReqs;

		// Vertex shader uniform buffer block
		VkBufferCreateInfo bufferInfo = {};
		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.pNext = NULL;
		allocInfo.allocationSize = 0;
		allocInfo.memoryTypeIndex = 0;

		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = sizeof(uboFragmentLights);
		bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		// Create a new buffer
		VK_CHECK_RESULT(vkCreateBuffer(m_pWRenderer->m_SwapChain.device, &bufferInfo, nullptr, &uniformData.fsLights.buffer));
		// Get memory requirements including size, alignment and memory type 
		vkGetBufferMemoryRequirements(m_pWRenderer->m_SwapChain.device, uniformData.fsLights.buffer, &memReqs);
		allocInfo.allocationSize = memReqs.size;
		// Get the memory type index that supports host visibile memory access
		// Most implementations offer multiple memory tpyes and selecting the 
		// correct one to allocate memory from is important
		// We also want the buffer to be host coherent so we don't have to flush 
		// after every update. 
		// Note that this may affect performance so you might not want to do this 
		// in a real world application that updates buffers on a regular base
		allocInfo.memoryTypeIndex = VkTools::GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_pWRenderer->m_DeviceMemoryProperties);
		// Allocate memory for the uniform buffer
		VK_CHECK_RESULT(vkAllocateMemory(m_pWRenderer->m_SwapChain.device, &allocInfo, nullptr, &(uniformData.fsLights.memory)));
		// Bind memory to buffer
		VK_CHECK_RESULT(vkBindBufferMemory(m_pWRenderer->m_SwapChain.device, uniformData.fsLights.buffer, uniformData.fsLights.memory, 0));

		// Store information in the uniform's descriptor
		uniformData.fsLights.descriptor.buffer = uniformData.fsLights.buffer;
		uniformData.fsLights.descriptor.offset = 0;
		uniformData.fsLights.descriptor.range = sizeof(uboFragmentLights);
	}

	//prepare fullscreen
	{
		VkMemoryRequirements memReqs;

		// Vertex shader uniform buffer block
		VkBufferCreateInfo bufferInfo = {};
		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.pNext = NULL;
		allocInfo.allocationSize = 0;
		allocInfo.memoryTypeIndex = 0;

		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = sizeof(uboFullScreen);
		bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		// Create a new buffer
		VK_CHECK_RESULT(vkCreateBuffer(m_pWRenderer->m_SwapChain.device, &bufferInfo, nullptr, &uniformData.vsFullScreen.buffer));
		// Get memory requirements including size, alignment and memory type 
		vkGetBufferMemoryRequirements(m_pWRenderer->m_SwapChain.device, uniformData.vsFullScreen.buffer, &memReqs);
		allocInfo.allocationSize = memReqs.size;
		// Get the memory type index that supports host visibile memory access
		// Most implementations offer multiple memory tpyes and selecting the 
		// correct one to allocate memory from is important
		// We also want the buffer to be host coherent so we don't have to flush 
		// after every update. 
		// Note that this may affect performance so you might not want to do this 
		// in a real world application that updates buffers on a regular base
		allocInfo.memoryTypeIndex = VkTools::GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_pWRenderer->m_DeviceMemoryProperties);
		// Allocate memory for the uniform buffer
		VK_CHECK_RESULT(vkAllocateMemory(m_pWRenderer->m_SwapChain.device, &allocInfo, nullptr, &(uniformData.vsFullScreen.memory)));
		// Bind memory to buffer
		VK_CHECK_RESULT(vkBindBufferMemory(m_pWRenderer->m_SwapChain.device, uniformData.vsFullScreen.buffer, uniformData.vsFullScreen.memory, 0));

		// Store information in the uniform's descriptor
		uniformData.vsFullScreen.descriptor.buffer = uniformData.vsFullScreen.buffer;
		uniformData.vsFullScreen.descriptor.offset = 0;
		uniformData.vsFullScreen.descriptor.range = sizeof(uboFullScreen);
	}

	// Init some values
	m_uboVS.instancePos[0] = glm::vec4(0.0f);
	m_uboVS.instancePos[1] = glm::vec4(-4.0f, 0.0, -4.0f, 0.0f);
	m_uboVS.instancePos[2] = glm::vec4(4.0f, 0.0, -4.0f, 0.0f);

	UpdateUniformBuffers();
	UpdateQuadUniformData();
	UpdateUniformBuffersLights();
}

void Renderer::PrepareVertices(bool useStagingBuffers)
{
	//G-Buffer
	CreateFrameBuffer();

	//Debug Quad
	GenerateQuad();

	//Create font pipeline
	m_VkFont->CreateFontVk((GetAssetPath() + "Textures/freetype/AmazDooMLeft.ttf"), 64, 96);

	m_VkFont->SetupDescriptorPool();
	m_VkFont->SetupDescriptorSetLayout();
	m_VkFont->PrepareUniformBuffers();
	m_VkFont->InitializeChars("qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM-:.@1234567890", *m_pTextureLoader);
	m_VkFont->PrepareResources(g_iDesktopWidth, g_iDesktopHeight);
	BeginTextUpdate();


	
	staticModel.InitFromFile("Geometry/nanosuit/nanosuit2.obj", GetAssetPath());


	std::vector<VkBufferObject_s> listStagingBuffers;
	listLocalBuffers.resize(staticModel.surfaces.size());
	listStagingBuffers.resize(staticModel.surfaces.size());
	listDescriptros.resize(staticModel.surfaces.size());

	m_pWRenderer->m_DescriptorPool = VK_NULL_HANDLE;
	SetupDescriptorPool();
	VkDescriptorSetAllocateInfo allocInfo = VkTools::Initializer::DescriptorSetAllocateInfo(m_pWRenderer->m_DescriptorPool, &descriptorSetLayout, 1);


	// Image descriptor for the shadow map attachment
	VkDescriptorImageInfo GBufferPosition =
		VkTools::Initializer::DescriptorImageInfo(
			colorSampler,
			offScreenFrameBuf.position.view,
			VK_IMAGE_LAYOUT_GENERAL);

	//Normal, Diffuse and Specular packed texture
	VkDescriptorImageInfo GBufferNM =
		VkTools::Initializer::DescriptorImageInfo(
			colorSampler,
			offScreenFrameBuf.nm.view,
			VK_IMAGE_LAYOUT_GENERAL);


	// Debug Descriptor
	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_pWRenderer->m_SwapChain.device, &allocInfo, &quadDescriptorSet));
	std::vector<VkWriteDescriptorSet> quadWriteDescriptorSets =
	{
		// Binding 0 : Vertex shader uniform buffer
		VkTools::Initializer::WriteDescriptorSet(quadDescriptorSet,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,0, &uniformData.quad.descriptor),

		// Binding 1: Image descriptor
		VkTools::Initializer::WriteDescriptorSet(quadDescriptorSet,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1, &GBufferPosition),

		// Binding 2: Image descriptor
		VkTools::Initializer::WriteDescriptorSet(quadDescriptorSet,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2, &GBufferNM),
	};
	vkUpdateDescriptorSets(m_pWRenderer->m_SwapChain.device, quadWriteDescriptorSets.size(), quadWriteDescriptorSets.data(), 0, NULL);

	// FullScreen Descriptor
	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_pWRenderer->m_SwapChain.device, &allocInfo, &defferedModelDescriptorSet));
	std::vector<VkWriteDescriptorSet> defferedWriteModelDescriptorSet =
	{
		// Binding 0 : Vertex shader uniform buffer
		VkTools::Initializer::WriteDescriptorSet(defferedModelDescriptorSet,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,0, &uniformData.vsFullScreen.descriptor),

		// Binding 1: Image descriptor
		VkTools::Initializer::WriteDescriptorSet(defferedModelDescriptorSet,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1, &GBufferPosition),

		// Binding 2: Image descriptor
		VkTools::Initializer::WriteDescriptorSet(defferedModelDescriptorSet,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2, &GBufferNM),

		//Binding 4
		VkTools::Initializer::WriteDescriptorSet(defferedModelDescriptorSet,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4, &uniformData.fsLights.descriptor),
	};
	vkUpdateDescriptorSets(m_pWRenderer->m_SwapChain.device, defferedWriteModelDescriptorSet.size(), defferedWriteModelDescriptorSet.data(), 0, NULL);

	for (int i = 0; i < staticModel.surfaces.size(); i++) 
	{
		//Get triangles
		srfTriangles_t* tr = staticModel.surfaces[i].geometry;


		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_pWRenderer->m_SwapChain.device, &allocInfo, &listDescriptros[i]));
		std::vector<VkWriteDescriptorSet> writeDescriptorSets =
		{
			//uniform descriptor
			VkTools::Initializer::WriteDescriptorSet(listDescriptros[i],VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,	0,&uniformDataVS.descriptor)
		};

		//We need this because descriptorset will take pointer to vkDescriptorImageInfo.
		//So if we delete it before update descriptor sets. The data will not be sented to gpu. Will pointing to memory address in which nothing exist anymore
		//The program won't break. But you will see problems in fragment shader
		std::vector<VkDescriptorImageInfo> descriptors;
		descriptors.reserve(staticModel.surfaces[i].numMaterials);

		//Get all materials
		for (uint32_t j = 0; j < staticModel.surfaces[i].numMaterials; j++)
		{
			VkTools::VulkanTexture* pTexture = staticModel.surfaces[i].material[j].getTexture();
			descriptors.push_back(VkTools::Initializer::DescriptorImageInfo(pTexture->sampler, pTexture->view, VK_IMAGE_LAYOUT_GENERAL));
			writeDescriptorSets.push_back(VkTools::Initializer::WriteDescriptorSet(listDescriptros[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, j + 1, &descriptors[j]));
		}
		//Update descriptor set
		vkUpdateDescriptorSets(m_pWRenderer->m_SwapChain.device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);


		//Create stagging buffer first
		VkBufferObject::CreateBuffer(
			m_pWRenderer->m_SwapChain,
			m_pWRenderer->m_DeviceMemoryProperties,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			static_cast<uint32_t>(sizeof(drawVert) * tr->numVerts),
			listStagingBuffers[i],
			tr->verts);

		//Create Local Copy
		VkBufferObject::CreateBuffer(
			m_pWRenderer->m_SwapChain,
			m_pWRenderer->m_DeviceMemoryProperties,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			static_cast<uint32_t>(sizeof(drawVert) * tr->numVerts),
			listLocalBuffers[i]);


		//Create new command buffer
		VkCommandBuffer copyCmd = GetCommandBuffer(true);


		//Submit info to the queue
		VkBufferObject::SubmitBufferObjects(
			copyCmd,
			m_pWRenderer->m_Queue,
			*m_pWRenderer,
			static_cast<uint32_t>(sizeof(drawVert) * tr->numVerts),
			listStagingBuffers[i],
			listLocalBuffers[i], (drawVertFlags::Vertex | drawVertFlags::Normal | drawVertFlags::Uv | drawVertFlags::Tangent | drawVertFlags::Binormal));

		meshSize.push_back(tr->numVerts);

		numVerts += tr->numVerts;
		numUvs += tr->numVerts;
		numNormals += tr->numVerts;
	}
}


void Renderer::PreparePipeline()
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
		VkTools::Initializer::PipelineInputAssemblyStateCreateInfo(
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			0,
			VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterizationState =
		VkTools::Initializer::PipelineRasterizationStateCreateInfo(
			VK_POLYGON_MODE_FILL,
			VK_CULL_MODE_BACK_BIT,
			VK_FRONT_FACE_CLOCKWISE,
			0);

	VkPipelineColorBlendAttachmentState blendAttachmentState =
		VkTools::Initializer::PipelineColorBlendAttachmentState(
			0xf,
			VK_FALSE);

	VkPipelineColorBlendStateCreateInfo colorBlendState =
		VkTools::Initializer::PipelineColorBlendStateCreateInfo(
			1,
			&blendAttachmentState);

	VkPipelineDepthStencilStateCreateInfo depthStencilState =
		VkTools::Initializer::PipelineDepthStencilStateCreateInfo(
			VK_TRUE,
			VK_TRUE,
			VK_COMPARE_OP_LESS_OR_EQUAL);

	VkPipelineViewportStateCreateInfo viewportState =
		VkTools::Initializer::PipelineViewportStateCreateInfo(1, 1, 0);

	VkPipelineMultisampleStateCreateInfo multisampleState =
		VkTools::Initializer::PipelineMultisampleStateCreateInfo(
			VK_SAMPLE_COUNT_1_BIT,
			0);

	std::vector<VkDynamicState> dynamicStateEnables = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamicState =
		VkTools::Initializer::PipelineDynamicStateCreateInfo(
			dynamicStateEnables.data(),
			static_cast<uint32_t>(dynamicStateEnables.size()),
			0);

	// Load shaders
	//Shaders are loaded from the SPIR-V format, which can be generated from glsl
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
	shaderStages[0] = LoadShader(GetAssetPath() + "Shaders/DeferredShading/DefferedModel.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = LoadShader(GetAssetPath() + "Shaders/DeferredShading/DefferedModel.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);


	VkGraphicsPipelineCreateInfo pipelineCreateInfo =
		VkTools::Initializer::PipelineCreateInfo(
			pipelineLayout,
			m_pWRenderer->m_RenderPass,
			0);

	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();
	pipelineCreateInfo.pVertexInputState = &quadVbo.inputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.renderPass = m_pWRenderer->m_RenderPass;
	pipelineCreateInfo.pDynamicState = &dynamicState;

	// Create rendering pipeline
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_pWRenderer->m_SwapChain.device, m_pWRenderer->m_PipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));

	//Debug Quad Pipeline
	shaderStages[0] = LoadShader(GetAssetPath() + "Shaders/DeferredShading/DebugQuad.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = LoadShader(GetAssetPath() + "Shaders/DeferredShading/DebugQuad.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	pipelineCreateInfo.pVertexInputState = &quadVbo.inputState;

	//Turn off culling
	rasterizationState =
		VkTools::Initializer::PipelineRasterizationStateCreateInfo(
			VK_POLYGON_MODE_FILL,
			VK_CULL_MODE_NONE,
			VK_FRONT_FACE_CLOCKWISE,
			0);

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_pWRenderer->m_SwapChain.device, m_pWRenderer->m_PipelineCache, 1, &pipelineCreateInfo, nullptr, &quadPipeline));

	//G-Buffer
	shaderStages[0] = LoadShader(GetAssetPath() + "Shaders/DeferredShading/DefferedMRT.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = LoadShader(GetAssetPath() + "Shaders/DeferredShading/DefferedMRT.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	// Separate render pass
	pipelineCreateInfo.renderPass = offScreenFrameBuf.renderPass;

	// Separate layout
	pipelineCreateInfo.layout = frameBufferPipelineLayout;

	// Blend attachment states required for all color attachments
	// This is important, as color write mask will otherwise be 0x0 and you
	// won't see anything rendered to the attachment
	std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachmentStates = 
	{
		VkTools::Initializer::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
		VkTools::Initializer::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
		VkTools::Initializer::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
	};
	colorBlendState.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
	colorBlendState.pAttachments = blendAttachmentStates.data();

	pipelineCreateInfo.pVertexInputState = &listLocalBuffers[0].inputState;

	//Turn on culling again
	rasterizationState =
		VkTools::Initializer::PipelineRasterizationStateCreateInfo(
			VK_POLYGON_MODE_FILL,
			VK_CULL_MODE_BACK_BIT,
			VK_FRONT_FACE_CLOCKWISE,
			0);

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_pWRenderer->m_SwapChain.device, m_pWRenderer->m_PipelineCache, 1, &pipelineCreateInfo, nullptr, &frameBufferPipeline));
}



void Renderer::VLoadTexture(std::string fileName, VkFormat format, bool forceLinearTiling, bool bUseCubeMap)
{

}


void Renderer::SetupDescriptorSetLayout()
{
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
	{
		// Binding 0 : Vertex shader uniform buffer
		VkTools::Initializer::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_VERTEX_BIT,0),

		// Binding 1 : Fragment shader image sampler
		VkTools::Initializer::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT,1),

		// Binding 2 : Fragment shader image sampler
		VkTools::Initializer::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT,2),

		// Binding 3 : Fragment shader image sampler
		VkTools::Initializer::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT,3),

		// Binding 4 : Fragment shader uniform buffer
		VkTools::Initializer::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_FRAGMENT_BIT,4),
	};

	VkDescriptorSetLayoutCreateInfo descriptorLayout = VkTools::Initializer::DescriptorSetLayoutCreateInfo(setLayoutBindings.data(), setLayoutBindings.size());
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_pWRenderer->m_SwapChain.device, &descriptorLayout, nullptr, &descriptorSetLayout));

	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = VkTools::Initializer::PipelineLayoutCreateInfo(&descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(m_pWRenderer->m_SwapChain.device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));


	VK_CHECK_RESULT(vkCreatePipelineLayout(m_pWRenderer->m_SwapChain.device, &pPipelineLayoutCreateInfo, nullptr, &frameBufferPipelineLayout));
}

void Renderer::SetupDescriptorPool()
{
	// Example uses one ubo and one image sampler
	std::vector<VkDescriptorPoolSize> poolSizes =
	{
		VkTools::Initializer::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 21),
		VkTools::Initializer::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8*4)
	};

	VkDescriptorPoolCreateInfo descriptorPoolInfo = VkTools::Initializer::DescriptorPoolCreateInfo(poolSizes.size(),poolSizes.data(),9);
	VK_CHECK_RESULT(vkCreateDescriptorPool(m_pWRenderer->m_SwapChain.device, &descriptorPoolInfo, nullptr, &m_pWRenderer->m_DescriptorPool));
}


void Renderer::SetupDescriptorSet()
{
	/*
	VkDescriptorSetAllocateInfo allocInfo = VkTools::Initializer::DescriptorSetAllocateInfo(m_pWRenderer->m_DescriptorPool,&descriptorSetLayout,1);

	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_pWRenderer->m_SwapChain.device, &allocInfo, &descriptorSet));

	// Image descriptor for the color map texture
	VkDescriptorImageInfo texDescriptor = VkTools::Initializer::DescriptorImageInfo(m_VkTexture.sampler, m_VkTexture.view,VK_IMAGE_LAYOUT_GENERAL);

	std::vector<VkWriteDescriptorSet> writeDescriptorSets =
	{
		// Binding 0 : Vertex shader uniform buffer
		VkTools::Initializer::WriteDescriptorSet(descriptorSet,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,	0,&uniformDataVS.descriptor),
	
		// Binding 1 : Fragment shader texture sampler
		VkTools::Initializer::WriteDescriptorSet(descriptorSet,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,&texDescriptor)
	};

	vkUpdateDescriptorSets(m_pWRenderer->m_SwapChain.device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
	*/
}

void Renderer::StartFrame()
{

	{
		// Get next image in the swap chain (back/front buffer)
		VK_CHECK_RESULT(m_pWRenderer->m_SwapChain.GetNextImage(Semaphores.presentComplete, &m_pWRenderer->m_currentBuffer));

		// Submit the post present image barrier to transform the image back to a color attachment
		// that can be used to write to by our render pass
		VkSubmitInfo submitInfo = VkTools::Initializer::SubmitInfo();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_pWRenderer->m_PostPresentCmdBuffers[m_pWRenderer->m_currentBuffer];

		VK_CHECK_RESULT(vkQueueSubmit(m_pWRenderer->m_Queue, 1, &submitInfo, VK_NULL_HANDLE));

		// Make sure that the image barrier command submitted to the queue 
		// has finished executing
		VK_CHECK_RESULT(vkQueueWaitIdle(m_pWRenderer->m_Queue));
	}



	{

		//Submit model
		VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submitInfo = VkTools::Initializer::SubmitInfo();

		//Draw GBuffer
		submitInfo.pWaitDstStageMask = &submitPipelineStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &GBufferScreenCmdBuffer;
		
		
		// Wait for swap chain presentation to finish
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &Semaphores.presentComplete;
		

		//Signal ready for model render to complete
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &GBufferSemaphore;
		VK_CHECK_RESULT(vkQueueSubmit(m_pWRenderer->m_Queue, 1, &submitInfo, VK_NULL_HANDLE));

		//Draw Model
		submitInfo.pWaitSemaphores = &GBufferSemaphore;
		submitInfo.pCommandBuffers = &m_pWRenderer->m_DrawCmdBuffers[m_pWRenderer->m_currentBuffer];
		submitInfo.pSignalSemaphores = &Semaphores.renderComplete;
		VK_CHECK_RESULT(vkQueueSubmit(m_pWRenderer->m_Queue, 1, &submitInfo, VK_NULL_HANDLE));


		//Wait for color output before rendering text
		submitInfo.pWaitDstStageMask = &stageFlags;

		// Wait model rendering to finnish
		submitInfo.pWaitSemaphores = &Semaphores.renderComplete;
		//Signal ready for text to completeS
		submitInfo.pSignalSemaphores = &Semaphores.textOverlayComplete;
		
		submitInfo.pCommandBuffers = &m_VkFont->cmdBuffers[m_pWRenderer->m_currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(m_pWRenderer->m_Queue, 1, &submitInfo, VK_NULL_HANDLE));


		// Reset stage mask
		submitInfo.pWaitDstStageMask = &submitPipelineStages;
		// Reset wait and signal semaphores for rendering next frame
		// Wait for swap chain presentation to finish
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &Semaphores.presentComplete;
		// Signal ready with offscreen semaphore
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &Semaphores.renderComplete;
	}

	{
		// Submit pre present image barrier to transform the image from color attachment to present(khr) for presenting to the swap chain
		VkSubmitInfo submitInfo = VkTools::Initializer::SubmitInfo();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_pWRenderer->m_PrePresentCmdBuffers[m_pWRenderer->m_currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(m_pWRenderer->m_Queue, 1, &submitInfo, VK_NULL_HANDLE));


		// Present the current buffer to the swap chain
		// We pass the signal semaphore from the submit info
		// to ensure that the image is not rendered until
		// all commands have been submitted
		VK_CHECK_RESULT(m_pWRenderer->m_SwapChain.QueuePresent(m_pWRenderer->m_Queue, m_pWRenderer->m_currentBuffer, Semaphores.textOverlayComplete));
	}
}



void Renderer::LoadAssets()
{
	m_VkFont = TYW_NEW VkFont(m_pWRenderer->m_SwapChain.physicalDevice, m_pWRenderer->m_SwapChain.device, m_pWRenderer->m_Queue, m_pWRenderer->m_FrameBuffers, m_pWRenderer->m_SwapChain.colorFormat, m_pWRenderer->m_SwapChain.depthFormat, &m_WindowWidth, &m_WindowHeight);
}


//Main global Renderer
Renderer g_Renderer;

int main()
{
	g_bPrepared = g_Renderer.VInitRenderer(720, 1280, false, HandleWindowMessages);
	
#if defined(_WIN32)
	MSG msg;
#endif

	while (TRUE)
	{
		if (!GenerateEvents(msg))break;

		auto tStart = std::chrono::high_resolution_clock::now();

		//Update once camera is moved
		if (g_Renderer.m_bViewUpdated)
		{
			g_Renderer.m_bViewUpdated = false;
			g_Renderer.UpdateUniformBuffers();
			g_Renderer.UpdateQuadUniformData();
		}

		//Update lights all the time as they move
		g_Renderer.UpdateUniformBuffersLights();

		g_Renderer.StartFrame();
		g_Renderer.EndFrame(nullptr);
		auto tEnd = std::chrono::high_resolution_clock::now();
		
		g_Renderer.frameCounter++;
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		g_Renderer.frameTimer = (float)tDiff / 1000.0f;
		g_Renderer.m_Camera.update(g_Renderer.frameTimer);
		if (g_Renderer.m_Camera.moving())
		{
			g_Renderer.m_bViewUpdated = true;
		}

		g_Renderer.fpsTimer += (float)tDiff;
		if (g_Renderer.fpsTimer > 1000.0f)
		{
			g_Renderer.BeginTextUpdate();
			g_Renderer.lastFPS = g_Renderer.frameCounter;
			g_Renderer.fpsTimer = 0.0f;
			g_Renderer.frameCounter = 0;
		}
	}
	g_Renderer.VShutdown();
	return 0;
}


LRESULT CALLBACK HandleWindowMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		//prepared = false;
		DestroyWindow(hWnd);
		PostQuitMessage(0);
		break;
	case WM_PAINT:
		//ValidateRect(window, NULL);
		//break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case KEY_P:
			//paused = !paused;
			break;
		case KEY_ESCAPE:
			PostQuitMessage(0);
			break;
		}

		if (g_Renderer.m_Camera.firstperson)
		{
			switch (wParam)
			{
			case KEY_W:
				g_Renderer.m_Camera.keys.up = true;
				break;
			case KEY_S:
				g_Renderer.m_Camera.keys.down = true;
				break;
			case KEY_A:
				g_Renderer.m_Camera.keys.left = true;
				break;
			case KEY_D:
				g_Renderer.m_Camera.keys.right = true;
				break;
			}
		}

		//keyPressed((uint32_t)wParam);
		break;
	case WM_KEYUP:
		if (g_Renderer.m_Camera.firstperson)
		{
			switch (wParam)
			{
			case KEY_W:
				g_Renderer.m_Camera.keys.up = false;
				break;
			case KEY_S:
				g_Renderer.m_Camera.keys.down = false;
				break;
			case KEY_A:
				g_Renderer.m_Camera.keys.left = false;
				break;
			case KEY_D:
				g_Renderer.m_Camera.keys.right = false;
				break;
			}
		}
		break;
	case WM_RBUTTONDOWN:
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
		g_MousePos.x = (float)LOWORD(lParam);
		g_MousePos.y = (float)HIWORD(lParam);
		break;
	case WM_MOUSEWHEEL:
	{
		short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		g_zoom += (float)wheelDelta * 0.005f * g_ZoomSpeed;
		g_Renderer.m_Camera.translate(glm::vec3(0.0f, 0.0f, (float)wheelDelta * 0.005f * g_ZoomSpeed));
		g_Renderer.m_bViewUpdated = true;
		break;
	}
	case WM_MOUSEMOVE:
		if (wParam & MK_RBUTTON)
		{
			int32_t posx = LOWORD(lParam);
			int32_t posy = HIWORD(lParam);
			g_zoom += (g_MousePos.y - (float)posy) * .005f * g_ZoomSpeed;
			g_Renderer.m_Camera.translate(glm::vec3(-0.0f, 0.0f, (g_MousePos.y - (float)posy) * .005f * g_ZoomSpeed));
			g_MousePos = glm::vec2((float)posx, (float)posy);
			g_Renderer.m_bViewUpdated = true;
		}
		if (wParam & MK_LBUTTON)
		{
			int32_t posx = LOWORD(lParam);
			int32_t posy = HIWORD(lParam);
			g_Rotation.x += (g_MousePos.y - (float)posy) * 1.25f * g_RotationSpeed;
			g_Rotation.y -= (g_MousePos.x - (float)posx) * 1.25f * g_RotationSpeed;
			g_Renderer.m_Camera.rotate(glm::vec3((g_MousePos.y - (float)posy) * g_Renderer.m_Camera.rotationSpeed, -(g_MousePos.x - (float)posx) * g_Renderer.m_Camera.rotationSpeed, 0.0f));
			g_MousePos = glm::vec2((float)posx, (float)posy);
			g_Renderer.m_bViewUpdated = true;
		}
		if (wParam & MK_MBUTTON)
		{
			int32_t posx = LOWORD(lParam);
			int32_t posy = HIWORD(lParam);
			g_Renderer.m_Camera.translate(glm::vec3(-(g_MousePos.x - (float)posx) * 0.01f, -(g_MousePos.y - (float)posy) * 0.01f, 0.0f));
			g_Renderer.m_bViewUpdated = true;

			g_MousePos.x = (float)posx;
			g_MousePos.y = (float)posy;
		}
		break;
	case WM_SIZE:
		RECT rect;
		if (GetWindowRect(hWnd, &rect))
		{
			WIN_Sizing(wParam, &rect);
		}
		break;
	case WM_EXITSIZEMOVE:
		if (g_bPrepared) 
		{
			g_Renderer.VWindowResize(g_iDesktopHeight, g_iDesktopWidth);
			g_Renderer.m_VkFont->UpdateUniformBuffers(g_iDesktopWidth, g_iDesktopHeight, g_zoom);
		}
		break;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


bool GenerateEvents(MSG& msg)
{
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			return false;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return true;
}


void WIN_Sizing(WORD side, RECT *rect)
{
	// restrict to a standard aspect ratio
	g_iDesktopWidth = rect->right - rect->left;
	g_iDesktopHeight = rect->bottom - rect->top;

	// Adjust width/height for window decoration
	RECT decoRect = { 0, 0, 0, 0 };
	AdjustWindowRect(&decoRect, WINDOW_STYLE | WS_SYSMENU, FALSE);
	uint32_t decoWidth = decoRect.right - decoRect.left;
	uint32_t decoHeight = decoRect.bottom - decoRect.top;

	g_iDesktopWidth -= decoWidth;
	g_iDesktopHeight -= decoHeight;
}