// shader dumping stuff
#if _DEBUG
#include <shader.h>
#include <agc.h>
#include <agc/core/texturespec.h>
#include <agc/drawcommandbuffer.h>
#include <agc/gnmp/dataformats.h>

namespace
{
	[[maybe_unused]]const char* shader_type_name(sce::Agc::ShaderType type)
	{
		using namespace sce::Agc;

		switch (type)
		{
		case ShaderType::kCs:
			return "CS";
		case ShaderType::kPs:
			return "PS";
		case ShaderType::kGs:
			return "GS";
		case ShaderType::kHs:
			return "HS";
		case ShaderType::kGsFront:
			return "GS_FRONT";
		case ShaderType::kHsFront:
			return "HS_FRONT";
		case ShaderType::kGsBack:
			return "GS_BACK";
		case ShaderType::kHsBack:
			return "HS_BACK";
		case ShaderType::kFs:
			return "FS";
		default:
			return "UNKNOWN";
		}
	}

	[[maybe_unused]]const char* sharp_type_name(sce::Agc::UserDataLayout::SharpResourceType type)
	{
		using Type = sce::Agc::UserDataLayout::SharpResourceType;

		switch (type)
		{
		case Type::kReadOnlySharp:
			return "ReadOnlySharp";
		case Type::kReadWriteSharp:
			return "ReadWriteSharp";
		case Type::kSampler:
			return "Sampler";
		case Type::kConstBuffer:
			return "ConstBuffer";
		default:
			return "Unknown";
		}
	}

	[[maybe_unused]] const char* register_type_name(sce::Agc::RegisterType type)
	{
		using namespace sce::Agc;

		switch (type & RegisterType::kTypeBitMask)
		{
		case RegisterType::kCx:
			return "CX";
		case RegisterType::kSh:
			return "SH";
		case RegisterType::kUc:
			return "UC";
		default:
			return "UNKNOWN";
		}
	}

	[[maybe_unused]] void dump_register(const sce::Agc::CxRegister& reg, const char* name)
	{
		fprintf(stdout, 
			"    %-32s offset=0x%08" PRIx32 " value=0x%08" PRIx32 "",
			name,
			static_cast<uint32_t>(reg.m_offset),
			reg.m_value);
	}

	[[maybe_unused]] void dump_register(const sce::Agc::ShRegister& reg, const char* name)
	{
		fprintf(stdout, 
			"    %-32s offset=0x%08" PRIx32 " value=0x%08" PRIx32 "",
			name,
			static_cast<uint32_t>(reg.m_offset),
			reg.m_value);
	}

	[[maybe_unused]] void dump_register(const sce::Agc::UcRegister& reg, const char* name)
	{
		fprintf(stdout, 
			"    %-32s offset=0x%08" PRIx32 " value=0x%08" PRIx32 "",
			name,
			static_cast<uint32_t>(reg.m_offset),
			reg.m_value);
	}

	[[maybe_unused]] void dump_semantic(const sce::Agc::Semantic& s, uint32_t index, const char* category)
	{
		fprintf(stdout, 
			"    [%3" PRIu32 "] %-8s "
			"semantic=%3" PRIu32
			" hwMapping=%3" PRIu32
			" size=%" PRIu32
			" F16=%" PRIu32
			" flat=%" PRIu32
			" linear=%" PRIu32
			" custom=%" PRIu32
			" staticVB=%" PRIu32
			" staticAttr=%" PRIu32
			" default=%" PRIu32
			" defaultHi=%" PRIu32
			"",
			index,
			category,
			s.m_semantic,
			s.m_hardwareMapping,
			s.m_sizeInElements,
			s.m_isF16,
			s.m_isFlatShaded,
			s.m_isLinear,
			s.m_isCustom,
			s.m_staticVbIndex,
			s.m_staticAttribute,
			s.m_defaultValue,
			s.m_defaultValueHi);
	}

	[[maybe_unused]] void dump_user_data(const sce::Agc::UserDataLayout* ud)
	{
		using namespace sce::Agc;

		fprintf(stdout, "");
		fprintf(stdout, "  UserDataLayout");
		fprintf(stdout, "  ------------------------------");

		if (!ud)
		{
			fprintf(stdout, "    <null>");
			return;
		}

		fprintf(stdout, 
			"    m_directResourceOffset = %p",
			static_cast<void*>(ud->m_directResourceOffset));

		fprintf(stdout, 
			"    m_eudSizeInDwords      = %" PRIu16 "",
			ud->m_eudSizeInDwords);

		fprintf(stdout, 
			"    m_srtSizeInDwords      = %" PRIu16 "",
			ud->m_srtSizeInDwords);

		fprintf(stdout, 
			"    m_directResourceCount  = %" PRIu16 "",
			ud->m_directResourceCount);

		//
		// Direct resources
		//
		fprintf(stdout, "");
		fprintf(stdout, "    Direct Resources");
		fprintf(stdout, "    ------------------------------");

		if (ud->m_directResourceOffset)
		{
			for (uint32_t i = 0; i < ud->m_directResourceCount; ++i)
			{
				fprintf(stdout, 
					"      [%3" PRIu32 "] offset = 0x%04" PRIx16
					" (%" PRIu16 " dwords)",
					i,
					ud->m_directResourceOffset[i],
					ud->m_directResourceOffset[i]);
			}
		}
		else
		{
			fprintf(stdout, "      <null>");
		}

		//
		// Sharp resources
		//
		fprintf(stdout, "");
		fprintf(stdout, "    Sharp Resources");
		fprintf(stdout, "    ------------------------------");

		for (uint32_t type = 0;
			type <= static_cast<uint32_t>(UserDataLayout::SharpResourceType::kLast);
			++type)
		{
			const auto sharpType =
				static_cast<UserDataLayout::SharpResourceType>(type);

			const uint32_t count = ud->m_sharpResourceCount[type];

			fprintf(stdout, 
				"      %-16s count=%" PRIu32 " ptr=%p",
				sharp_type_name(sharpType),
				count,
				static_cast<void*>(ud->m_sharpResourceOffset[type]));

			if (!ud->m_sharpResourceOffset[type])
				continue;

			for (uint32_t i = 0; i < count; ++i)
			{
				const auto& sharp = ud->m_sharpResourceOffset[type][i];

				fprintf(stdout, 
					"        [%3" PRIu32 "] "
					"offsetInDwords=%" PRIu16
					" small=%" PRIu16
					"",
					i,
					sharp.m_offsetInDwords,
					sharp.m_small);
			}
		}
	}

	[[maybe_unused]] void dump_specials(const sce::Agc::SpecialRegisters* specials)
	{
		using namespace sce::Agc;

		fprintf(stdout, "");
		fprintf(stdout, "  Special Registers");
		fprintf(stdout, "  ------------------------------");

		if (!specials)
		{
			fprintf(stdout, "    <null>");
			return;
		}

		dump_register(specials->m_geCntl, "m_geCntl");
		dump_register(specials->m_vgtShaderStagesEn, "m_vgtShaderStagesEn");

		fprintf(stdout, 
			"    %-32s compilerBlock0=%" PRIu32
			" useThreadDimensions=%" PRIu32
			" compilerBlock1=%" PRIu32
			" default=%" PRIu32
			"",
			"m_dispatchModifier",
			specials->m_dispatchModifier.m_compilerBlock0,
			specials->m_dispatchModifier.m_useThreadDimensions,
			specials->m_dispatchModifier.m_compilerBlock1,
			specials->m_dispatchModifier.m_default);

		fprintf(stdout, 
			"    %-32s start=%" PRIu16
			" end=%" PRIu16
			" size=%" PRIu16
			"",
			"m_userDataRange",
			specials->m_userDataRange.m_start,
			specials->m_userDataRange.m_end,
			specials->m_userDataRange.size());

		fprintf(stdout, 
			"    %-32s "
			"startVertex=%" PRIu32
			" startIndex=%" PRIu32
			" startInstance=%" PRIu32
			" drawIndex=%" PRIu32
			" userVGPRs=%" PRIu32
			" rtSlice=%" PRIu32
			" fuseDraws=%" PRIu32
			" compilerFlags=0x%08" PRIx32
			" default=%" PRIu32
			" reserved=0x%08" PRIx32
			"",
			"m_drawModifier",
			specials->m_drawModifier.m_enableStartVertexOffset,
			specials->m_drawModifier.m_enableStartIndexOffset,
			specials->m_drawModifier.m_enableStartInstanceOffset,
			specials->m_drawModifier.m_enableDrawIndex,
			specials->m_drawModifier.m_enableUserVGPRs,
			specials->m_drawModifier.m_renderTargetSliceOffset,
			specials->m_drawModifier.m_fuseDraws,
			specials->m_drawModifier.m_compilerFlags,
			specials->m_drawModifier.m_default,
			specials->m_drawModifier.m_reserved);

		dump_register(
			specials->m_vgtGsOutPrimType,
			"m_vgtGsOutPrimType");

		dump_register(
			specials->m_geUserVgprEn,
			"m_geUserVgprEn");
	}


	[[maybe_unused]] void dump_shader(const sce::Agc::Shader* shader, bool dump_code = false)
	{
		using namespace sce::Agc;

		fprintf(stdout, "");
		fprintf(stdout, "============================================================");
		fprintf(stdout, "AGC Shader Dump");
		fprintf(stdout, "============================================================");

		if (!shader)
		{
			fprintf(stdout, "<null Shader *>");
			return;
		}

		//
		// Top-level Shader
		//
		fprintf(stdout, "");
		fprintf(stdout, "Shader @ %p", static_cast<const void*>(shader));
		fprintf(stdout, "------------------------------");

		fprintf(stdout, 
			"  m_fileHeader                 = 0x%08" PRIx32
			"  %s",
			shader->m_fileHeader,
			shader->m_fileHeader == Shader::kMagic ? "(valid magic)" : "(BAD MAGIC)");

		fprintf(stdout, 
			"  m_version                   = %" PRIu32
			"  %s",
			shader->m_version,
			shader->m_version == Shader::kVersion ? "(expected)" : "(different version)");

		fprintf(stdout, 
			"  m_headerSize                = %" PRIu32 " bytes",
			shader->m_headerSize);

		fprintf(stdout, 
			"  m_shaderSize                = %" PRIu32 " bytes",
			shader->m_shaderSize);

		fprintf(stdout, 
			"  m_embeddedConstantBufferSizeInDQW = %" PRIu32 "",
			shader->m_embeddedConstantBufferSizeInDQW);

		fprintf(stdout, 
			"  m_target                    = 0x%08" PRIx32 "",
			shader->m_target);

		fprintf(stdout, 
			"  m_type                      = 0x%02x (%s)",
			static_cast<uint32_t>(shader->m_type),
			shader_type_name(shader->m_type));

		fprintf(stdout, 
			"  m_numInputSemantics        = %" PRIu32 "",
			shader->m_numInputSemantics);

		fprintf(stdout, 
			"  m_numOutputSemantics       = %" PRIu16 "",
			shader->m_numOutputSemantics);

		fprintf(stdout, 
			"  m_scratchSizeInDWPerThread = %" PRIu16 "",
			shader->m_scratchSizeInDWPerThread);

		fprintf(stdout, 
			"  m_specialSizesInBytes      = %" PRIu16 "",
			shader->m_specialSizesInBytes);

		fprintf(stdout, 
			"  m_numCxRegisters           = %" PRIu8 "",
			shader->m_numCxRegisters);

		fprintf(stdout, 
			"  m_numShRegisters           = %" PRIu8 "",
			shader->m_numShRegisters);

		//
		// Pointers
		//
		fprintf(stdout, "");
		fprintf(stdout, "  Pointers");
		fprintf(stdout, "  ------------------------------");

		fprintf(stdout, 
			"    m_userData       = %p",
			static_cast<void*>(shader->m_userData));

		fprintf(stdout, 
			"    m_code           = %p",
			const_cast<const void*>(shader->m_code));

		fprintf(stdout, 
			"    m_cxRegisters    = %p",
			static_cast<void*>(shader->m_cxRegisters));

		fprintf(stdout, 
			"    m_shRegisters    = %p",
			static_cast<void*>(shader->m_shRegisters));

		fprintf(stdout, 
			"    m_specials       = %p",
			static_cast<void*>(shader->m_specials));

		fprintf(stdout, 
			"    m_inputSemantics = %p",
			static_cast<void*>(shader->m_inputSemantics));

		fprintf(stdout, 
			"    m_outputSemantics = %p",
			static_cast<void*>(shader->m_outputSemantics));

		//
		// CX registers
		//
		fprintf(stdout, "");
		fprintf(stdout, "  CX Registers (%" PRIu8 ")", shader->m_numCxRegisters);
		fprintf(stdout, "  ------------------------------");

		if (shader->m_cxRegisters)
		{
			for (uint32_t i = 0; i < shader->m_numCxRegisters; ++i)
			{
				const auto& reg = shader->m_cxRegisters[i];

				fprintf(stdout, 
					"    [%3" PRIu32 "] offset=0x%08" PRIx32
					" value=0x%08" PRIx32 "",
					i,
					static_cast<uint32_t>(reg.m_offset),
					reg.m_value);
			}
		}
		else
		{
			fprintf(stdout, "    <null>");
		}

		//
		// SH registers
		//
		fprintf(stdout, "");
		fprintf(stdout, "  SH Registers (%" PRIu8 ")", shader->m_numShRegisters);
		fprintf(stdout, "  ------------------------------");

		if (shader->m_shRegisters)
		{
			for (uint32_t i = 0; i < shader->m_numShRegisters; ++i)
			{
				const auto& reg = shader->m_shRegisters[i];

				fprintf(stdout, 
					"    [%3" PRIu32 "] offset=0x%08" PRIx32
					" value=0x%08" PRIx32 "",
					i,
					static_cast<uint32_t>(reg.m_offset),
					reg.m_value);
			}
		}
		else
		{
			fprintf(stdout, "    <null>");
		}

		//
		// Special registers
		//
		dump_specials(shader->m_specials);

		//
		// Input semantics
		//
		fprintf(stdout, "");
		fprintf(stdout, 
			"  Input Semantics (%" PRIu32 ")",
			shader->m_numInputSemantics);

		fprintf(stdout, "  ------------------------------");

		if (shader->m_inputSemantics)
		{
			for (uint32_t i = 0; i < shader->m_numInputSemantics; ++i)
			{
				dump_semantic(
					shader->m_inputSemantics[i],
					i,
					"input");
			}
		}
		else
		{
			fprintf(stdout, "    <null>");
		}

		//
		// Output semantics
		//
		fprintf(stdout, "");
		fprintf(stdout, 
			"  Output Semantics (%" PRIu16 ")",
			shader->m_numOutputSemantics);

		fprintf(stdout, "  ------------------------------");

		if (shader->m_outputSemantics)
		{
			for (uint32_t i = 0; i < shader->m_numOutputSemantics; ++i)
			{
				dump_semantic(
					shader->m_outputSemantics[i],
					i,
					"output");
			}
		}
		else
		{
			fprintf(stdout, "    <null>");
		}

		//
		// User data
		//
		dump_user_data(shader->m_userData);

		//
		// Optional microcode dump
		//
		if (dump_code)
		{
			fprintf(stdout, "");
			fprintf(stdout, "  Shader Microcode");
			fprintf(stdout, "  ------------------------------");

			if (!shader->m_code)
			{
				fprintf(stdout, "    <null>");
			}
			else
			{
				auto _code = const_cast<void*>(shader->m_code);

				const uint8_t* code =
					reinterpret_cast<uint8_t*>(_code);

				for (uint32_t offset = 0;
					offset < shader->m_shaderSize;
					offset += 16)
				{
					fprintf(stdout, 
						"    %08" PRIx32 ": ",
						offset);

					const uint32_t remaining =
						shader->m_shaderSize - offset;

					const uint32_t count =
						remaining < 16 ? remaining : 16;

					for (uint32_t i = 0; i < count; ++i)
					{
						fprintf(stdout, 
							"%02x ",
							code[offset + i]);
					}

					for (uint32_t i = count; i < 16; ++i)
						fprintf(stdout, "   ");

					fprintf(stdout, " | ");

					for (uint32_t i = 0; i < count; ++i)
					{
						const uint8_t c = code[offset + i];

						fprintf(stdout, 
							"%c",
							(c >= 0x20 && c <= 0x7e) ? c : '.');
					}

					fprintf(stdout, "");
				}
			}
		}

		fprintf(stdout, "");
		fprintf(stdout, "============================================================");
		fprintf(stdout, "End AGC Shader Dump");
		fprintf(stdout, "============================================================");
	}

	[[maybe_unused]] bool write_as_tga(const char* filename, const unsigned char* pixels, uint32_t width, uint32_t height)
	{
#pragma pack(push, 1)
		struct TGAHeader
		{
			uint8_t  idLength;
			uint8_t  colorMapType;
			uint8_t  imageType;
			uint16_t colorMapOrigin;
			uint16_t colorMapLength;
			uint8_t  colorMapDepth;
			uint16_t xOrigin;
			uint16_t yOrigin;
			uint16_t imageWidth;
			uint16_t imageHeight;
			uint8_t  pixelDepth;
			uint8_t  imageDescriptor;
		};
#pragma pack(pop)

		TGAHeader header{};

		header.imageType = 2;				// Uncompressed true-color
		header.imageWidth = (uint16_t)width;
		header.imageHeight = (uint16_t)height;
		header.pixelDepth = 32;				// RGBA
		header.imageDescriptor = 8 | 0x20;	// 8-bit alpha + top-left origin

		FILE* handle = nullptr;

		if (fopen_s(&handle, filename, "wb") != 0 || !handle)
			return false;

		fwrite(&header, sizeof(header), 1, handle);

		// ImGui: RGBA
		// TGA: BGRA
		for (uint32_t y = 0; y < height; ++y)
		{
			for (uint32_t x = 0; x < width; ++x)
			{
				const unsigned char* src = pixels + (y * width + x) * 4;

				unsigned char bgra[4] =
				{
					src[2], // B
					src[1], // G
					src[0], // R
					src[3]  // A
				};

				fwrite(bgra, sizeof(bgra), 1, handle);
			}
		}

		fclose(handle);
		return true;
	};
}
#endif

#include <imgui.h>

#ifndef IMGUI_DISABLE
#include <imgui_internal.h>
#include <stdint.h>				// xxxxx_t
#include <string.h>				// std::memset
#include <vector>				// std::vector

#include <agc.h>
#include <agc/core/texturespec.h>
#include <agc/drawcommandbuffer.h>
#include <agc/gnmp/dataformats.h>

#include <mspace.h>             //
#include <pad.h>				//  
#include <mouse.h>				//  
#include <libime.h>				// 

//
#include "Shader/ImGui_shader_common.h"
#include "imgui_libfont_PlayStation.h"
#include "imgui_impl_PlayStation_Agc.h"

namespace ImGui_PS
{
	namespace EmbeddedShader
	{
#define IMGUI_AGC_DECLARE_SHADER(basename) \
        extern char basename ## _header[]; \
        extern const char basename ## _text[]; \
        sce::Agc::Shader* basename

		IMGUI_AGC_DECLARE_SHADER(Basic_Vertex_Shader);
		IMGUI_AGC_DECLARE_SHADER(Basic_Pixel_Shader);
#undef IMGUI_AGC_DECLARE_SHADER
	}
}

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

static sce::Agc::IndexSize indexSize = []()
{
	sce::Agc::IndexSize indexSize;
	if constexpr (sizeof(ImDrawIdx) == 1)
	{
		indexSize = sce::Agc::IndexSize::k8;
	}
	else if constexpr (sizeof(ImDrawIdx) == 2)
	{
		indexSize = sce::Agc::IndexSize::k16;
	}
	else if constexpr (sizeof(ImDrawIdx) == 4)
	{
		indexSize = sce::Agc::IndexSize::k32;
	}
	else
	{
		static_assert(sizeof(ImDrawIdx) == 1 || sizeof(ImDrawIdx) == 2 || sizeof(ImDrawIdx) == 4, "Unsupported ImDrawIdx size: only 16-bit or 32-bit indices are supported.");
	}

	return indexSize;
}();

struct StaticRenderStates
{
	struct CxRegisters
	{
		sce::Agc::CxBlendControl blendControl;
		sce::Agc::CxPrimitiveSetup primitiveSetup;
		sce::Agc::CxDepthStencilControl depthStencilControl;
		sce::Agc::CxScanModeControl scanModeControl;
		sce::Agc::CxShaderLinkage shaderLinkage;
	};

	struct ShRegisters
	{
	};

	struct UcRegisters
	{
		sce::Agc::UcPrimitiveState shaderLinkage;
	};

	// registers that seem better to be reset
	struct CleanCxRegisters
	{
		sce::Agc::CxBlendControl blendControl;
		sce::Agc::CxScanModeControl scanModeControl;
	};

	sce::Agc::CxRegister* cxRegs;
	sce::Agc::ShRegister* shRegs;
	sce::Agc::UcRegister* ucRegs;
	sce::Agc::CxRegister* cleanCxRegs;
	uint32_t              numCxRegs;
	uint32_t              numShRegs;
	uint32_t              numUcRegs;
	uint32_t              numCleanCxRegs;

	void setRegisters(sce::Agc::DrawCommandBuffer &dcb)
	{
		dcb.setCxRegistersIndirect(cxRegs, numCxRegs);
		dcb.setShRegistersIndirect(shRegs, numShRegs);
		dcb.setUcRegistersIndirect(ucRegs, numUcRegs);
	}

	void setCleanRegisters(sce::Agc::DrawCommandBuffer &dcb)
	{
		dcb.setCxRegistersIndirect(cleanCxRegs, numCleanCxRegs);
	}
};

template <typename CxRegisterType>
inline void setCxRegistersDirect(sce::Agc::DrawCommandBuffer &dcb, const CxRegisterType &reg) 
{
	for (uint32_t i = 0; i < sizeof(CxRegisterType) / sizeof(sce::Agc::CxRegister); ++i)
	{
		dcb.setCxRegisterDirect(reg.m_regs[i]);
	}
}

template <typename UcRegisterType>
inline void setUcRegistersDirect(sce::Agc::DrawCommandBuffer &dcb, const UcRegisterType &reg) 
{
	for (uint32_t i = 0; i < sizeof(UcRegisterType) / sizeof(sce::Agc::UcRegister); ++i)
	{
		dcb.setUcRegisterDirect(reg.m_regs[i]);
	}
}

struct ImGui_ImplPlayStation_RenderBuffers
{
	//
	void*  pConstantBuffer = nullptr;
	void*  pIndexBuffer = nullptr;
	void*  pVertexBuffer = nullptr;
		   
	//	   
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

	// Triple-buffered frame resources.
	static constexpr int kBufferCount = 3;
	ImGui_ImplPlayStation_RenderBuffers frameResources[kBufferCount];
	uint32_t                            activeBufferId = kBufferCount - 1;

	//
	sce::Agc::Core::VertexAttribute* pVertexAttributes = nullptr;
	StaticRenderStates                  renderStates;

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
	io.BackendPlatformUserData = (void*)platformUserData;
	IM_ASSERT(io.BackendRendererUserData == nullptr && "Backend's RenderUserData Already initialized!");
	io.BackendRendererUserData = (void*)renderUserData;

	//
	io.BackendPlatformName = "Prospero";
	io.BackendRendererName = "Agc";
	io.LogFilename = "/data/ImGui_Agc.log";
	io.BackendFlags = (ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_HasGamepad);
	io.ConfigFlags = (ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad /* | ImGuiConfigFlags_IsSRGB */);
	io.MouseDrawCursor = true;
	io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
	
	//
#if _DEBUG
	fprintf(stdout, "Display(W/H): [%d, %d]", width, height);
#endif

	// create fonts atlas.
	{
		//
		sce::Agc::SizeAlign sizeAlign = { 128 * 1024 * 1024, 8 }; // 128 MiB
		platformUserData->fontMemorySpace.MspaceHeapBacking = platformUserData->allocators.garlic.allocate(sizeAlign.m_size, sizeAlign.m_align);
		platformUserData->fontMemorySpace.MspaceHandle = sceLibcMspaceCreate("ImGuiFontSpace", platformUserData->fontMemorySpace.MspaceHeapBacking, sizeAlign.m_size, 0);
		if (platformUserData->fontMemorySpace.MspaceHandle == nullptr)
		{
			fprintf(stdout, "Failed to allocate font memory");
			return false;
		}
		else
		{
			fprintf(stdout, "allocated font memory @ %p", platformUserData->fontMemorySpace.MspaceHandle);
		}

		//
		ImFont* defaultFont = ImGuiLibFont::AddSystemFont(io.Fonts, 12.0f * 1.0f);
		if (defaultFont == nullptr)
		{
			fprintf(stdout, "failed to add default system font.");
			return false;

		}
		else
		{
			fprintf(stdout, "added default system font @ %p", defaultFont);
		}

		//
		int ret = ImGuiLibFont::Initialize();
		if (ret != 0)
		{
			fprintf(stdout, "failed to initialize a imgui font atlas.");
			return false;
		}
		else
		{
			fprintf(stdout, "initialized a imgui font atlas.");
		}

		//
		ImGuiLibFont::BuildFontAtlas(io.Fonts, platformUserData->fontMemorySpace.MspaceHandle);
	}

	// create fonts texture.
	{
		unsigned char* pixels;
		int32_t width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

#if _DEBUG
		write_as_tga("/data/imgui_font.tga", pixels, width, height);
		sceKernelChmod("/data/imgui_font.tga", 00777);
#endif

		//
		PlayStationImage* psImage = platformUserData->allocators.garlic.allocate<PlayStationImage>();
		if (psImage == nullptr)
		{
			fprintf(stdout, "failed to allocate PlayStation Texture Container");
			return false;
		}
		else
		{
			fprintf(stdout, "allocated PlayStation Texture Container @ %p", psImage);
		}

		//
		psImage->texture = platformUserData->allocators.garlic.allocate<sce::Agc::Core::Texture>();
		if (psImage->texture == nullptr)
		{
			fprintf(stdout, "failed to allocate PlayStation Texture");
			return false;
		}
		else
		{
			fprintf(stdout, "allocated PlayStation Texture @ %p", psImage->texture);
		}

		psImage->sampler = platformUserData->allocators.garlic.allocate<sce::Agc::Core::Sampler>();
		if (psImage->sampler == nullptr)
		{
			fprintf(stdout, "failed to allocate PlayStation sampler");
			return false;
		}
		else
		{
			fprintf(stdout, "allocated PlayStation sampler @ %p", psImage->sampler);
		}

		//
		sce::Agc::Core::TextureSpec texSpec;
		texSpec.init();
		texSpec.m_type = sce::Agc::Core::Texture::Type::k2d;
		texSpec.m_width = width;
		texSpec.m_height = height;
		texSpec.m_depth = 1;
		texSpec.m_numMips = 1;
		texSpec.m_format = sce::Agc::Core::DataFormat{ sce::Agc::Core::TypedFormat::k8_8_8_8UNorm, sce::Agc::Core::Swizzle::kRGBA_R4S4 };
		texSpec.m_tileMode = sce::AgcGpuAddress::TileMode::kLinear;

		//
		auto sz = sce::Agc::Core::getSize(&texSpec);
		auto texData = platformUserData->allocators.garlic.allocate(sz.m_size, sz.m_align);
		if (texData == nullptr)
		{
			fprintf(stdout, "failed to allocate garlic memory for texture");
			return false;
		}
		else
		{
			fprintf(stdout, "allocated garlic memory for texture @ %p", texData);
		}

		//
		texSpec.m_dataAddress = texData;

		//
		memcpy((unsigned char*)texSpec.m_dataAddress, pixels, 4 * width * height);
		
		//
		sce::Agc::Core::initialize(psImage->texture, &texSpec);
		psImage->sampler->init().setXyFilterMode(sce::Agc::Core::Sampler::FilterMode::kBilinear);

		//
		io.Fonts->TexID = psImage->TextureID();
	}

	//
	{
		sce::Agc::createShader(&ImGui_PS::EmbeddedShader::Basic_Pixel_Shader, ImGui_PS::EmbeddedShader::Basic_Pixel_Shader_header, ImGui_PS::EmbeddedShader::Basic_Pixel_Shader_text);
		sce::Agc::createShader(&ImGui_PS::EmbeddedShader::Basic_Vertex_Shader, ImGui_PS::EmbeddedShader::Basic_Vertex_Shader_header, ImGui_PS::EmbeddedShader::Basic_Vertex_Shader_text);

		//
		sce::Agc::Shader** shaders[] =
		{
			&ImGui_PS::EmbeddedShader::Basic_Vertex_Shader,
			&ImGui_PS::EmbeddedShader::Basic_Pixel_Shader,
		};

		constexpr uint32_t numShaders = sizeof(shaders) / sizeof(shaders[0]);

		renderUserData->renderStates.numCxRegs = sizeof(StaticRenderStates::CxRegisters) / sizeof(sce::Agc::CxRegister);
		renderUserData->renderStates.numShRegs = sizeof(StaticRenderStates::ShRegisters) / sizeof(sce::Agc::ShRegister);
		renderUserData->renderStates.numUcRegs = sizeof(StaticRenderStates::UcRegisters) / sizeof(sce::Agc::UcRegister);
		renderUserData->renderStates.numCleanCxRegs = sizeof(StaticRenderStates::CleanCxRegisters) / sizeof(sce::Agc::CxRegister);
		
		for (uint32_t i = 0; i < numShaders; ++i)
		{
			sce::Agc::Shader* shader = *shaders[i];
			renderUserData->renderStates.numCxRegs += shader->m_numCxRegisters;
			renderUserData->renderStates.numShRegs += shader->m_numShRegisters;
		}

		renderUserData->renderStates.cxRegs = a_args.allocators.garlic.allocate<sce::Agc::CxRegister>(renderUserData->renderStates.numCxRegs, sce::Agc::Alignment::kRegister);
		renderUserData->renderStates.shRegs = a_args.allocators.garlic.allocate<sce::Agc::ShRegister>(renderUserData->renderStates.numShRegs, sce::Agc::Alignment::kRegister);
		renderUserData->renderStates.ucRegs = a_args.allocators.garlic.allocate<sce::Agc::UcRegister>(renderUserData->renderStates.numUcRegs, sce::Agc::Alignment::kRegister);
		renderUserData->renderStates.cleanCxRegs = a_args.allocators.garlic.allocate<sce::Agc::CxRegister>(renderUserData->renderStates.numCleanCxRegs, sce::Agc::Alignment::kRegister);

		//
		auto cxRegs = (StaticRenderStates::CxRegisters*)renderUserData->renderStates.cxRegs;
		cxRegs->blendControl.init()
		.setBlend(sce::Agc::CxBlendControl::Blend::kEnable)
		.setColorBlendFunc(sce::Agc::CxBlendControl::ColorBlendFunc::kAdd)
		.setColorSourceMultiplier(sce::Agc::CxBlendControl::ColorSourceMultiplier::kSrcAlpha)
		.setColorDestMultiplier(sce::Agc::CxBlendControl::ColorDestMultiplier::kOneMinusSrcAlpha)
		.setAlphaBlendFunc(sce::Agc::CxBlendControl::AlphaBlendFunc::kAdd)
		.setAlphaSourceMultiplier(sce::Agc::CxBlendControl::AlphaSourceMultiplier::kOne)
		.setAlphaDestMultiplier(sce::Agc::CxBlendControl::AlphaDestMultiplier::kOneMinusSrcAlpha)
		.setSlot(0);
		
		cxRegs->primitiveSetup.init()
		.setCullFace(sce::Agc::CxPrimitiveSetup::CullFace::kNone)
		.setPolygonMode(sce::Agc::CxPrimitiveSetup::PolygonMode::kEnable)
		.setFrontPolygonMode(sce::Agc::CxPrimitiveSetup::FrontPolygonMode::kFill)
		.setBackPolygonMode(sce::Agc::CxPrimitiveSetup::BackPolygonMode::kFill);
		
		cxRegs->depthStencilControl.init()
		.setDepth(sce::Agc::CxDepthStencilControl::Depth::kDisable);
		
		cxRegs->scanModeControl.init()
			.setViewportScissor(sce::Agc::CxScanModeControl::ViewportScissor::kEnable);

		//
		auto shaderCxRegs = (sce::Agc::CxRegister*)(cxRegs + 1);
		for (uint32_t i = 0; i < numShaders; ++i)
		{
			sce::Agc::Shader* shader = *shaders[i];
			std::copy_n(shader->m_cxRegisters, shader->m_numCxRegisters, shaderCxRegs);
			shaderCxRegs += shader->m_numCxRegisters;
		}

		//
		auto shaderShRegs = renderUserData->renderStates.shRegs;
		for (uint32_t i = 0; i < numShaders; ++i)
		{
			sce::Agc::Shader* shader = *shaders[i];
			std::copy_n(shader->m_shRegisters, shader->m_numShRegisters, shaderShRegs);
			shaderShRegs += shader->m_numShRegisters;
		}

		//
		auto ucRegs = (StaticRenderStates::UcRegisters*)renderUserData->renderStates.ucRegs;
		sce::Agc::Core::linkShaders
		(
			&cxRegs->shaderLinkage, 
			&ucRegs->shaderLinkage, 
			nullptr,
			ImGui_PS::EmbeddedShader::Basic_Vertex_Shader, 
			ImGui_PS::EmbeddedShader::Basic_Pixel_Shader, 
			sce::Agc::UcPrimitiveType::Type::kTriList
		);

		//
		auto cleanCxRegs = (StaticRenderStates::CleanCxRegisters*)renderUserData->renderStates.cleanCxRegs;
		cleanCxRegs->blendControl.init();
		cleanCxRegs->scanModeControl.init();
	}

	//	
	{
		renderUserData->pVertexAttributes = a_args.allocators.garlic.allocate<sce::Agc::Core::VertexAttribute>(3, sce::Agc::Alignment::kVertexAttribute);

		//
		renderUserData->pVertexAttributes[0] =  sce::Agc::Core::VertexAttribute
		{ 
			0, 
			sce::Agc::Core::VertexAttribute::Format::k32_32Float, 
			offsetof(VS_INPUT, position), 
			sce::Agc::Core::VertexAttribute::Index::kVertexId
		};

		renderUserData->pVertexAttributes[1] = sce::Agc::Core::VertexAttribute
		{ 
			0, 
			sce::Agc::Core::VertexAttribute::Format::k32_32Float, 
			offsetof(VS_INPUT, uv), 
			sce::Agc::Core::VertexAttribute::Index::kVertexId 
		};

		renderUserData->pVertexAttributes[2] = sce::Agc::Core::VertexAttribute
		{ 
			0, 
			sce::Agc::Core::VertexAttribute::Format::k8_8_8_8UNorm, 
			offsetof(VS_INPUT, color), 
			sce::Agc::Core::VertexAttribute::Index::kVertexId 
		};
	}

	// this is retarded, we shouldn't be responsable for this shit
	{
		//
		extern const char __shader_header_start[];
		extern const char __shader_header_end[];
		sceKernelMprotect(__shader_header_start, __shader_header_end - __shader_header_start, SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_READ);

		//
		extern const char __shader_text_start[];
		extern const char __shader_text_end[];
		sceKernelMprotect(__shader_text_start, __shader_text_end - __shader_text_start, SCE_KERNEL_PROT_CPU_READ | SCE_KERNEL_PROT_GPU_READ);
	}

#if _DEBUG
	PRINT_POS;
#endif

	//
	return true;
}

IMGUI_IMPL_API bool ImGui_ImplPlayStation_Init(ImGui_InitUserData& a_args, unsigned int width, unsigned int height)
{
	if (
		a_args.allocators.garlic._instance == nullptr || 
		a_args.allocators.garlic._allocate == nullptr || 
		a_args.allocators.garlic._free == nullptr ||
		a_args.allocators.garlic._instance == nullptr ||
		a_args.allocators.garlic._allocate == nullptr || 
		a_args.allocators.garlic._free == nullptr)
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
	// Each slot remains alive for kBufferCount frames, allowing the GPU to continue consuming the previous frame's buffers while we write into the current slot.
	backendRenderUserData->activeBufferId = (backendRenderUserData->activeBufferId + 1) % ImGui_ImplPlayStation_RendererUserData::kBufferCount;

	//
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

IMGUI_IMPL_API void ImGui_ImplPlayStation_RenderDrawData(ImGuiDrawCommandBuffer& dcb, ImDrawData* draw_data)
{
	// skip render setup if it's not needed.
	if (draw_data->TotalIdxCount == 0 || draw_data->TotalVtxCount == 0)
		return;

	// Avoid rendering when minimized
	if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
		return;
	
	auto* backendUserData = ImGui_ImplPlayStation_GetBackendUserData();
	IM_ASSERT(backendUserData != nullptr && "ImGui_ImplPlayStation_Init() not called");

	auto* backendRenderUserData = ImGui_ImplPlayStation_GetBackendRenderUserData();
	IM_ASSERT(backendRenderUserData != nullptr && "ImGui_ImplPlayStation_Init() not called");

#if _DEBUG
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
	backendRenderUserData->renderStates.setRegisters(dcb);

	//
	auto& frameResources = backendRenderUserData->frameResources[backendRenderUserData->activeBufferId];

	// Ensure the current frame's vertex buffer is large enough.
	if (frameResources.pVertexBuffer == nullptr || frameResources.vertexCapacity < static_cast<uint32_t>(draw_data->TotalVtxCount))
	{
		if (frameResources.pVertexBuffer)
		{
			backendUserData->allocators.garlic.free(frameResources.pVertexBuffer);
		}

		frameResources.vertexCapacity = draw_data->TotalVtxCount;
		frameResources.pVertexBuffer = (void*)backendUserData->allocators.garlic.allocate<ImDrawVert>(frameResources.vertexCapacity);
	}

	// Ensure the current frame's index buffer is large enough.
	if (frameResources.pIndexBuffer == nullptr || frameResources.indexCapacity < static_cast<uint32_t>(draw_data->TotalIdxCount))
	{
		if (frameResources.pIndexBuffer)
		{
			backendUserData->allocators.garlic.free(frameResources.pIndexBuffer);
		}

		frameResources.indexCapacity = draw_data->TotalIdxCount;
		frameResources.pIndexBuffer = (void*)backendUserData->allocators.garlic.allocate<ImDrawIdx>(frameResources.indexCapacity);
	}

	// Constant buffer is always exactly one matrix.
	if (frameResources.pConstantBuffer == nullptr)
	{
		frameResources.pConstantBuffer = (void*)backendUserData->allocators.garlic.allocate<float4x4>(1);
	}

	//
	[[maybe_unused]] void* cBufferData = frameResources.pConstantBuffer;
	[[maybe_unused]] void* vertexData = frameResources.pVertexBuffer;
	[[maybe_unused]] void* indexData = frameResources.pIndexBuffer;


	// Setup Index/vertexBuffer/constantBuffer structures
	sce::Agc::Core::Buffer vertexBuffer, constantBuffer;

	// Upload vertex/index data into a single contiguous GPU buffer + Build projection matrix
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
		memcpy(cBufferData, orthoProjMatrix, sizeof(orthoProjMatrix));

		//
		sce::Agc::Core::BufferSpec vertexBufferSpec;
		vertexBufferSpec.initAsRegularBuffer(vertexData, sizeof(ImDrawVert), draw_data->TotalVtxCount);
		sce::Agc::Core::initialize(&vertexBuffer, &vertexBufferSpec);

		//
		sce::Agc::Core::BufferSpec constantBufferSpec;
		constantBufferSpec.initAsConstantBuffer(cBufferData, sizeof(float4x4));
		sce::Agc::Core::initialize(&constantBuffer, &constantBufferSpec);
	}

	//
	sce::Agc::Core::Binder binder;
	binder.init(&dcb, &dcb);
	binder.setStageActive(sce::Agc::ShaderType::kGs, true);
	binder.setStageActive(sce::Agc::ShaderType::kPs, true);
	binder.setShaders(nullptr, ImGui_PS::EmbeddedShader::Basic_Vertex_Shader, ImGui_PS::EmbeddedShader::Basic_Pixel_Shader);

	//
	auto& binderStage = binder.getStage(sce::Agc::ShaderType::kGs);
	binderStage.setVertexAttributeTable(backendRenderUserData->pVertexAttributes);
	binderStage.setVertexBuffers(0, 1, &vertexBuffer);
	binderStage.setConstantBuffers(0, 1, &constantBuffer);

	//
	dcb.setIndexSize(indexSize, sce::Agc::GeCachePolicy::kBypass);
	dcb.setIndexCount(draw_data->TotalIdxCount);
	dcb.setIndexBuffer(indexData);
	dcb.setNumInstances(1);

	//
	ImGuiIO& io = ImGui::GetIO();
	int fb_width = (int)(draw_data->DisplaySize.x * io.DisplayFramebufferScale.x);
	int fb_height = (int)(draw_data->DisplaySize.y * io.DisplayFramebufferScale.y);

	// Setup viewport once.
	// The viewport transform is shared by all ImGui draw commands;
	// only the scissor rectangle changes per command.
	sce::Agc::CxViewport viewportScissor;
	viewportScissor.init()
		.setSlot(0)
		.setScaleZ(0.5f)
		.setOffsetZ(0.5f)
		.setMinZ(0.0f)
		.setMaxZ(1.0f)
		.setWindowOffset(sce::Agc::CxViewport::WindowOffset::kDisable)
		.setScaleX(fb_width * 0.5f)
		.setScaleY(-fb_height * 0.5f)
		.setOffsetX(fb_width * 0.5f)
		.setOffsetY(fb_height * 0.5f);

	// Draw
	uint32_t indexBufferOffset = 0;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		//
		const ImDrawList* cmd_list = draw_data->CmdLists[n];

#if _DEBUG
#else
		backendRenderUserData->info.totalCmds += cmd_list->CmdBuffer.Size;
#endif

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
		 
			// Project scissor/clipping rectangles into framebuffer space.
			ImVec2 clip_min(
				(pcmd->ClipRect.x - draw_data->DisplayPos.x) * io.DisplayFramebufferScale.x,
				(pcmd->ClipRect.y - draw_data->DisplayPos.y) * io.DisplayFramebufferScale.y);

			ImVec2 clip_max(
				(pcmd->ClipRect.z - draw_data->DisplayPos.x) * io.DisplayFramebufferScale.x,
				(pcmd->ClipRect.w - draw_data->DisplayPos.y) * io.DisplayFramebufferScale.y);

			// Clamp to framebuffer.
			clip_min.x = (ImMax)(clip_min.x, 0.0f);
			clip_min.y = (ImMax)(clip_min.y, 0.0f);
			clip_max.x = (ImMin)(clip_max.x, (float)fb_width);
			clip_max.y = (ImMin)(clip_max.y, (float)fb_height);

			if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
				continue;

			// Update scissor for this command.
			viewportScissor
				.setLeft((uint32_t)clip_min.x)
				.setTop((uint32_t)clip_min.y)
				.setRight((uint32_t)clip_max.x)
				.setBottom((uint32_t)clip_max.y);

			// Emit viewport + scissor state.
			setCxRegistersDirect(dcb, viewportScissor);

			//
			const PlayStationImage* psImage = (PlayStationImage*)pcmd->TextureId;
			if (psImage)
			{
				auto& ps = binder.getStage(sce::Agc::ShaderType::kPs);
				ps.setTextures(0, 1, psImage->texture);
				ps.setSamplers(0, 1, psImage->sampler);
			}

			//
			const uint32_t startIndex = indexBufferOffset + pcmd->IdxOffset;
			dcb.setIndexBuffer(indexData);
			dcb.drawIndexOffset(startIndex, pcmd->ElemCount, ImGui_PS::EmbeddedShader::Basic_Vertex_Shader->m_specials->m_drawModifier);
			binder.postDraw();
		}

		indexBufferOffset += cmd_list->IdxBuffer.size();
	}

	//
	backendRenderUserData->renderStates.setCleanRegisters(dcb);

#ifdef _DEBUG
#else
	backendRenderUserData->info.deltaTime = ImGui::GetIO().DeltaTime;
	backendRenderUserData->info.frameCount++;
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