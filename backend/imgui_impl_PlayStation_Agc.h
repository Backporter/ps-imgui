#pragma once

#ifndef __PROSPERO__
#error "Invalid Backend Target Included In Project"
#endif

//
#include "imgui.h" // IMGUI_IMPL_API

// forward defs to avoid including headers.
struct ScePadControllerInformation;
struct ScePadData;
struct SceMouseData;
struct SceImeEvent;

namespace sce
{
	namespace Agc
	{
		class DrawCommandBuffer;

		namespace Core
		{
			class Texture;
			class Sampler;
		}
	}
}

using ImGuiDrawCommandBuffer = sce::Agc::DrawCommandBuffer;

struct PlayStationImage
{
	sce::Agc::Core::Texture* texture;
	sce::Agc::Core::Sampler* sampler;

	ImTextureID TextureID() { return reinterpret_cast<ImTextureID>(this); }
};
static_assert(alignof(PlayStationImage) == 8);

// ImGui Relies on the user to provide a input sink or you'll need to do input stuff yourself
template <class T>
struct ImGui_InputBase
{
public:
	using ReadData = bool(T**, int*);
	using ReadInfo = bool(void*);
public:
	ImGui_InputBase() = default;

	ImGui_InputBase(ReadData* a_readFunc, ReadInfo* a_readInfo) :
		_readFunc(a_readFunc),
		_readInfo(a_readInfo)
	{}

	~ImGui_InputBase() = default;

	bool Poll()
	{
		if (_readFunc)
		{
			return _readFunc(&_data, &_dataSize);
		}
		else
		{
			return false;
		}
	}

	bool PollInfo(void* info)
	{
		if (_readInfo)
		{
			return _readInfo(info);
		}
		else
		{
			return false;
		}
	}

	operator bool() const noexcept { return static_cast<bool>(_readFunc); }

	int32_t count() const { return _dataSize; }
	T*      data() const { return _data; }
private:
	int32_t	  _dataSize = 0;
	T*        _data = nullptr;
	ReadData* _readFunc = nullptr;
	ReadInfo* _readInfo = nullptr;
};

// ImGui Relies on the user to provide a allocator that can allocate and free, (type doesn't matter, just ensure that if using a stack allocator you give imgui it's own allocator that's not shared)
class ImGui_Allocator
{
public:
	using allocate_t = void*(void*, unsigned int, unsigned int);
	using free_t = void(void*, void*);

	ImGui_Allocator() = default;

	ImGui_Allocator(void* a_instance, allocate_t* a_alloc, free_t* a_free) :
		_instance(a_instance),
		_allocate(a_alloc),
		_free(a_free)
	{}

	~ImGui_Allocator() = default;


	inline void* allocate(int a_size, unsigned int a_alignment)
	{
		return _allocate(_instance, a_size, a_alignment);
	}

	template <typename T>
	inline T* allocate(int a_num = 1, unsigned int a_alignment = alignof(T))
	{
		return static_cast<T*>(_allocate(_instance, sizeof(T) * a_num, a_alignment));
	}

	inline void free(void* ptr)
	{
		return _free(_instance, ptr);
	}
public:
	void*		_instance = nullptr;
	allocate_t* _allocate = nullptr;
	free_t*		_free = nullptr;
};

// we need both, CPU allocations are needed as well as GPU allocations
struct ImGui_Allocators
{
	ImGui_Allocator onion;
	ImGui_Allocator garlic;
};

struct ImGui_ImplPlayStation_RenderInfoData
{
	ImGui_ImplPlayStation_RenderInfoData() 
	{ 
		memset(this, 0, sizeof(*this)); 
	}

	~ImGui_ImplPlayStation_RenderInfoData() = default;

	float    deltaTime = 0.f;
	uint64_t frameCount = 0;
	size_t   totalVtx = 0;
	size_t   totalIdx = 0;
	size_t   totalCmdLists = 0;
	size_t   totalCmds = 0;
};

struct ImGui_InitUserData
{
	// Allocators
	ImGui_Allocators allocators;

	// Input Sinks ->
	ImGui_InputBase<SceMouseData> MouseInput;
	ImGui_InputBase<ScePadData>   GamePadInput;
	ImGui_InputBase<SceImeEvent>  KeyboardInput;

	// toggle settings
	bool  EnableMouseSensitivity = false;
	bool  EnableMouseAcceleration = false;
	bool  EnableMouseScaling = false;
	bool  EnableMouseSmoothing = false;
	float MouseSensitivity = 1.0f;
	float MouseAccelerationFactor = 0.0f;
};

#ifndef IMGUI_DISABLE
IMGUI_IMPL_API bool ImGui_ImplPlayStation_Init(ImGui_InitUserData& a_allocators, unsigned int width, unsigned int height);
IMGUI_IMPL_API void ImGui_ImplPlayStation_Shutdown();
IMGUI_IMPL_API void ImGui_ImplPlayStation_NewFrame();

//
extern void* ImGui_PlayStation_MemAllocFunc(size_t sz, void*);
extern void  ImGui_PlayStation_MemFreeFunc(void* ptr, void*);

// #
/*
	- Uses a raw sce::Gnm::DrawCommandBuffer for it's rendering
*/
/*__noinline*/ IMGUI_IMPL_API void ImGui_ImplPlayStation_RenderDrawData(ImGuiDrawCommandBuffer&, ImDrawData* draw_data);

// process deltas using MouseProcessor push a mouse event via ImGui::GetIO().AddMousePosEvent
IMGUI_IMPL_API void ImGui_ImplPlayStation_AddProcessedMouseEvent(float x, float y);

// Enable/Disable Polling of the Input Sinks
IMGUI_IMPL_API void ImGui_ImplPlayStation_EnableInputPolling();
IMGUI_IMPL_API void ImGui_ImplPlayStation_DisableInputPolling();

// MouseProcessor Setters/Getters
IMGUI_IMPL_API void  ImGui_ImplPlayStation_SetDisplaySize(unsigned int width, unsigned int height);
IMGUI_IMPL_API void  ImGui_ImplPlayStation_SetSensitivity(float);
IMGUI_IMPL_API void  ImGui_ImplPlayStation_SetMouseAccelerationEnabled(bool);
IMGUI_IMPL_API float ImGui_ImplPlayStation_GetSensitivity();
IMGUI_IMPL_API bool  ImGui_ImplPlayStation_GetMouseAccelerationEnabled();
#endif // #ifndef IMGUI_DISABLE