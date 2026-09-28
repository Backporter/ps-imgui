#include <imgui.h>

#ifndef IMGUI_DISABLE
#include "imgui_impl_PlayStation_Gnm.h"

#include <stdint.h>				// xxxxx_t
#include <string.h>				// std::memset
#include <array>				// std::array
#include <vector>				// std::vector
#include <shader.h>				//
#include <gnm.h>				//  
#include <gnmx.h>				//  
#include <gnm\sampler.h>		//
#include <gnm\texture.h>		//
#include <gnmx/shader_parser.h> //
#include <mspace.h>             //
#include <pad.h>				//  
#include <mouse.h>				//  
#include <libime.h>				// 

//
#include "imgui_libfont_PlayStation.h"   // <- 
#include "Shader/ImGui_shader_common.h"
#pragma comment(lib, "libSceShaderBinary.a")

#ifndef TILE_TEXTURE
// tiled textures perform the best so you should be tiling them
#define TILE_TEXTURE true
#endif

namespace
{
	struct vertexBufferDescriptor
	{
		const char*	  m_semanticName;
		const uint8_t m_semanticIndex;
	};

	struct ShaderBase
	{
		size_t			m_sourceSize;
		const uint32_t* m_source;
	};

	struct FetchShaderBase
	{
		void*	 m_fetchShader;
		void*	 m_remapTable;
		uint32_t m_shaderModifier;
	};

	struct EmbeddedVsFetchShader :
		public ShaderBase,
		public FetchShaderBase
	{
		sce::Gnmx::VsShader*		    m_shader;
		sce::Gnmx::InputOffsetsCache    m_offsetsTable;
		sce::Gnmx::InputResourceOffsets m_resourcesOffsets;

		void initializeWithAllocators(ImGui_Allocators& allocators)
		{
			sce::Gnmx::ShaderInfo shaderInfo;
			sce::Gnmx::parseShader(&shaderInfo, m_source);
			
			//
			void* shaderBinary = allocators.garlic.allocate(shaderInfo.m_gpuShaderCodeSize, sce::Gnm::kAlignmentOfShaderInBytes);
			void* shaderHeader = allocators.onion.allocate(shaderInfo.m_vsShader->computeSize(), sce::Gnm::kAlignmentOfBufferInBytes);
			
			//
			memcpy(shaderBinary, shaderInfo.m_gpuShaderCode, shaderInfo.m_gpuShaderCodeSize);
			memcpy(shaderHeader, shaderInfo.m_vsShader, shaderInfo.m_vsShader->computeSize());
			
			//
			m_shader = static_cast<sce::Gnmx::VsShader*>(shaderHeader);
			m_shader->patchShaderGpuAddress(shaderBinary);
			
			//
			sce::Gnmx::generateInputResourceOffsetTable(&m_resourcesOffsets, sce::Gnm::ShaderStage::kShaderStageVs, m_shader);
			sce::Gnmx::generateInputOffsetsCache(&m_offsetsTable, sce::Gnm::kShaderStageVs, m_shader);
		}
	};

	struct EmbeddedPsShader :
		public ShaderBase
	{
		sce::Gnmx::PsShader*		     m_shader;
		sce::Gnmx::InputOffsetsCache     m_offsetsTable;
		sce::Gnmx::InputResourceOffsets  m_resourcesOffsets;

		void initializeWithAllocators(ImGui_Allocators& allocators)
		{
			sce::Gnmx::ShaderInfo shaderInfo;
			sce::Gnmx::parseShader(&shaderInfo, m_source);

			//
			void *shaderBinary = allocators.garlic.allocate(shaderInfo.m_gpuShaderCodeSize, sce::Gnm::kAlignmentOfShaderInBytes);
			void *shaderHeader = allocators.onion.allocate(shaderInfo.m_psShader->computeSize(), sce::Gnm::kAlignmentOfBufferInBytes);

			//
			memcpy(shaderBinary, shaderInfo.m_gpuShaderCode, shaderInfo.m_gpuShaderCodeSize);
			memcpy(shaderHeader, shaderInfo.m_psShader, shaderInfo.m_psShader->computeSize());

			//
			m_shader = static_cast<sce::Gnmx::PsShader*>(shaderHeader);
			m_shader->patchShaderGpuAddress(shaderBinary);

			//
			sce::Gnmx::generateInputResourceOffsetTable(&m_resourcesOffsets, sce::Gnm::ShaderStage::kShaderStagePs, m_shader);
			sce::Gnmx::generateInputOffsetsCache(&m_offsetsTable, sce::Gnm::ShaderStage::kShaderStagePs, m_shader);
		}
	};

	class MouseProcessor
	{
	public:
		void SetDisplaySize(float w, float h)
		{
			displayW = w;
			displayH = h;
			absX = displayW * 0.5f;
			absY = displayH * 0.5f;
		}

		// --- Toggles ---
		void ToggleSensitivity(bool v) { useSensitivity = v; }
		void ToggleAcceleration(bool v) { useAcceleration = v; }
		void ToggleSmoothing(bool v) { useSmoothing = v; }
		void ToggleScaling(bool v) { useScaling = v; }

		// --- Parameters ---
		void SetSensitivity(float s) { sensitivity = s; }
		void SetAcceleration(float a) { accelFactor = a; }
		void SetSmoothing(float s) { smoothing = s; }

		float GetSensitivity() { return sensitivity; }
		bool GetUseAcceleration() const { return useAcceleration; }

		void ProcessDelta(float dx, float dy)
		{
			// if (enabled)
			{
				if (useSensitivity)
				{
					ApplySensitivity(dx, dy);
				}

				if (useAcceleration)
				{
					ApplyAcceleration(dx, dy);
				}

				if (useScaling)
				{
					ApplyScaling(dx, dy);
				}

				if (useSmoothing)
				{
					ApplySmoothing(dx, dy);
				}
			}

			Integrate(dx, dy);

			// if (enabled)
			{
				ClampToScreen();
			}
		}

		float X() const { return absX; }
		float Y() const { return absY; }
	private:
		void ApplySensitivity(float& dx, float& dy)
		{
			dx *= sensitivity;
			dy *= sensitivity;
		}

		void ApplyAcceleration(float& dx, float& dy)
		{
			float speed = sqrtf(dx * dx + dy * dy);
			float accel = 1.0f + speed * accelFactor;
			dx *= accel;
			dy *= accel;
		}

		void ApplyScaling(float& dx, float& dy)
		{
			float scale = displayW / 1920.0f;
			if (scale < 1.0f)
			{
				scale = 1.0f;
			}

			dx *= scale;
			dy *= scale;
		}

		void ApplySmoothing(float& dx, float& dy)
		{
			smoothedDx = Lerp(smoothedDx, dx, smoothing);
			smoothedDy = Lerp(smoothedDy, dy, smoothing);
			dx = smoothedDx;
			dy = smoothedDy;
		}

		void Integrate(float dx, float dy)
		{
			absX += dx;
			absY += dy;
		}

		void ClampToScreen()
		{
			absX = Clamp(absX, 0.0f, displayW - 1.0f);
			absY = Clamp(absY, 0.0f, displayH - 1.0f);
		}

		static float Lerp(float a, float b, float t)
		{
			return a + (b - a) * t;
		}

		static float Clamp(float v, float lo, float hi)
		{
			return v < lo ? lo : (v > hi ? hi : v);
		}

		// --- State ---
		float absX = 0.0f;
		float absY = 0.0f;
		float smoothedDx = 0.0f;
		float smoothedDy = 0.0f;
		float displayW = 1920.0f;
		float displayH = 1080.0f;
		float sensitivity = 1.5f;
		float accelFactor = 0.0f;
		float smoothing = 1.0f;

		// Toggles
		// bool enabled = false;
		bool useSensitivity = true;
		bool useAcceleration = false;
		bool useScaling = false;
		bool useSmoothing = false;
	};

	static inline void generateVertexInputRemapTable(const sce::Shader::Binary::Program* vsp, const vertexBufferDescriptor *vertexBufferDescs, uint32_t numVertexBufferDescs, uint32_t *remapTable)
	{
		SCE_GNM_ASSERT(vsp);
		SCE_GNM_ASSERT(vertexBufferDescs);
		SCE_GNM_ASSERT(numVertexBufferDescs);
		SCE_GNM_ASSERT(remapTable);

		// loop through all vertex input attributes and check if each attribute has corresponding vertex buffer descriptor
		for (uint8_t i = 0; i < vsp->m_numInputAttributes; ++i)
		{
			sce::Shader::Binary::Attribute* attrib = vsp->m_inputAttributes + i;
			sce::Shader::Binary::PsslSemantic psslSemantic = (sce::Shader::Binary::PsslSemantic)attrib->m_psslSemantic;

			//
			fprintf(stdout, "Semantic Name -> %s", (const char*)attrib->getSemanticName());

			// An input with system semantic does not need a slot in the remap table; ignore it
			if (psslSemantic != sce::Shader::Binary::kSemanticUserDefined)
				// Any semantic other than user defined ones are system
				continue;

			// look for the corresponding vertex buffer semantic name
			bool found = false;
			for (uint32_t j = 0; j < numVertexBufferDescs; j++)
			{
				if (strcmp((const char*)attrib->getSemanticName(), vertexBufferDescs[j].m_semanticName) == 0 &&
					attrib->m_semanticIndex == vertexBufferDescs[j].m_semanticIndex)
				{
					found = true;
					break;
				}
			}

			// error if there is no verterx buffer descriptor corresponding to this input attribute
			if (!found)
			{
				fprintf(stdout, "Error: vertex shader input semantic %s%d was not found in the vertex buffer semantic name list\n", (const char*)attrib->getSemanticName(), attrib->m_semanticIndex);
				printf("Error: vertex shader input semantic %s%d was not found in the vertex buffer semantic name list\n", (const char*)attrib->getSemanticName(), attrib->m_semanticIndex);
				SCE_GNM_ERROR("Error: vertex shader input semantic %s%d was not found in the vertex buffer semantic name list\n", (const char*)attrib->getSemanticName(), attrib->m_semanticIndex);
				exit(1);
			}
		}

		// loop through all vertex buffer descriptors and fill out remap table entries
		for (uint32_t i = 0; i < numVertexBufferDescs; i++)
		{
			sce::Shader::Binary::Attribute *inputAttribute = vsp->getInputAttributeBySemanticNameAndIndex(
				vertexBufferDescs[i].m_semanticName,
				vertexBufferDescs[i].m_semanticIndex);

			if (inputAttribute)
				remapTable[i] = inputAttribute->m_resourceIndex;
			else
				remapTable[i] = 0xFFFFFFFF; // unused
		}
	}

	static inline uint32_t getStartRegister(const sce::Gnm::InputUsageSlot *inputUsageSlot, uint32_t inputUsageSlotCount, sce::Gnm::ShaderInputUsageType usageType, uint32_t apiSlot)
	{
		for (uint32_t slot = 0; slot < inputUsageSlotCount; ++slot)
		{
			const sce::Gnm::InputUsageSlot *ius = inputUsageSlot + slot;
			if (ius->m_apiSlot == apiSlot && ius->m_usageType == usageType)
				return ius->m_startRegister;
		}

		return 0xFFFFFFFF;
	}

	static inline void BuildFetchShader(ImGui_Allocators& allocators, EmbeddedVsFetchShader& vertexShader)
	{
		sce::Shader::Binary::Program vsProgram;
		auto res = vsProgram.loadFromMemory(vertexShader.m_source, vertexShader.m_sourceSize);
		if (res != sce::Shader::Binary::PsslStatus::kStatusOk)
		{
			fprintf(stdout, "Failed to read shader");
		}

		const vertexBufferDescriptor vertexBufferDescs[] =
		{
			{ "POSITION", 0 },
			{ "TEXCOORD", 0 },
			{ "COLOR",	 0 },
		};

		enum
		{
			kVertexBufferDescs = sizeof(vertexBufferDescs) / sizeof(vertexBufferDescriptor)
		};

		vertexShader.m_remapTable = allocators.onion.allocate(kVertexBufferDescs * sizeof(uint32_t), 4);
		std::memset((uint32_t*)vertexShader.m_remapTable, -1, sizeof(uint32_t) * kVertexBufferDescs);
		generateVertexInputRemapTable(&vsProgram, vertexBufferDescs, kVertexBufferDescs, (uint32_t*)vertexShader.m_remapTable);
		uint32_t fetchShaderSize = sce::Gnmx::computeVsFetchShaderSize(vertexShader.m_shader);
		vertexShader.m_fetchShader = allocators.garlic.allocate(fetchShaderSize, sce::Gnm::kAlignmentOfFetchShaderInBytes);
		sce::Gnmx::generateVsFetchShader(vertexShader.m_fetchShader, &vertexShader.m_shaderModifier, vertexShader.m_shader, nullptr, vertexShader.m_remapTable, kVertexBufferDescs);
	}

	[[maybe_unused]] static inline sce::Gnm::SizeAlign gnmTextureInitLinear2d(sce::Gnm::Texture* gnmTexture, int32_t w, int32_t h)
	{
		sce::Gnm::TextureSpec textureSpec;
		int32_t ret;

		textureSpec.init();
		textureSpec.m_width = w;
		textureSpec.m_height = h;
		textureSpec.m_depth = 1;
		textureSpec.m_pitch = 0;
		textureSpec.m_numMipLevels = 1;
		textureSpec.m_numSlices = 1;
		textureSpec.m_format = sce::Gnm::kDataFormatR8G8B8A8Unorm;
		textureSpec.m_textureType = sce::Gnm::kTextureType2d;
		textureSpec.m_minGpuMode = sce::Gnm::kGpuModeBase;
		textureSpec.m_tileModeHint = sce::Gnm::kTileModeDisplay_LinearAligned;
		textureSpec.m_numFragments = sce::Gnm::kNumFragments1;

		ret = gnmTexture->init(&textureSpec);
		if (ret != SCE_GNM_OK)
		{
			return sce::Gnm::SizeAlign(0, 0);
		}

		return gnmTexture->getSizeAlign();
	}

	[[maybe_unused]] static inline sce::Gnm::SizeAlign gnmTextureInit2d(sce::Gnm::Texture* gnmTexture, sce::Gnm::DataFormat format, int32_t w, int32_t h)
	{
		sce::Gnm::TextureSpec textureSpec;
		int32_t ret;

		textureSpec.init();
		textureSpec.m_format = format;
		textureSpec.m_width = w;
		textureSpec.m_height = h;
		textureSpec.m_depth = 1;
		textureSpec.m_pitch = 0;
		textureSpec.m_numMipLevels = 1;
		textureSpec.m_numSlices = 1;
		textureSpec.m_textureType = sce::Gnm::kTextureType2d;
		textureSpec.m_minGpuMode = sce::Gnm::getGpuMode();;
		textureSpec.m_tileModeHint = sce::Gnm::kTileModeDisplay_2dThin;
		textureSpec.m_numFragments = sce::Gnm::kNumFragments1;

		ret = gnmTexture->init(&textureSpec);
		if (ret != SCE_GNM_OK)
		{
			return sce::Gnm::SizeAlign(0, 0);
		}

		return gnmTexture->getSizeAlign();
	}
}

static sce::Gnm::IndexSize indexSize = []()
{
	sce::Gnm::IndexSize indexSize;
	if constexpr (sizeof(ImDrawIdx) == 2)
	{
		indexSize = sce::Gnm::IndexSize::kIndexSize16;
	}
	else if constexpr (sizeof(ImDrawIdx) == 4)
	{
		indexSize = sce::Gnm::IndexSize::kIndexSize32;
	}
	else
	{
		static_assert(sizeof(ImDrawIdx) == 2 || sizeof(ImDrawIdx) == 4, "Unsupported ImDrawIdx size: only 16-bit or 32-bit indices are supported.");
	}

	return indexSize;
}();

static sce::Gnm::DataFormat indexFormat = []()
{
	return (indexSize == sce::Gnm::IndexSize::kIndexSize16) ? sce::Gnm::kDataFormatR16Uint : sce::Gnm::kDataFormatR32Uint;
}();

struct ImGui_ImplPlayStation_RenderBuffers
{
	void*  pConstantBuffer = nullptr;
	void*  pIndexBuffer = nullptr;
	void*  pVertexBuffer = nullptr;
	void*  pTableBuffer = nullptr;

	uint32_t vertexCapacity = 0;
	uint32_t indexCapacity = 0;
};

struct ImGui_ImplPlayStation_MemorySpace
{
	void*		   MspaceHeapBacking = nullptr;
	SceLibcMspace  MspaceHandle = nullptr;
};

struct ImGui_ImplPlayStation_PlatformUserData
{
	// IO
	bool                          PollInput = false;
	ImGui_InputBase<SceMouseData> MouseInput;
	ImGui_InputBase<ScePadData>   GamepadInput;
	ImGui_InputBase<SceImeEvent>  KeyboardInput;

	// IO Helper:
	MouseProcessor                 mouseProcessor;
	
	// Garlic / Onion Allocators:
	ImGui_Allocators               allocators;

	// Libc Memory Space:
	ImGui_ImplPlayStation_MemorySpace fontMemorySpace;
	ImGui_ImplPlayStation_MemorySpace memorySpace;
};

struct ImGui_ImplPlayStation_RendererUserData
{
	ImGui_ImplPlayStation_RenderInfoData info;

	//
	static constexpr int kBufferCount = 3;
	ImGui_ImplPlayStation_RenderBuffers frameResources[kBufferCount];
	uint32_t activeBufferId = 0;


	// Shaders:
	EmbeddedVsFetchShader               vertexShader{};
	EmbeddedPsShader                    pixelShader{};
	
	// Time Info:
	int64_t                             Time = 0;
	int64_t                             TicksPerSecond = 0;
};

static inline ImGui_ImplPlayStation_PlatformUserData* ImGui_ImplPlayStation_GetBackendUserData() 
{
	return ImGui::GetCurrentContext() ? (ImGui_ImplPlayStation_PlatformUserData*)ImGui::GetIO().BackendPlatformUserData : nullptr; 
}

static inline ImGui_ImplPlayStation_RendererUserData* ImGui_ImplPlayStation_GetBackendRenderUserData() 
{
	return ImGui::GetCurrentContext() ? (ImGui_ImplPlayStation_RendererUserData*)ImGui::GetIO().BackendRendererUserData : nullptr; 
}

[[maybe_unused]] void* ImGui_PlayStation_MemAllocFunc(size_t sz, void*)
{
	//
	IM_ASSERT(ImGui_ImplPlayStation_GetBackendUserData() != nullptr && "ImGui_ImplPlayStation_Init() not called");

	//
	auto* backendUserData = ImGui_ImplPlayStation_GetBackendUserData();
	auto res = sceLibcMspaceMalloc(backendUserData->memorySpace.MspaceHandle, sz);
	if (res == nullptr)
	{
		fprintf(stdout, "sceLibcMspaceMalloc() failed");
	}

	return res;
}

[[maybe_unused]] void ImGui_PlayStation_MemFreeFunc(void* ptr, void*)
{
	//
	IM_ASSERT(ImGui_ImplPlayStation_GetBackendUserData() != nullptr && "ImGui_ImplPlayStation_Init() not called");

	//
	auto* backendUserData = ImGui_ImplPlayStation_GetBackendUserData();
	auto res = sceLibcMspaceFree(backendUserData->memorySpace.MspaceHandle, ptr);
	if (res != 0)
	{
		fprintf(stdout, "sceLibcMspaceFree() failed for 0x%x", res);
	}
}

static bool ImGui_ImplPlayStation_InitEx(ImGui_InitUserData& a_args, unsigned int width, unsigned int height)
{
#if _DEBUG
	PRINT_POS;
#endif

	auto* platformUserData = IM_NEW(ImGui_ImplPlayStation_PlatformUserData)();
	auto* renderUserData = IM_NEW(ImGui_ImplPlayStation_RendererUserData)();

	//
	renderUserData->TicksPerSecond = sceKernelGetProcessTimeCounterFrequency();
	renderUserData->Time = sceKernelGetProcessTimeCounter();

	//
	platformUserData->allocators = a_args.allocators;
	platformUserData->MouseInput = a_args.MouseInput;
	platformUserData->GamepadInput = a_args.GamePadInput;
	platformUserData->KeyboardInput = a_args.KeyboardInput;
	platformUserData->mouseProcessor.SetDisplaySize(static_cast<float>(width), static_cast<float>(height));

	//
	if (a_args.EnableMouseSensitivity)
	{
		platformUserData->mouseProcessor.SetSensitivity(a_args.MouseSensitivity);
	}

	//
	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(io.BackendPlatformUserData == nullptr && "Backend's PlatformsUserData Already initialized!");
	IM_ASSERT(io.BackendRendererUserData == nullptr && "Backend's RenderUserData Already initialized!");
	io.BackendPlatformUserData = (void*)platformUserData;
	io.BackendRendererUserData = (void*)renderUserData;

	//
	io.BackendPlatformName = "Orbis";
	io.BackendRendererName = "Gnm/x";
	io.BackendFlags = (ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_HasGamepad);
	io.ConfigFlags = (ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad /* | ImGuiConfigFlags_IsSRGB */);
	io.MouseDrawCursor = true;
	io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));

	//
#if _DEBUG
	fprintf(stdout, "Display(W/H): [%d, %d]", width, height);
#endif

	// setup pixel shader
	{
		static const unsigned int s_pixelShader[/* bytes = 488 */] =
		{
			#include "Shader\ImGui_ps_p.h"
		};

		renderUserData->pixelShader.m_source = (const uint32_t*)s_pixelShader;
		renderUserData->pixelShader.m_sourceSize = sizeof(s_pixelShader) * sizeof(unsigned int);
		renderUserData->pixelShader.initializeWithAllocators(platformUserData->allocators);
	}

	// setup vertex shader
	{
		static const unsigned int s_vertexShader[/* bytes = 632 */] =
		{
			#include "Shader\ImGui_vs_vv.h"
		};

		renderUserData->vertexShader.m_source = (const uint32_t*)s_vertexShader;
		renderUserData->vertexShader.m_sourceSize = sizeof(s_vertexShader) * sizeof(unsigned int);
		renderUserData->vertexShader.initializeWithAllocators(platformUserData->allocators);

#if _DEBUG
		fprintf(stdout, "VS numInputSemantics: %u", bd->vertexShader.m_shader->m_numInputSemantics);
		for (uint32_t i = 0; i < bd->vertexShader.m_shader->m_numInputSemantics; ++i)
		{
			const auto& s = bd->vertexShader.m_shader->getInputSemanticTable()[i];
			fprintf(stdout, "input[%u]: semantic=%u vgpr=%u size=%u", i, s.m_semantic, s.m_vgpr, s.m_sizeInElements);
		}
#endif

		// gnm/x requires a fetch shader for the vertex shader
		BuildFetchShader(platformUserData->allocators, renderUserData->vertexShader);
	}

	// create fonts atlas.
	{
		sce::Gnm::SizeAlign sizeAlign = { 128 * 1024 * 1024, 8 }; // 128 MiB
		platformUserData->fontMemorySpace.MspaceHeapBacking = platformUserData->allocators.onion.allocate(sizeAlign.m_size, sizeAlign.m_align);
		platformUserData->fontMemorySpace.MspaceHandle = sceLibcMspaceCreate("ImGuiFontSpace", platformUserData->fontMemorySpace.MspaceHeapBacking, sizeAlign.m_size, 0);

		//
		if (platformUserData->fontMemorySpace.MspaceHandle == nullptr)
		{
			fprintf(stdout, "Failed to allocate font memory");
		}

		ImFont* defaultFont = ImGuiLibFont::AddSystemFont(io.Fonts, 12.0f * 1.0f);
		if (defaultFont == nullptr)
		{
			fprintf(stdout, "failed to add default system font.");
		}

		int ret = ImGuiLibFont::Initialize();
		if (ret != 0)
		{
			fprintf(stdout, "failed to initialize a imgui font atlas.");
		}

		//
		ImGuiLibFont::BuildFontAtlas(io.Fonts, platformUserData->fontMemorySpace.MspaceHandle);
	}

	// create fonts texture.
	{
		unsigned char* pixels;
		int32_t width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

		//
		PlayStationImage* psImage = platformUserData->allocators.onion.allocate<PlayStationImage>();
		if (psImage == nullptr)
		{
			fprintf(stdout, "failed to allocate PlayStation Texture Container");
			return false;
		}

		//
		psImage->texture = platformUserData->allocators.garlic.allocate<sce::Gnm::Texture>();
		if (psImage->texture == nullptr)
		{
			fprintf(stdout, "failed to allocate PlayStation Texture");
			return false;
		}

		psImage->sampler = platformUserData->allocators.garlic.allocate<sce::Gnm::Sampler>();
		if (psImage->sampler == nullptr)
		{
			fprintf(stdout, "failed to sampler PlayStation Texture");
			return false;
		}

		// setup sampler
		auto fontSampler = psImage->sampler;
		fontSampler->init();
		fontSampler->setXyFilterMode(sce::Gnm::FilterMode::kFilterModeBilinear, sce::Gnm::FilterMode::kFilterModeBilinear);

		//
#if TILE_TEXTURE
		auto sizeAlign = gnmTextureInit2d(psImage->texture, sce::Gnm::kDataFormatR8G8B8A8Unorm, width, height);
#else
		auto sizeAlign = gnmTextureInitLinear2d(psImage->texture, width, height);
#endif

		//
		auto texData = platformUserData->allocators.garlic.allocate(sizeAlign.m_size, sizeAlign.m_align);
		if (texData == nullptr)
		{
			fprintf(stdout, "failed to allocate garlic memory for texture");
			return false;
		}

		//
		psImage->texture->setBaseAddress(texData);
		psImage->texture->setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);
		
		//
#if TILE_TEXTURE
		sce::GpuAddress::TilingParameters tilingParameters{};
		if (tilingParameters.initFromTexture(psImage->texture, 0, 0) != 0)
		{
			fprintf(stdout, "Failed to init TilingParameters from texture.");
			return false;
		}

		if (sce::GpuAddress::tileSurface(texData, pixels, &tilingParameters) != 0)
		{
			fprintf(stdout, "Failed to tile surface texture.");
			return false;
		}
#else
		std::copy_n(pixels, 4 * width * height, (unsigned char*)texData);
#endif

		//
		io.Fonts->TexID = psImage->TextureID();
	}

#if _DEBUG
	PRINT_POS;
#endif

	return true;
}

IMGUI_IMPL_API bool ImGui_ImplPlayStation_Init(ImGui_InitUserData& a_args, unsigned int width, unsigned int height)
{
	if (a_args.allocators.garlic._instance == nullptr || a_args.allocators.garlic._allocate == nullptr || a_args.allocators.garlic._free == nullptr || a_args.allocators.onion._instance == nullptr || a_args.allocators.onion._allocate == nullptr || a_args.allocators.onion._free == nullptr)
	{
		throw std::exception("ImGui requires a allocator and none was set");
	}
	
	//
	return ImGui_ImplPlayStation_InitEx(a_args, width, height);
}

IMGUI_IMPL_API void ImGui_ImplPlayStation_Shutdown()
{
	//
	auto* backendUserData = ImGui_ImplPlayStation_GetBackendUserData();
	IM_ASSERT(backendUserData != nullptr && "No platform backend to shutdown, or already shutdown?");

	//
	auto* backendRenderUserData = ImGui_ImplPlayStation_GetBackendRenderUserData();
	IM_ASSERT(backendRenderUserData != nullptr && "No platform backend to shutdown, or already shutdown?");

	//
	ImGuiIO& io = ImGui::GetIO();
	io.BackendPlatformName = nullptr;
	io.BackendPlatformUserData = nullptr;
	io.BackendFlags &= ~(ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos | ImGuiBackendFlags_HasGamepad);

	//
	IM_DELETE(backendUserData);
	IM_DELETE(backendRenderUserData);

}

IMGUI_IMPL_API void ImGui_ImplPlayStation_NewFrame()
{
	auto* backendUserData = ImGui_ImplPlayStation_GetBackendUserData();
	IM_ASSERT(backendUserData != nullptr && "Context or backend not initialized? Did you call ImGui_ImplPlayStation_Init()?");
	
	auto* backendRenderUserData = ImGui_ImplPlayStation_GetBackendRenderUserData();
	IM_ASSERT(backendRenderUserData != nullptr && "Context or backend not initialized? Did you call ImGui_ImplPlayStation_Init()?");

	//
	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(io.Fonts->IsBuilt());

	// Setup time step
	int64_t current_time = sceKernelGetProcessTimeCounter();
	io.DeltaTime = (float)(current_time - backendRenderUserData->Time) / backendRenderUserData->TicksPerSecond;
	backendRenderUserData->Time = current_time;

	// Rotate to the next frame-resource slot.
	//
	// Each slot remains alive for kBufferCount frames, allowing the GPU
	// to continue consuming the previous frame's buffers while we write
	// into the current slot.
	backendRenderUserData->activeBufferId = (backendRenderUserData->activeBufferId + 1) % ImGui_ImplPlayStation_RendererUserData::kBufferCount;

	if (backendUserData->PollInput)
	{
		// process mouse
		if (backendUserData->MouseInput) // only Poll if the pointer is valid.
		{
			backendUserData->MouseInput.Poll();

			for (int i = 0; i < backendUserData->MouseInput.count(); ++i)
			{
				auto& m = backendUserData->MouseInput.data()[i];
				backendUserData->mouseProcessor.ProcessDelta((float)m.xAxis, (float)m.yAxis);
				io.AddMousePosEvent(backendUserData->mouseProcessor.X(), backendUserData->mouseProcessor.Y());
				io.AddMouseButtonEvent(ImGuiMouseButton_Left, m.buttons & SCE_MOUSE_BUTTON_PRIMARY);
				io.AddMouseButtonEvent(ImGuiMouseButton_Right, m.buttons & SCE_MOUSE_BUTTON_SECONDARY);
				io.AddMouseButtonEvent(ImGuiMouseButton_Middle, m.buttons & SCE_MOUSE_BUTTON_OPTIONAL);
				if (m.wheel != 0)
				{
					io.AddMouseWheelEvent(0.0f, (float)m.wheel);
				}
			}
		}

		// process gamepad as a gamepad
		if (backendUserData->GamepadInput)
		{
			// 
			ScePadControllerInformation controllerInformation{};
			auto infoValid = backendUserData->GamepadInput.PollInfo((void*)&controllerInformation);

			backendUserData->GamepadInput.Poll();
			for (int i = 0; i < backendUserData->GamepadInput.count(); ++i)
			{
				auto& padData = backendUserData->GamepadInput.data()[i];
				if (padData.connected == false || (padData.buttons & SCE_PAD_BUTTON_INTERCEPTED)) // ignore intercepted / disconnected events
				{
					continue;
				}

#define IM_SATURATE(V) (V < 0.0f ? 0.0f : V > 1.0f ? 1.0f : V)
#define MAP_BUTTON(KEY_NO, BUTTON_ENUM)                                \
		{                                                              \
			io.AddKeyEvent(KEY_NO, (padData.buttons & BUTTON_ENUM) != 0); \
		}
#define MAP_ANALOG(KEY_NO, VALUE, V0, V1)                              \
		{                                                              \
			float vn = (float)(VALUE - V0) / (float)(V1 - V0);         \
			io.AddKeyAnalogEvent(KEY_NO, vn > 0.10f, IM_SATURATE(vn)); \
		}

				MAP_BUTTON(ImGuiKey_GamepadStart, SCE_PAD_BUTTON_OPTIONS);
				MAP_BUTTON(ImGuiKey_GamepadFaceLeft, SCE_PAD_BUTTON_SQUARE);
				MAP_BUTTON(ImGuiKey_GamepadFaceRight, SCE_PAD_BUTTON_CIRCLE);
				MAP_BUTTON(ImGuiKey_GamepadFaceUp, SCE_PAD_BUTTON_TRIANGLE);
				MAP_BUTTON(ImGuiKey_GamepadFaceDown, SCE_PAD_BUTTON_CROSS);
				MAP_BUTTON(ImGuiKey_GamepadDpadLeft, SCE_PAD_BUTTON_LEFT);
				MAP_BUTTON(ImGuiKey_GamepadDpadRight, SCE_PAD_BUTTON_RIGHT);
				MAP_BUTTON(ImGuiKey_GamepadDpadUp, SCE_PAD_BUTTON_UP);
				MAP_BUTTON(ImGuiKey_GamepadDpadDown, SCE_PAD_BUTTON_DOWN);
				MAP_BUTTON(ImGuiKey_GamepadL1, SCE_PAD_BUTTON_L1);
				MAP_BUTTON(ImGuiKey_GamepadR1, SCE_PAD_BUTTON_R1);
				MAP_BUTTON(ImGuiKey_GamepadL3, SCE_PAD_BUTTON_L3);
				MAP_BUTTON(ImGuiKey_GamepadR3, SCE_PAD_BUTTON_R3);
				MAP_ANALOG(ImGuiKey_GamepadL2, padData.analogButtons.l2, 0, 255);
				MAP_ANALOG(ImGuiKey_GamepadR2, padData.analogButtons.r2, 0, 255);

				// L Stick
				auto leftDeadZone = infoValid ? controllerInformation.stickInfo.deadZoneLeft : 0;
				MAP_ANALOG(ImGuiKey_GamepadLStickLeft, padData.leftStick.x, 128 - leftDeadZone, 0);
				MAP_ANALOG(ImGuiKey_GamepadLStickRight, padData.leftStick.x, 128 + leftDeadZone, 255);
				MAP_ANALOG(ImGuiKey_GamepadLStickUp, padData.leftStick.y, 128 + leftDeadZone, 255);
				MAP_ANALOG(ImGuiKey_GamepadLStickDown, padData.leftStick.y, 128 - leftDeadZone, 0);

				// R Stick
				auto rightDeadZone = infoValid ? controllerInformation.stickInfo.deadZoneRight : 0;
				MAP_ANALOG(ImGuiKey_GamepadRStickLeft, padData.rightStick.x, 128 - rightDeadZone, 0);
				MAP_ANALOG(ImGuiKey_GamepadRStickRight, padData.rightStick.x, 128 + rightDeadZone, 255);
				MAP_ANALOG(ImGuiKey_GamepadRStickUp, padData.rightStick.y, 128 + rightDeadZone, 255);
				MAP_ANALOG(ImGuiKey_GamepadRStickDown, padData.rightStick.y, 128 - rightDeadZone, 0);

#undef MAP_BUTTON
#undef MAP_ANALOG
			}
		}

		// if we have no mouse sink then we can treat these things in the controller as a phsudo mouse using the touchpad and left joystick / right joystck
		if (backendUserData->MouseInput == false && backendUserData->GamepadInput != false)
		{
			ScePadControllerInformation controllerInformation{};
			if (backendUserData->GamepadInput.PollInfo((void**)&controllerInformation))
			{
				backendUserData->GamepadInput.Poll();
				for (int i = 0; i < backendUserData->GamepadInput.count(); ++i)
				{
					//
					auto& padData = backendUserData->GamepadInput.data()[i];
					if (padData.connected == false || (padData.buttons & SCE_PAD_BUTTON_INTERCEPTED)) // ignore intercepted / disconnected events
					{
						continue;
					}

					// 1: TouchPad
					if (controllerInformation.touchPadInfo.resolution.x > 0)
					{
						const auto& touchInfo = controllerInformation.touchPadInfo;
						if (padData.touchData.touchNum > 0)
						{
							const auto& t = padData.touchData.touch[0];

							float nx = (float)t.x / (float)touchInfo.resolution.x;
							float ny = (float)t.y / (float)touchInfo.resolution.y;

							nx = nx < 0.f ? 0.f : (nx > 1.f ? 1.f : nx);
							ny = ny < 0.f ? 0.f : (ny > 1.f ? 1.f : ny);

							float mx = nx * io.DisplaySize.x;
							float my = ny * io.DisplaySize.y;

							io.AddMousePosEvent(mx, my);
						}
					}

					// Touchpad Click / X Button
					io.AddMouseButtonEvent(ImGuiMouseButton_Left, ((padData.buttons & SCE_PAD_BUTTON_TOUCH_PAD) != 0 || (padData.buttons & SCE_PAD_BUTTON_CROSS) != 0));

					// --- 2(1). Left stick as relative mouse ---
					{
						int center = 128;
						int maxV = 255;

						float dx = (float)(padData.leftStick.x - center);
						float dy = (float)(padData.leftStick.y - center);

						if (fabsf(dx) < controllerInformation.stickInfo.deadZoneLeft)
							dx = 0.0f;

						if (fabsf(dy) < controllerInformation.stickInfo.deadZoneLeft)
							dy = 0.0f;

						float nx = dx / (float)(maxV - center);
						float ny = dy / (float)(maxV - center);

						const float stickScale = 12.0f;
						float mdx = nx * stickScale;
						float mdy = ny * stickScale;

						if (mdx != 0.0f || mdy != 0.0f)
						{
							backendUserData->mouseProcessor.ProcessDelta(mdx, mdy);
							io.AddMousePosEvent(backendUserData->mouseProcessor.X(), backendUserData->mouseProcessor.Y());
						}
					}

					// --- 2(2). Right stick as relative mouse ---
					{
						int center = 128;
						int maxV = 255;

						float dx = (float)(padData.rightStick.x - center);
						float dy = (float)(padData.rightStick.y - center);

						if (fabsf(dx) < controllerInformation.stickInfo.deadZoneRight)
							dx = 0.0f;

						if (fabsf(dy) < controllerInformation.stickInfo.deadZoneRight)
							dy = 0.0f;

						float nx = dx / (float)(maxV - center);
						float ny = dy / (float)(maxV - center);

						const float stickScale = 12.0f;
						float mdx = nx * stickScale;
						float mdy = ny * stickScale;

						if (mdx != 0.0f || mdy != 0.0f)
						{
							backendUserData->mouseProcessor.ProcessDelta(mdx, mdy);
							io.AddMousePosEvent(backendUserData->mouseProcessor.X(), backendUserData->mouseProcessor.Y());
						}
					}

					// --- 3. L2/R2 as mouse buttons ---
					io.AddMouseButtonEvent(ImGuiMouseButton_Right, padData.analogButtons.r2 > 20);

					// --- 4. D‑pad vertical as mouse wheel ---
					if (padData.buttons & SCE_PAD_BUTTON_UP)
						io.AddMouseWheelEvent(0.0f, +1.0f);

					if (padData.buttons & SCE_PAD_BUTTON_DOWN)
						io.AddMouseWheelEvent(0.0f, -1.0f);
				}
			}
		}
	}
}

/*
	-- Draw using a DrawCommandBuffer
*/
/*__noinline*/ IMGUI_IMPL_API void ImGui_ImplPlayStation_RenderDrawData(ImGuiDrawCommandBuffer& dcb, ImDrawData* draw_data)
{
	[[maybe_unused]] ImGuiIO& io = ImGui::GetIO();

	auto* backendUserData = ImGui_ImplPlayStation_GetBackendUserData();
	IM_ASSERT(backendUserData != nullptr && "ImGui_ImplPlayStation_Init() not called");

	auto* backendRenderUserData = ImGui_ImplPlayStation_GetBackendRenderUserData();
	IM_ASSERT(backendRenderUserData != nullptr && "ImGui_ImplPlayStation_Init() not called");

#if _DEBUG
	PRINT_POS;
	fprintf(stdout, "TotalVtxCount: %d", draw_data->TotalVtxCount);
	fprintf(stdout, "TotalIdxCount: %d", draw_data->TotalIdxCount);
	fprintf(stdout, "DisplaySize.x: %f, DisplaySize.y: %f", draw_data->DisplaySize.x, draw_data->DisplaySize.y);
#else
	backendRenderUserData->info.totalVtx = draw_data->TotalVtxCount;
	backendRenderUserData->info.totalIdx = draw_data->TotalIdxCount;
	backendRenderUserData->info.totalCmdLists = draw_data->CmdListsCount;
	backendRenderUserData->info.totalCmds = 0;
#endif

	//
	if (draw_data->TotalVtxCount == 0 || draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
	{
		return;
	}

	//
	auto& frameResources = backendRenderUserData->frameResources[backendRenderUserData->activeBufferId];

	// Ensure the current frame's vertex buffer is large enough.
	if (frameResources.pVertexBuffer == nullptr || frameResources.vertexCapacity < static_cast<uint32_t>(draw_data->TotalVtxCount))
	{
		if (frameResources.pVertexBuffer)
		{
			backendUserData->allocators.garlic.free(frameResources.pVertexBuffer);
		}

		frameResources.vertexCapacity = static_cast<uint32_t>(draw_data->TotalVtxCount);
		frameResources.pVertexBuffer = (void*)backendUserData->allocators.garlic.allocate<ImDrawVert>(frameResources.vertexCapacity);
	}

	// Ensure the current frame's index buffer is large enough.
	if (frameResources.pIndexBuffer == nullptr || frameResources.indexCapacity < static_cast<uint32_t>(draw_data->TotalIdxCount))
	{
		if (frameResources.pIndexBuffer)
		{
			backendUserData->allocators.garlic.free(frameResources.pIndexBuffer);
		}

		frameResources.indexCapacity = static_cast<uint32_t>(draw_data->TotalIdxCount);
		frameResources.pIndexBuffer = (void*)backendUserData->allocators.garlic.allocate<ImDrawIdx>(frameResources.indexCapacity);
	}

	// Constant buffer is always exactly one matrix.
	if (frameResources.pConstantBuffer == nullptr)
	{
		frameResources.pConstantBuffer = (void*)backendUserData->allocators.garlic.allocate<float4x4>(1);
	}

	//
	if (frameResources.pTableBuffer == nullptr)
	{
		frameResources.pTableBuffer = (void*)backendUserData->allocators.garlic.allocate<sce::Gnm::Buffer>(3);
	}

	//
	[[maybe_unused]] void* cBufferData = frameResources.pConstantBuffer;
	[[maybe_unused]] void* vertexData = frameResources.pVertexBuffer;
	[[maybe_unused]] void* indexData = frameResources.pIndexBuffer;

	// Upload vertex/index data into a single contiguous GPU buffer
	{
		uint32_t	vtx_offset = 0;
		ImDrawVert* vtx_dst = static_cast<ImDrawVert*>(vertexData);
		ImDrawIdx*	idx_dst = static_cast<ImDrawIdx*>(indexData);

		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
			for (int i = 0; i < cmd_list->IdxBuffer.Size; i++)
			{
				idx_dst[i] = cmd_list->IdxBuffer[i] + vtx_offset;
			}

			vtx_dst += cmd_list->VtxBuffer.Size;
			idx_dst += cmd_list->IdxBuffer.Size;
			vtx_offset += cmd_list->VtxBuffer.Size;
		}
	}

	// Build projection matrix
	{
		float L = draw_data->DisplayPos.x;
		float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
		float T = draw_data->DisplayPos.y;
		float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
		float orthoProjMatrix[4][4] =
		{
			{ 2.0f / (R - L),	 0.0f,				0.0f, 0.0f },
			{ 0.0f,				 2.0f / (T - B),    0.0f, 0.0f },
			{ 0.0f,				 0.0f,				1.0f, 0.0f },
			{ (R + L) / (L - R), (T + B) / (B - T), 0.0f, 1.0f },
		};

		std::memcpy(cBufferData, orthoProjMatrix, sizeof(orthoProjMatrix));
	}


	// Setup Index/vertexBuffer/constantBuffer structures
	sce::Gnm::Buffer indexBuffer, constantBuffer, *vbTable = (sce::Gnm::Buffer*)frameResources.pTableBuffer;
	indexBuffer.initAsDataBuffer(indexData, indexFormat, draw_data->TotalIdxCount);
	constantBuffer.initAsConstantBuffer(cBufferData, sizeof(float4x4));
	vbTable[0].initAsVertexBuffer((char*)vertexData + offsetof(VS_INPUT, position),	sce::Gnm::kDataFormatR32G32Float, sizeof(ImDrawVert), draw_data->TotalVtxCount);   // POSITION
	vbTable[1].initAsVertexBuffer((char*)vertexData + offsetof(VS_INPUT, uv), sce::Gnm::kDataFormatR32G32Float, sizeof(ImDrawVert), draw_data->TotalVtxCount);         // TEXCOORD
	vbTable[2].initAsVertexBuffer((char*)vertexData + offsetof(VS_INPUT, color), sce::Gnm::kDataFormatR8G8B8A8Unorm, sizeof(ImDrawVert), draw_data->TotalVtxCount);    // COLOR
	indexBuffer.setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);
	constantBuffer.setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);
	vbTable[0].setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);
	vbTable[1].setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);
	vbTable[2].setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);

	// Setup Pipline stuff
	sce::Gnm::BlendControl blendState;
	blendState.init();
	blendState.setBlendEnable(true);
	blendState.setSeparateAlphaEnable(true);
	blendState.setAlphaEquation(
		sce::Gnm::BlendMultiplier::kBlendMultiplierOne,
		sce::Gnm::BlendFunc::kBlendFuncAdd,
		sce::Gnm::BlendMultiplier::kBlendMultiplierOneMinusSrcAlpha
	);

	blendState.setColorEquation(
		sce::Gnm::BlendMultiplier::kBlendMultiplierSrcAlpha,
		sce::Gnm::BlendFunc::kBlendFuncAdd,
		sce::Gnm::BlendMultiplier::kBlendMultiplierOneMinusSrcAlpha
	);

	sce::Gnm::PrimitiveSetup primitiveSetup;
	primitiveSetup.init();
	primitiveSetup.setCullFace(sce::Gnm::PrimitiveSetupCullFaceMode::kPrimitiveSetupCullFaceNone);
	primitiveSetup.setPolygonMode(
		sce::Gnm::PrimitiveSetupPolygonMode::kPrimitiveSetupPolygonModeFill,
		sce::Gnm::PrimitiveSetupPolygonMode::kPrimitiveSetupPolygonModeFill
	);

	// setup depth state
	sce::Gnm::DepthStencilControl depthState;
	depthState.init();
	depthState.setDepthEnable(false);
	depthState.setDepthControl(sce::Gnm::DepthControlZWrite::kDepthControlZWriteDisable, sce::Gnm::CompareFunc::kCompareFuncAlways);
	depthState.setDepthBoundsEnable(false);

// AI
	auto vs = backendRenderUserData->vertexShader.m_shader;
	auto ps = backendRenderUserData->pixelShader.m_shader;
	const auto* vsSlots = vs->getInputUsageSlotTable();
	const auto* psSlots = ps->getInputUsageSlotTable();
	uint32_t regFS = getStartRegister(vsSlots, vs->m_common.m_numInputUsageSlots, sce::Gnm::kShaderInputUsageSubPtrFetchShader, 0);
	uint32_t regVB = getStartRegister(vsSlots, vs->m_common.m_numInputUsageSlots, sce::Gnm::kShaderInputUsagePtrVertexBufferTable, 0);
	uint32_t regCB = getStartRegister(vsSlots, vs->m_common.m_numInputUsageSlots, sce::Gnm::kShaderInputUsageImmConstBuffer, 0);
	uint32_t regTex = getStartRegister(psSlots, ps->m_common.m_numInputUsageSlots, sce::Gnm::kShaderInputUsageImmResource, 0);
	uint32_t regSamp = getStartRegister(psSlots, ps->m_common.m_numInputUsageSlots, sce::Gnm::kShaderInputUsageImmSampler, 0);

	//
#if _DEBUG
	fprintf(stdout, "regFS: %d", regFS);
	fprintf(stdout, "regVB: %d", regVB);
	fprintf(stdout, "regCB: %d", regCB);
	fprintf(stdout, "regTex: %d", regTex);
	fprintf(stdout, "regSamp: %d", regSamp);
#endif

#if _DEBUG
#endif

	//
	dcb.pushMarker("ImGui");

	//
	if (regFS != 0xFFFFFFFF)
		dcb.setPointerInUserData(sce::Gnm::ShaderStage::kShaderStageVs, regFS, backendRenderUserData->vertexShader.m_fetchShader);
	if (regVB != 0xFFFFFFFF)
		dcb.setPointerInUserData(sce::Gnm::ShaderStage::kShaderStageVs, regVB, vbTable);
	if (regCB != 0xFFFFFFFF)
		dcb.setVsharpInUserData(sce::Gnm::ShaderStage::kShaderStageVs, regCB, &constantBuffer);

	uint32_t psInputs[32];
	sce::Gnm::generatePsShaderUsageTable(psInputs, vs->getExportSemanticTable(), vs->m_numExportSemantics, ps->getPixelInputSemanticTable(), ps->m_numInputSemantics);
	dcb.setPsShaderUsage(psInputs, ps->m_numInputSemantics);
// AI

	// Bind pipeline
	dcb.setBlendControl(0, blendState);
	dcb.setPrimitiveSetup(primitiveSetup);
	dcb.setDepthStencilControl(depthState);
	dcb.setActiveShaderStages(sce::Gnm::ActiveShaderStages::kActiveShaderStagesVsPs);
	dcb.setPrimitiveType(sce::Gnm::PrimitiveType::kPrimitiveTypeTriList);
	dcb.setNumInstances(1);
	dcb.setVsShader(&backendRenderUserData->vertexShader.m_shader->m_vsStageRegisters, 0);
	dcb.setPsShader(&backendRenderUserData->pixelShader.m_shader->m_psStageRegisters);
	dcb.setIndexSize(indexSize, sce::Gnm::kCachePolicyBypass);
	dcb.setIndexCount(draw_data->TotalIdxCount);
	dcb.setIndexBuffer(indexBuffer.getBaseAddress());
	sce::Gnmx::setupScreenViewport(&dcb, 0, 0, draw_data->DisplaySize.x, draw_data->DisplaySize.y, 0.5f, 0.5f);

#if _DEBUG
	PRINT_FMT_VA
	(
		"setupScreenViewport:\n"
		"left -> %d\n"
		"top -> %d\n"
		"right -> %f\n"
		"bottom -> %f\n"
		"zScale -> %f\n"
		"zOffset -> %f", 
		0, 
		0, 
		draw_data->DisplaySize.x, 
		draw_data->DisplaySize.y, 
		0.5f, 
		0.5f
	);


	fprintf(stdout, "TotalIdxCount: %d", draw_data->TotalIdxCount);
	fprintf(stdout, "vertexData:   %p", vertexData);
	fprintf(stdout, "indexData:    %p", indexData);
	fprintf(stdout, "indexBase:    %p", indexBuffer.getBaseAddress());
	fprintf(stdout, "vbTable:      %p", vbTable);
	fprintf(stdout, "cbData:       %p", cBufferData);
	fprintf(stdout, "fontTexBase:  %p", FontAtlasShaderData.texture.getBaseAddress());
	fprintf(stdout, "fetchShader:  %p", backendRenderUserData->vertexShader.m_fetchShader);
#endif

	// Draw
	uint32_t indexBufferOffset = 0;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];

		backendRenderUserData->info.totalCmds += cmd_list->CmdBuffer.Size;

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

			// Project scissor/clipping rectangles into framebuffer space
			ImVec2 clip_min(pcmd->ClipRect.x - draw_data->DisplayPos.x, pcmd->ClipRect.y - draw_data->DisplayPos.y);
			ImVec2 clip_max(pcmd->ClipRect.z - draw_data->DisplayPos.x, pcmd->ClipRect.w - draw_data->DisplayPos.y);
			if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
				continue;
#if _DEBUG
			PRINT_FMT_VA
			(
				"setScreenScissor:\n"
				"left -> %d\n"
				"top -> %d\n"
				"right -> %d\n"
				"bottom -> %d\n",
				(int)clip_min.x,
				(int)clip_min.y,
				(int)clip_max.x,
				(int)clip_max.y
			);
#endif

			PlayStationImage* psImage = (PlayStationImage*)pcmd->GetTexID();

			//
			if (regTex != 0xFFFFFFFF)
			{
				if (psImage)
				{
					dcb.setTsharpInUserData(sce::Gnm::ShaderStage::kShaderStagePs, regTex, psImage->texture);
				}
			}

			if (regSamp != 0xFFFFFFFF)
			{
				if (psImage)
				{
					dcb.setSsharpInUserData(sce::Gnm::ShaderStage::kShaderStagePs, regSamp, psImage->sampler);
				}
			}

			//
			const uint32_t startIndex = indexBufferOffset + pcmd->IdxOffset;
			dcb.setScreenScissor((int)clip_min.x, (int)clip_min.y, (int)clip_max.x, (int)clip_max.y);
			dcb.drawIndexOffset(startIndex, pcmd->ElemCount);
		}

		indexBufferOffset += cmd_list->IdxBuffer.size();
	}

	//
	dcb.popMarker();

#if _DEBUG
#else
	backendRenderUserData->info.deltaTime = ImGui::GetIO().DeltaTime;
	backendRenderUserData->info.frameCount++;
#endif

	//
#if _DEBUG
	PRINT_POS;
#endif
}

/*
	-- Draw uisng a graphics context (event with a graphics context you can just call ImGui_ImplPlayStation_RenderDrawData(&gfxCtx.m_dcb, ..) and it should work)
*/
/*__noinline*/ IMGUI_IMPL_API void ImGui_ImplPlayStation_RenderDrawData(ImGuiGfxContext& gfxCtx, ImDrawData* draw_data)
{
	auto* backendUserData = ImGui_ImplPlayStation_GetBackendUserData();
	IM_ASSERT(backendUserData != nullptr && "ImGui_ImplPlayStation_Init() not called");

	auto* backendRenderUserData = ImGui_ImplPlayStation_GetBackendRenderUserData();
	IM_ASSERT(backendRenderUserData != nullptr && "ImGui_ImplPlayStation_Init() not called");
	
#if _DEBUG
	PRINT_POS;
	fprintf(stdout, "TotalVtxCount: %d", draw_data->TotalVtxCount);
	fprintf(stdout, "TotalIdxCount: %d", draw_data->TotalIdxCount);
	fprintf(stdout, "DisplaySize.x: %f, DisplaySize.y: %f", draw_data->DisplaySize.x, draw_data->DisplaySize.y);
#else
	backendRenderUserData->info.totalVtx = draw_data->TotalVtxCount;
	backendRenderUserData->info.totalIdx = draw_data->TotalIdxCount;
	backendRenderUserData->info.totalCmdLists = draw_data->CmdListsCount;
	backendRenderUserData->info.totalCmds = 0;
#endif

	//
	if (draw_data->TotalVtxCount == 0 || draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
	{
		return;
	}

	// Allocate buffers
	auto& frameResources = backendRenderUserData->frameResources[backendRenderUserData->activeBufferId];

	// Ensure the current frame's vertex buffer is large enough.
	if (frameResources.pVertexBuffer == nullptr || frameResources.vertexCapacity < static_cast<uint32_t>(draw_data->TotalVtxCount))
	{
		if (frameResources.pVertexBuffer)
		{
			backendUserData->allocators.garlic.free(frameResources.pVertexBuffer);
		}

		frameResources.vertexCapacity = static_cast<uint32_t>(draw_data->TotalVtxCount);
		frameResources.pVertexBuffer = (void*)backendUserData->allocators.garlic.allocate<ImDrawVert>(frameResources.vertexCapacity);
	}

	// Ensure the current frame's index buffer is large enough.
	if (frameResources.pIndexBuffer == nullptr || frameResources.indexCapacity < static_cast<uint32_t>(draw_data->TotalIdxCount))
	{
		if (frameResources.pIndexBuffer)
		{
			backendUserData->allocators.garlic.free(frameResources.pIndexBuffer);
		}

		frameResources.indexCapacity = static_cast<uint32_t>(draw_data->TotalIdxCount);
		frameResources.pIndexBuffer = (void*)backendUserData->allocators.garlic.allocate<ImDrawIdx>(frameResources.indexCapacity);
	}

	// Constant buffer is always exactly one matrix.
	if (frameResources.pConstantBuffer == nullptr)
	{
		frameResources.pConstantBuffer = (void*)backendUserData->allocators.garlic.allocate<float4x4>(1);
	}

	if (frameResources.pTableBuffer == nullptr)
	{
		frameResources.pTableBuffer = (void*)backendUserData->allocators.garlic.allocate<sce::Gnm::Buffer>(3);
	}

	//
	[[maybe_unused]] void* cBufferData = frameResources.pConstantBuffer;
	[[maybe_unused]] void* vertexData = frameResources.pVertexBuffer;
	[[maybe_unused]] void* indexData = frameResources.pIndexBuffer;

	// Upload vertex/index data into a single contiguous GPU buffer
	{
		uint32_t	vtx_offset = 0;
		ImDrawVert* vtx_dst = static_cast<ImDrawVert*>(vertexData);
		ImDrawIdx*	idx_dst = static_cast<ImDrawIdx*>(indexData);

		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
			for (int i = 0; i < cmd_list->IdxBuffer.Size; i++)
			{
				idx_dst[i] = cmd_list->IdxBuffer[i] + vtx_offset;
			}

			vtx_dst += cmd_list->VtxBuffer.Size;
			idx_dst += cmd_list->IdxBuffer.Size;
			vtx_offset += cmd_list->VtxBuffer.Size;
		}
	}

	// Build projection matrix
	{
		float L = draw_data->DisplayPos.x;
		float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
		float T = draw_data->DisplayPos.y;
		float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
		float orthoProjMatrix[4][4] =
		{
			{ 2.0f / (R - L),	 0.0f,				0.0f, 0.0f },
			{ 0.0f,				 2.0f / (T - B),    0.0f, 0.0f },
			{ 0.0f,				 0.0f,				1.0f, 0.0f },
			{ (R + L) / (L - R), (T + B) / (B - T), 0.0f, 1.0f },
		};

		std::memcpy(cBufferData, orthoProjMatrix, sizeof(orthoProjMatrix));
	}

	// Setup Index/vertexBuffer/constantBuffer structures
	sce::Gnm::Buffer indexBuffer, vertexBuffer[3], constantBuffer;
	indexBuffer.initAsDataBuffer(indexData, indexFormat, draw_data->TotalIdxCount);
	constantBuffer.initAsConstantBuffer(cBufferData, sizeof(float4x4));
	vertexBuffer[0].initAsVertexBuffer((char*)vertexData + offsetof(VS_INPUT, position), sce::Gnm::kDataFormatR32G32Float, sizeof(ImDrawVert), draw_data->TotalVtxCount); // POSITION
	vertexBuffer[1].initAsVertexBuffer((char*)vertexData + offsetof(VS_INPUT, uv), sce::Gnm::kDataFormatR32G32Float, sizeof(ImDrawVert), draw_data->TotalVtxCount);     // TEXCOORD
	vertexBuffer[2].initAsVertexBuffer((char*)vertexData + offsetof(VS_INPUT, color), sce::Gnm::kDataFormatR8G8B8A8Unorm, sizeof(ImDrawVert), draw_data->TotalVtxCount);    // COLOR
	indexBuffer.setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);
	constantBuffer.setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);
	vertexBuffer[0].setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO); // R
	vertexBuffer[1].setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO); // R
	vertexBuffer[2].setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO); // R

	// Setup Pipline stuff
	sce::Gnm::BlendControl blendState;
	blendState.init();
	blendState.setBlendEnable(true);
	blendState.setSeparateAlphaEnable(true);
	blendState.setAlphaEquation(
		sce::Gnm::BlendMultiplier::kBlendMultiplierOne,
		sce::Gnm::BlendFunc::kBlendFuncAdd,
		sce::Gnm::BlendMultiplier::kBlendMultiplierOneMinusSrcAlpha
	);

	blendState.setColorEquation(
		sce::Gnm::BlendMultiplier::kBlendMultiplierSrcAlpha,
		sce::Gnm::BlendFunc::kBlendFuncAdd,
		sce::Gnm::BlendMultiplier::kBlendMultiplierOneMinusSrcAlpha
	);

	sce::Gnm::PrimitiveSetup primitiveSetup;
	primitiveSetup.init();
	primitiveSetup.setCullFace(sce::Gnm::PrimitiveSetupCullFaceMode::kPrimitiveSetupCullFaceNone);
	primitiveSetup.setPolygonMode(
		sce::Gnm::PrimitiveSetupPolygonMode::kPrimitiveSetupPolygonModeFill,
		sce::Gnm::PrimitiveSetupPolygonMode::kPrimitiveSetupPolygonModeFill
	);

	// 
	sce::Gnm::DepthStencilControl depthState;
	depthState.init();
	depthState.setDepthEnable(false);
	depthState.setDepthControl(sce::Gnm::DepthControlZWrite::kDepthControlZWriteDisable, sce::Gnm::CompareFunc::kCompareFuncAlways);
	depthState.setDepthBoundsEnable(false);

	// --- PS usage table ---
	auto& vs = backendRenderUserData->vertexShader;
	auto& ps = backendRenderUserData->pixelShader;

	uint32_t psInputs[32];
	sce::Gnm::generatePsShaderUsageTable(
		psInputs, 
		vs.m_shader->getExportSemanticTable(), 
		vs.m_shader->m_numExportSemantics, 
		ps.m_shader->getPixelInputSemanticTable(), 
		ps.m_shader->m_numInputSemantics);

	//
	gfxCtx.pushMarker("ImGui");

	// Bind Pipeline
	gfxCtx.setBlendControl(0, blendState);
	gfxCtx.setPrimitiveSetup(primitiveSetup);
	gfxCtx.setDepthStencilControl(depthState);
	gfxCtx.setActiveShaderStages(sce::Gnm::ActiveShaderStages::kActiveShaderStagesVsPs);
	gfxCtx.setPrimitiveType(sce::Gnm::PrimitiveType::kPrimitiveTypeTriList);
	gfxCtx.setNumInstances(1);
	gfxCtx.setVsShader(vs.m_shader, 0, vs.m_fetchShader);
	gfxCtx.setPsShader(ps.m_shader);
	gfxCtx.setPsShaderUsage(psInputs, ps.m_shader->m_numInputSemantics);
	gfxCtx.setVertexBuffers(sce::Gnm::ShaderStage::kShaderStageVs, 0, 3, vertexBuffer);
	gfxCtx.setConstantBuffers(sce::Gnm::ShaderStage::kShaderStageVs, 0, 1, &constantBuffer);
	gfxCtx.setIndexSize(indexSize);
	gfxCtx.setIndexCount(draw_data->TotalIdxCount);
	gfxCtx.setIndexBuffer(indexBuffer.getBaseAddress());
	gfxCtx.setupScreenViewport(0, 0, draw_data->DisplaySize.x, draw_data->DisplaySize.y, 0.5f, 0.5f);

#if _DEBUG
	PRINT_FMT_VA
	(
		"setupScreenViewport:\n"
		"left -> %d\n"
		"top -> %d\n"
		"right -> %f\n"
		"bottom -> %f\n"
		"zScale -> %f\n"
		"zOffset -> %f",
		0,
		0,
		draw_data->DisplaySize.x,
		draw_data->DisplaySize.y,
		0.5f,
		0.5f
	);

	fprintf(stdout, "TotalIdxCount: %d", draw_data->TotalIdxCount);
	fprintf(stdout, "vertexData:   %p", vertexData);
	fprintf(stdout, "indexData:    %p", indexData);
	fprintf(stdout, "indexBase:    %p", indexBuffer.getBaseAddress());
	fprintf(stdout, "cbData:       %p", cBufferData);
	fprintf(stdout, "fontTexBase:  %p", fontTexture.getBaseAddress());
	fprintf(stdout, "fetchShader:  %p", backendRenderUserData->vertexShader.m_fetchShader);
#endif

	// Draw
	uint32_t indexBufferOffset = 0;
	ImVec2 clip_off = draw_data->DisplayPos;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];

		backendRenderUserData->info.totalCmds += cmd_list->CmdBuffer.Size;

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

			// Project scissor/clipping rectangles into framebuffer space
			ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
			ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);
			if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
				continue;

#if _DEBUG
			PRINT_FMT_VA
			(
				"setScreenScissor:\n"
				"left -> %d\n"
				"top -> %d\n"
				"right -> %d\n"
				"bottom -> %d\n",
				(int)clip_min.x,
				(int)clip_min.y,
				(int)clip_max.x,
				(int)clip_max.y
			);
#endif

			//
			PlayStationImage* psImage = (PlayStationImage*)pcmd->GetTexID();
			if (psImage)
			{
				gfxCtx.setTextures(sce::Gnm::ShaderStage::kShaderStagePs, 0, 1, psImage->texture);
				gfxCtx.setSamplers(sce::Gnm::ShaderStage::kShaderStagePs, 0, 1, psImage->sampler);

			}

			//
			const uint32_t startIndex = indexBufferOffset + pcmd->IdxOffset;
			gfxCtx.setScreenScissor((int)clip_min.x, (int)clip_min.y, (int)clip_max.x, (int)clip_max.y);
			gfxCtx.drawIndexOffset(startIndex, pcmd->ElemCount);
		}

		indexBufferOffset += cmd_list->IdxBuffer.size();
	}

	//
	gfxCtx.popMarker();

#if _DEBUG
#else
	backendRenderUserData->info.deltaTime = ImGui::GetIO().DeltaTime;
	backendRenderUserData->info.frameCount++;
#endif

	//
#if _DEBUG
	PRINT_POS;
#endif
}

/*
	-- Draw uisng a lightweight graphics context (event with a graphics context you can just call ImGui_ImplPlayStation_RenderDrawData(&gfxCtx.m_dcb, ..) and it should work)
*/
/*__noinline*/ IMGUI_IMPL_API void ImGui_ImplPlayStation_RenderDrawData(ImGuiLightweightGfxContext& gfxCtx, ImDrawData* draw_data)
{
	auto* backendUserData = ImGui_ImplPlayStation_GetBackendUserData();
	IM_ASSERT(backendUserData != nullptr && "ImGui_ImplPlayStation_Init() not called");

	auto* backendRenderUserData = ImGui_ImplPlayStation_GetBackendRenderUserData();
	IM_ASSERT(backendRenderUserData != nullptr && "ImGui_ImplPlayStation_Init() not called");

#if _DEBUG
	PRINT_POS;
	fprintf(stdout, "TotalVtxCount: %d", draw_data->TotalVtxCount);
	fprintf(stdout, "TotalIdxCount: %d", draw_data->TotalIdxCount);
	fprintf(stdout, "DisplaySize.x: %f, DisplaySize.y: %f", draw_data->DisplaySize.x, draw_data->DisplaySize.y);
#else
	backendRenderUserData->info.totalVtx = draw_data->TotalVtxCount;
	backendRenderUserData->info.totalIdx = draw_data->TotalIdxCount;
	backendRenderUserData->info.totalCmdLists = draw_data->CmdListsCount;
	backendRenderUserData->info.totalCmds = 0;
#endif

	//
	if (draw_data->TotalVtxCount == 0 || draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
	{
		return;
	}

	// Allocate buffers
	auto& frameResources = backendRenderUserData->frameResources[backendRenderUserData->activeBufferId];

	// Ensure the current frame's vertex buffer is large enough.
	if (frameResources.pVertexBuffer == nullptr || frameResources.vertexCapacity < static_cast<uint32_t>(draw_data->TotalVtxCount))
	{
		if (frameResources.pVertexBuffer)
		{
			backendUserData->allocators.garlic.free(frameResources.pVertexBuffer);
		}

		frameResources.vertexCapacity = static_cast<uint32_t>(draw_data->TotalVtxCount);
		frameResources.pVertexBuffer = (void*)backendUserData->allocators.garlic.allocate<ImDrawVert>(frameResources.vertexCapacity);
	}

	// Ensure the current frame's index buffer is large enough.
	if (frameResources.pIndexBuffer == nullptr || frameResources.indexCapacity < static_cast<uint32_t>(draw_data->TotalIdxCount))
	{
		if (frameResources.pIndexBuffer)
		{
			backendUserData->allocators.garlic.free(frameResources.pIndexBuffer);
		}

		frameResources.indexCapacity = static_cast<uint32_t>(draw_data->TotalIdxCount);
		frameResources.pIndexBuffer = (void*)backendUserData->allocators.garlic.allocate<ImDrawIdx>(frameResources.indexCapacity);
	}

	// Constant buffer is always exactly one matrix.
	if (frameResources.pConstantBuffer == nullptr)
	{
		frameResources.pConstantBuffer = (void*)backendUserData->allocators.garlic.allocate<float4x4>(1);
	}

	if (frameResources.pTableBuffer == nullptr)
	{
		frameResources.pTableBuffer = (void*)backendUserData->allocators.garlic.allocate<sce::Gnm::Buffer>(3);
	}

	//
	[[maybe_unused]] void* cBufferData = frameResources.pConstantBuffer;
	[[maybe_unused]] void* vertexData = frameResources.pVertexBuffer;
	[[maybe_unused]] void* indexData = frameResources.pIndexBuffer;

	// Upload vertex/index data into a single contiguous GPU buffer
	{
		uint32_t	vtx_offset = 0;
		ImDrawVert* vtx_dst = static_cast<ImDrawVert*>(vertexData);
		ImDrawIdx*	idx_dst = static_cast<ImDrawIdx*>(indexData);

		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
			for (int i = 0; i < cmd_list->IdxBuffer.Size; i++)
			{
				idx_dst[i] = cmd_list->IdxBuffer[i] + vtx_offset;
			}

			vtx_dst += cmd_list->VtxBuffer.Size;
			idx_dst += cmd_list->IdxBuffer.Size;
			vtx_offset += cmd_list->VtxBuffer.Size;
		}
	}

	// Build projection matrix
	{
		float L = draw_data->DisplayPos.x;
		float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
		float T = draw_data->DisplayPos.y;
		float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
		float orthoProjMatrix[4][4] =
		{
			{ 2.0f / (R - L),	 0.0f,				0.0f, 0.0f },
			{ 0.0f,				 2.0f / (T - B),    0.0f, 0.0f },
			{ 0.0f,				 0.0f,				1.0f, 0.0f },
			{ (R + L) / (L - R), (T + B) / (B - T), 0.0f, 1.0f },
		};

		std::memcpy(cBufferData, orthoProjMatrix, sizeof(orthoProjMatrix));
	}

	// Setup Index/vertexBuffer/constantBuffer structures
	sce::Gnm::Buffer indexBuffer, vertexBuffer[3], constantBuffer;
	indexBuffer.initAsDataBuffer(indexData, indexFormat, draw_data->TotalIdxCount);
	constantBuffer.initAsConstantBuffer(cBufferData, sizeof(float4x4));
	vertexBuffer[0].initAsVertexBuffer((char*)vertexData + offsetof(VS_INPUT, position), sce::Gnm::kDataFormatR32G32Float, sizeof(ImDrawVert), draw_data->TotalVtxCount); // POSITION
	vertexBuffer[1].initAsVertexBuffer((char*)vertexData + offsetof(VS_INPUT, uv), sce::Gnm::kDataFormatR32G32Float, sizeof(ImDrawVert), draw_data->TotalVtxCount); // TEXCOORD
	vertexBuffer[2].initAsVertexBuffer((char*)vertexData + offsetof(VS_INPUT, color), sce::Gnm::kDataFormatR8G8B8A8Unorm, sizeof(ImDrawVert), draw_data->TotalVtxCount);
	indexBuffer.setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);
	constantBuffer.setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);
	vertexBuffer[0].setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO); // R
	vertexBuffer[1].setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO); // R
	vertexBuffer[2].setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO); // R

	// Setup Pipline stuff
	sce::Gnm::BlendControl blendState;
	blendState.init();
	blendState.setBlendEnable(true);
	blendState.setSeparateAlphaEnable(true);
	blendState.setAlphaEquation(
		sce::Gnm::BlendMultiplier::kBlendMultiplierOne,
		sce::Gnm::BlendFunc::kBlendFuncAdd,
		sce::Gnm::BlendMultiplier::kBlendMultiplierOneMinusSrcAlpha
	);

	blendState.setColorEquation(
		sce::Gnm::BlendMultiplier::kBlendMultiplierSrcAlpha,
		sce::Gnm::BlendFunc::kBlendFuncAdd,
		sce::Gnm::BlendMultiplier::kBlendMultiplierOneMinusSrcAlpha
	);

	sce::Gnm::PrimitiveSetup primitiveSetup;
	primitiveSetup.init();
	primitiveSetup.setCullFace(sce::Gnm::PrimitiveSetupCullFaceMode::kPrimitiveSetupCullFaceNone);
	primitiveSetup.setPolygonMode(
		sce::Gnm::PrimitiveSetupPolygonMode::kPrimitiveSetupPolygonModeFill,
		sce::Gnm::PrimitiveSetupPolygonMode::kPrimitiveSetupPolygonModeFill
	);

	// 
	sce::Gnm::DepthStencilControl depthState;
	depthState.init();
	depthState.setDepthEnable(false);
	depthState.setDepthControl(sce::Gnm::DepthControlZWrite::kDepthControlZWriteDisable, sce::Gnm::CompareFunc::kCompareFuncAlways);
	depthState.setDepthBoundsEnable(false);

	// --- PS usage table ---
	auto& vs = backendRenderUserData->vertexShader;
	auto& ps = backendRenderUserData->pixelShader;

	uint32_t psInputs[32];
	sce::Gnm::generatePsShaderUsageTable(
		psInputs,
		vs.m_shader->getExportSemanticTable(),
		vs.m_shader->m_numExportSemantics,
		ps.m_shader->getPixelInputSemanticTable(),
		ps.m_shader->m_numInputSemantics);

	//
	gfxCtx.pushMarker("ImGui");

	// Bind Pipeline
	gfxCtx.setVsShader(vs.m_shader, 0, vs.m_fetchShader, &vs.m_resourcesOffsets);
	gfxCtx.setPsShader(ps.m_shader, &ps.m_resourcesOffsets);
	gfxCtx.setBlendControl(0, blendState);
	gfxCtx.setPrimitiveSetup(primitiveSetup);
	gfxCtx.setDepthStencilControl(depthState);
	gfxCtx.setActiveShaderStages(sce::Gnm::ActiveShaderStages::kActiveShaderStagesVsPs);
	gfxCtx.setPrimitiveType(sce::Gnm::PrimitiveType::kPrimitiveTypeTriList);
	gfxCtx.setNumInstances(1);
	gfxCtx.setPsShaderUsage(psInputs, ps.m_shader->m_numInputSemantics);
	gfxCtx.setVertexBuffers(sce::Gnm::ShaderStage::kShaderStageVs, 0, 3, vertexBuffer);
	gfxCtx.setConstantBuffers(sce::Gnm::ShaderStage::kShaderStageVs, 0, 1, &constantBuffer);
	gfxCtx.setIndexSize(indexSize);
	gfxCtx.setIndexCount(draw_data->TotalIdxCount);
	gfxCtx.setIndexBuffer(indexBuffer.getBaseAddress());
	gfxCtx.setupScreenViewport(0, 0, draw_data->DisplaySize.x, draw_data->DisplaySize.y, 0.5f, 0.5f);

#if _DEBUG
	fprintf(stdout, 
		"setupScreenViewport:\n"
		"left -> %d\n"
		"top -> %d\n"
		"right -> %f\n"
		"bottom -> %f\n"
		"zScale -> %f\n"
		"zOffset -> %f",
		0,
		0,
		draw_data->DisplaySize.x,
		draw_data->DisplaySize.y,
		0.5f,
		0.5f
	);

	fprintf(stdout, "TotalIdxCount: %d", draw_data->TotalIdxCount);
	fprintf(stdout, "vertexData:   %p", vertexData);
	fprintf(stdout, "indexData:    %p", indexData);
	fprintf(stdout, "indexBase:    %p", indexBuffer.getBaseAddress());
	fprintf(stdout, "cbData:       %p", cBufferData);
	fprintf(stdout, "fontTexBase:  %p", fontTexture.getBaseAddress());
	fprintf(stdout, "fetchShader:  %p", backendRenderUserData->vertexShader.m_fetchShader);
#endif

	// Draw
	uint32_t indexBufferOffset = 0;
	ImVec2 clip_off = draw_data->DisplayPos;

	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];

		backendRenderUserData->info.totalCmds += cmd_list->CmdBuffer.Size;

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

			// Project scissor/clipping rectangles into framebuffer space
			ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
			ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);
			if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
				continue;

#if _DEBUG
			PRINT_FMT_VA
			(
				"setScreenScissor:\n"
				"left -> %d\n"
				"top -> %d\n"
				"right -> %d\n"
				"bottom -> %d\n",
				(int)clip_min.x,
				(int)clip_min.y,
				(int)clip_max.x,
				(int)clip_max.y
			);
#endif
			//
			PlayStationImage* psImage = (PlayStationImage*)pcmd->GetTexID();
			if (psImage)
			{
				gfxCtx.setTextures(sce::Gnm::ShaderStage::kShaderStagePs, 0, 1, psImage->texture);
				gfxCtx.setSamplers(sce::Gnm::ShaderStage::kShaderStagePs, 0, 1, psImage->sampler);
			}

			//
			const uint32_t startIndex = indexBufferOffset + pcmd->IdxOffset;
			gfxCtx.setScreenScissor((int)clip_min.x, (int)clip_min.y, (int)clip_max.x, (int)clip_max.y);
			gfxCtx.drawIndexOffset(startIndex, pcmd->ElemCount);
		}

		indexBufferOffset += cmd_list->IdxBuffer.size();
	}

	//
	gfxCtx.popMarker();

#ifdef _DEBUG
#else
	backendRenderUserData->info.deltaTime = ImGui::GetIO().DeltaTime;
	backendRenderUserData->info.frameCount++;
#endif

	//
#if _DEBUG
	PRINT_POS;
#endif
}


IMGUI_IMPL_API void ImGui_ImplPlayStation_EnableInputPolling()
{
	auto* backendUserData = ImGui_ImplPlayStation_GetBackendUserData();
	IM_ASSERT(backendUserData != nullptr && "Context or backend not initialized? Did you call ImGui_ImplPlayStation_Init()?");

	//
	backendUserData->PollInput = true;
}

IMGUI_IMPL_API void ImGui_ImplPlayStation_DisableInputPolling()
{
	auto* backendUserData = ImGui_ImplPlayStation_GetBackendUserData();
	IM_ASSERT(backendUserData != nullptr && "Context or backend not initialized? Did you call ImGui_ImplPlayStation_Init()?");

	//
	backendUserData->PollInput = false;
}

IMGUI_IMPL_API void ImGui_ImplPlayStation_AddProcessedMouseEvent(float x, float y)
{
	//
	auto* backendUserData = ImGui_ImplPlayStation_GetBackendUserData();
	IM_ASSERT(backendUserData != nullptr && "ImGui_ImplPlayStation_Init() not called");

	//
	backendUserData->mouseProcessor.ProcessDelta(x, y);

	//
	ImGuiIO& io = ImGui::GetIO();
	io.AddMousePosEvent(backendUserData->mouseProcessor.X(), backendUserData->mouseProcessor.Y());
}

IMGUI_IMPL_API void ImGui_ImplPlayStation_SetDisplaySize(unsigned int height, unsigned int width)
{
	//
	auto* backendUserData = ImGui_ImplPlayStation_GetBackendUserData();
	IM_ASSERT(backendUserData != nullptr && "ImGui_ImplPlayStation_Init() not called");

	//
	backendUserData->mouseProcessor.SetDisplaySize(width, height);
}

IMGUI_IMPL_API void ImGui_ImplPlayStation_SetSensitivity(float ft)
{
	//
	auto* backendUserData = ImGui_ImplPlayStation_GetBackendUserData();
	IM_ASSERT(backendUserData != nullptr && "ImGui_ImplPlayStation_Init() not called");

	//
	backendUserData->mouseProcessor.SetSensitivity(ft);
}

IMGUI_IMPL_API float ImGui_ImplPlayStation_GetSensitivity()
{
	//
	auto* backendUserData = ImGui_ImplPlayStation_GetBackendUserData();
	IM_ASSERT(backendUserData != nullptr && "ImGui_ImplPlayStation_Init() not called");

	//
	return backendUserData->mouseProcessor.GetSensitivity();
}

IMGUI_IMPL_API void ImGui_ImplPlayStation_SetMouseAccelerationEnabled(bool b)
{
	//
	auto* backendUserData = ImGui_ImplPlayStation_GetBackendUserData();
	IM_ASSERT(backendUserData != nullptr && "ImGui_ImplPlayStation_Init() not called");

	//
	return backendUserData->mouseProcessor.ToggleAcceleration(b);
}

IMGUI_IMPL_API bool ImGui_ImplPlayStation_GetMouseAccelerationEnabled()
{
	//
	auto* backendUserData = ImGui_ImplPlayStation_GetBackendUserData();
	IM_ASSERT(backendUserData != nullptr && "ImGui_ImplPlayStation_Init() not called");

	//
	return backendUserData->mouseProcessor.GetUseAcceleration();
}
#endif