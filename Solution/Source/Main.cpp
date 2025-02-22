/*
 * Model loader application, by Gustavo Zille.
 */

#include "Includes.h"
#include "Custom/Model.h"

struct UniformBlock
{
	CameraMatrix mProjectView;

	vec4 mLightPosition;
	vec4 mLightColor;
};

// But we only need two sets of resources (one in flight and one being used on CPU).
const uint32_t gDataBufferCount = 2;

Renderer* pRenderer = NULL;
Queue* pGraphicsQueue = NULL;
GpuCmdRing gGraphicsCmdRing = {};
SwapChain* pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Semaphore* pImageAcquiredSemaphore = NULL;

Custom::Model gModel;
VertexLayout gModelVertexLayout = {};
Shader* pModelShader = NULL;
Pipeline* pModelPipeline = NULL;
RootSignature* pRootSignature = NULL;

UniformBlock gUniformData;
Buffer* pUniformBuffer[gDataBufferCount] = { NULL };
DescriptorSet* pDescriptorSetUniforms = { NULL };

uint32_t gFrameIndex = 0;

ICameraController* pCameraController = NULL;

UIComponent* pGuiWindow = NULL;

const char* gWindowTestScripts[] = { "TestFullScreen.lua", "TestCenteredWindow.lua", "TestNonCenteredWindow.lua", "TestBorderless.lua" };

void reloadRequest(void*)
{
	ReloadDesc reload{ RELOAD_TYPE_SHADER };

	requestReload(&reload);
}

class ModelLoaderApp: public IApp
{
public:
	bool Init()
	{
		// Window and renderer setup.
		RendererDesc settings;
		memset(&settings, 0, sizeof(settings));
		initGPUConfiguration(settings.pExtendedSettings);
		initRenderer(GetName(), &settings, &pRenderer);

		// Check for init success.
		if (!pRenderer)
		{
			ShowUnsupportedMessage("Failed to initialize renderer!");

			return false;
		}

		setupGPUConfigurationPlatformParameters(pRenderer, settings.pExtendedSettings);

		QueueDesc queueDesc = {};
		queueDesc.mType = QUEUE_TYPE_GRAPHICS;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		initQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		GpuCmdRingDesc cmdRingDesc = {};
		cmdRingDesc.pQueue = pGraphicsQueue;
		cmdRingDesc.mPoolCount = gDataBufferCount;
		cmdRingDesc.mCmdPerPoolCount = 1;
		cmdRingDesc.mAddSyncPrimitives = true;
		initGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

		initSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		// Load custom model.
		// gModel.init("../../Art/Meshes/FBX/Castle.fbx");

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;

		for (uint32_t i = 0; i < gDataBufferCount; ++i)
		{
			ubDesc.mDesc.pName = "UniformBuffer";
			ubDesc.mDesc.mSize = sizeof(UniformBlock);
			ubDesc.ppBuffer = &pUniformBuffer[i];
			addResource(&ubDesc, NULL);
		}

		// Initialize Forge UI rendering.
		UserInterfaceDesc uiRenderDesc = {};
		uiRenderDesc.pRenderer = pRenderer;
		initUserInterface(&uiRenderDesc);

		const uint32_t numScripts = TF_ARRAY_COUNT(gWindowTestScripts);
		LuaScriptDesc  scriptDescs[numScripts] = {};

		for (uint32_t i = 0; i < numScripts; ++i)
		{
			scriptDescs[i].pScriptFileName = gWindowTestScripts[i];
		}

		DEFINE_LUA_SCRIPTS(scriptDescs, numScripts);

		waitForAllResourceLoads();

		vec3 camPos{ 64.0f, 48.0f, 64.0f };
		vec3 camLookAt{ vec3(0.0f) };
		CameraMotionParameters camMotionParams{ 150.0f, 500.0f, 200.0f };

		pCameraController = initFpsCameraController(camPos, camLookAt);
		pCameraController->setMotionParameters(camMotionParams);

		AddCustomInputBindings();

		initScreenshotInterface(pRenderer, pGraphicsQueue);

		gFrameIndex = 0;

		return true;
	}

	void Exit()
	{
		exitScreenshotInterface();

		exitCameraController(pCameraController);

		exitUserInterface();

		for (uint32_t i = 0; i < gDataBufferCount; ++i)
		{
			removeResource(pUniformBuffer[i]);
		}

		gModel.exit();

		exitGpuCmdRing(pRenderer, &gGraphicsCmdRing);
		exitSemaphore(pRenderer, pImageAcquiredSemaphore);

		exitResourceLoaderInterface(pRenderer);

		exitQueue(pRenderer, pGraphicsQueue);

		exitRenderer(pRenderer);
		exitGPUConfiguration();

		pRenderer = NULL;
	}

	bool Load(ReloadDesc* pReloadDesc)
	{
		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			addShaders();
			addRootSignatures();
			addDescriptorSets();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
		{
			UIComponentDesc guiDesc = {};
			guiDesc.mStartPosition = vec2(0.0f, 8.0f);
			uiAddComponent(GetName(), &guiDesc, &pGuiWindow);

			if (!addSwapChain())
			{
				return false;
			}

			if (!addDepthBuffer())
			{
				return false;
			}
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			generateLayouts();
			addPipelines();
		}

		prepareDescriptorSets();

		UserInterfaceLoadDesc uiLoad = {};
		uiLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
		uiLoad.mHeight = mSettings.mHeight;
		uiLoad.mWidth = mSettings.mWidth;
		uiLoad.mLoadType = pReloadDesc->mType;
		loadUserInterface(&uiLoad);

		return true;
	}

	void Unload(ReloadDesc* pReloadDesc)
	{
		waitQueueIdle(pGraphicsQueue);

		unloadUserInterface(pReloadDesc->mType);

		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			removePipelines();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
		{
			removeSwapChain(pRenderer, pSwapChain);
			removeRenderTarget(pRenderer, pDepthBuffer);
			uiRemoveComponent(pGuiWindow);
		}

		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			removeDescriptorSets();
			removeRootSignatures();
			removeShaders();
		}
	}

	void Update(float deltaTime)
	{
		if (!uiIsFocused())
		{
			pCameraController->onMove({ inputGetValue(0, CUSTOM_MOVE_X), inputGetValue(0, CUSTOM_MOVE_Y) });
			pCameraController->onRotate({ inputGetValue(0, CUSTOM_LOOK_X), inputGetValue(0, CUSTOM_LOOK_Y) });
			pCameraController->onMoveY(inputGetValue(0, CUSTOM_MOVE_UP));
			
			if (inputGetValue(0, CUSTOM_RESET_VIEW))
			{
				pCameraController->resetView();
			}
			
			if (inputGetValue(0, CUSTOM_TOGGLE_FULLSCREEN))
			{
				toggleFullscreen(pWindow);
			}
			
			if (inputGetValue(0, CUSTOM_TOGGLE_UI))
			{
				uiToggleActive();
			}
			
			if (inputGetValue(0, CUSTOM_EXIT))
			{
				requestShutdown();
			}
		}

		pCameraController->update(deltaTime);

		// Update scene.
		static float currentTime = 0.0f;
		currentTime += deltaTime * 1000.0f;

		// Update camera with time.
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontalFov = PI / 2.0f;
		CameraMatrix projMat = CameraMatrix::perspectiveReverseZ(horizontalFov, aspectInverse, 0.1f, 1000.0f);
		mat4 viewMat = pCameraController->getViewMatrix();
		
		gUniformData.mProjectView = projMat * viewMat;

		// Set point light parameters.
		gUniformData.mLightPosition = vec4(0.0f, 0.0f, 0.0f, 0.0f);
		gUniformData.mLightColor = vec4(0.9f, 0.9f, 0.7f, 1.0f); // Set a pale-yellow color.
	}

	void Draw()
	{
		if ((bool)pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}

		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
		GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);

		// Stall if CPU is running "gDataBufferCount" frames ahead of GPU.
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, elem.pFence, &fenceStatus);

		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
		{
			waitForFences(pRenderer, 1, &elem.pFence);
		}

		// Update uniform buffers.
		BufferUpdateDesc uniformUnpdateDesc = { pUniformBuffer[gFrameIndex] };
		beginUpdateResource(&uniformUnpdateDesc);
		memcpy(uniformUnpdateDesc.pMappedData, &gUniformData, sizeof(gUniformData));
		endUpdateResource(&uniformUnpdateDesc);

		// Reset cmd pool for this frame.
		resetCmdPool(pRenderer, elem.pCmdPool);

		Cmd* cmd = elem.pCmds[0];
		beginCmd(cmd);

		RenderTargetBarrier barriers[] = {
			{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		// Simply record the screen cleaning command.
		BindRenderTargetsDesc bindRenderTargets = {};
		bindRenderTargets.mRenderTargetCount = 1;
		bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_CLEAR };
		bindRenderTargets.mDepthStencil = { pDepthBuffer, LOAD_ACTION_CLEAR };
		cmdBindRenderTargets(cmd, &bindRenderTargets);
		
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		// Draw model.
		cmdBindPipeline(cmd, pModelPipeline);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetUniforms);
		gModel.draw(cmd);

		cmdBindRenderTargets(cmd, NULL);

		bindRenderTargets = {};
		bindRenderTargets.mRenderTargetCount = 1;
		bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_LOAD };
		bindRenderTargets.mDepthStencil = { NULL, LOAD_ACTION_DONTCARE };
		cmdBindRenderTargets(cmd, &bindRenderTargets);

		cmdDrawUserInterface(cmd);

		cmdBindRenderTargets(cmd, NULL);

		barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		endCmd(cmd);

		FlushResourceUpdateDesc flushUpdateDesc = {};
		flushUpdateDesc.mNodeIndex = 0;
		flushResourceUpdates(&flushUpdateDesc);
		
		Semaphore* waitSemaphores[2] = { flushUpdateDesc.pOutSubmittedSemaphore, pImageAcquiredSemaphore };

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.mSignalSemaphoreCount = 1;
		submitDesc.mWaitSemaphoreCount = TF_ARRAY_COUNT(waitSemaphores);
		submitDesc.ppCmds = &cmd;
		submitDesc.ppSignalSemaphores = &elem.pSemaphore;
		submitDesc.ppWaitSemaphores = waitSemaphores;
		submitDesc.pSignalFence = elem.pFence;
		queueSubmit(pGraphicsQueue, &submitDesc);

		QueuePresentDesc presentDesc = {};
		presentDesc.mIndex = (uint8_t)swapchainImageIndex;
		presentDesc.mWaitSemaphoreCount = 1;
		presentDesc.pSwapChain = pSwapChain;
		presentDesc.ppWaitSemaphores = &elem.pSemaphore;
		presentDesc.mSubmitDone = true;
		queuePresent(pGraphicsQueue, &presentDesc);

		gFrameIndex = (gFrameIndex + 1) % gDataBufferCount;
	}

	const char* GetName() { return "Model Loader"; }

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.mWindowHandle = pWindow->handle;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle);
		swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, COLOR_SPACE_SDR_SRGB);
		swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;
		swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
		swapChainDesc.mFlags = SWAP_CHAIN_CREATION_FLAG_ENABLE_FOVEATED_RENDERING_VR;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	bool addDepthBuffer()
	{
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue.depth = 0.0f;
		depthRT.mClearValue.stencil = 0;
		depthRT.mDepth = 1;
		depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
		depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		depthRT.mFlags = TEXTURE_CREATION_FLAG_ON_TILE | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}

	void addDescriptorSets()
	{
		DescriptorSetDesc desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount * 2 };
		addDescriptorSet(pRenderer, &desc, &pDescriptorSetUniforms);
	}

	void removeDescriptorSets()
	{
		removeDescriptorSet(pRenderer, pDescriptorSetUniforms);
	}

	void prepareDescriptorSets()
	{
		for (uint32_t i = 0; i < gDataBufferCount; ++i)
		{
			DescriptorData uParams[1] = {};
			uParams[0].pName = "uniformBlock";
			uParams[0].ppBuffers = &pUniformBuffer[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetUniforms, 1, uParams);
		}
	}

	void addRootSignatures()
	{
		uint32_t shadersCount = 1;
		Shader* shaders[1] = { pModelShader };

		RootSignatureDesc rootDesc = {};
		rootDesc.mShaderCount = shadersCount;
		rootDesc.ppShaders = shaders;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);
	}

	void removeRootSignatures()
	{
		removeRootSignature(pRenderer, pRootSignature);
	}

	void addShaders()
	{
		ShaderLoadDesc drawModelShader = {};
		drawModelShader.mVert.pFileName = "DrawModel.vert";
		drawModelShader.mFrag.pFileName = "DrawModel.frag";
		addShader(pRenderer, &drawModelShader, &pModelShader);
	}

	void removeShaders()
	{
		removeShader(pRenderer, pModelShader);
	}

	void generateLayouts()
	{
		// Model vertex layout.
		gModelVertexLayout.mBindingCount = 1;
		gModelVertexLayout.mBindings[0].mStride = sizeof(Custom::Vertex);

		gModelVertexLayout.mAttribCount = 3;

		// Position (vec3).
		gModelVertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		gModelVertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gModelVertexLayout.mAttribs[0].mBinding = 0;
		gModelVertexLayout.mAttribs[0].mLocation = 0;
		gModelVertexLayout.mAttribs[0].mOffset = offsetof(Custom::Vertex, mPosition);

		// Normal (vec3).
		gModelVertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		gModelVertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gModelVertexLayout.mAttribs[1].mBinding = 0;
		gModelVertexLayout.mAttribs[1].mLocation = 1;
		gModelVertexLayout.mAttribs[1].mOffset = offsetof(Custom::Vertex, mNormal);

		// UV (vec2).
		gModelVertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		gModelVertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
		gModelVertexLayout.mAttribs[2].mBinding = 0;
		gModelVertexLayout.mAttribs[2].mLocation = 2;
		gModelVertexLayout.mAttribs[2].mOffset = offsetof(Custom::Vertex, mUV);
	}

	void addPipelines()
	{
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_GEQUAL;

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE; // CULL_MODE_FRONT;

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = &depthStateDesc;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pModelShader;
		pipelineSettings.pVertexLayout = &gModelVertexLayout;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		pipelineSettings.mVRFoveatedRendering = true;
		addPipeline(pRenderer, &desc, &pModelPipeline);
	}

	void removePipelines()
	{
		removePipeline(pRenderer, pModelPipeline);
	}
};

DEFINE_APPLICATION_MAIN(ModelLoaderApp)
