#include <Windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#include "../renderer/uber_shaders.h"


#define PS(Data) #Data,
const char* uberShaderPixelStates[] =
{
	UBER_SHADER_PS_LIST(PS)
};
#undef PS


// -Zi embeds debug info
// -Qembed_debug embeds debug info in shader container
// -Vn header variable name
// -WX warnings as errors
// -O3 or -O0 optimization level
// -Wno-warning disables the warning


const char* va(const char* format, ...)
{
	static char string[64][32000];
	static int index = 0;
	char* buf = string[index++ & 63];
	va_list argptr;

	va_start(argptr, format);
	vsprintf(buf, format, argptr);
	va_end(argptr);

	return buf;
}


struct ShaderArgs
{
	const char* headerPath;
	const char* shaderPath;
	const char* entryPoint;
	const char* targetProfile;
};

void CompileShader(const ShaderArgs& args, int extraCount = 0, const char** extras = NULL)
{
	static char temp[4096];

	// -Ges: Enable strict mode
	// -Gis: Force IEEE strictness
	// -Zi: Embed debug info
	// -Qembed_debug: Embed debug info in shader container
	strcpy(temp, va("dxc.exe -Fh %s -E %s -T %s -WX -Ges -Gis -Zi -Qembed_debug",
		args.headerPath, args.entryPoint, args.targetProfile));

	for(int i = 0; i < extraCount; ++i)
	{
		strcat(temp, " ");
		strcat(temp, extras[i]);
	}

	strcat(temp, " ");
	strcat(temp, args.shaderPath);

	printf("%s\n", temp);
	system(temp);
}

struct SMAAArgs
{
	const char* headerPath;
	const char* shaderPath;
	const char* presetMacro;
	const char* variableName;
	bool vertexShader;
};

void CompileSMAAShader(const SMAAArgs& smaaArgs)
{
	const char* extras[] =
	{
		"-Vn", smaaArgs.variableName,
		smaaArgs.presetMacro,
		smaaArgs.vertexShader ? "-D SMAA_INCLUDE_VS=1" : "-D SMAA_INCLUDE_PS=1",
		"-D SMAA_HLSL_5_1=1",
		"-D SMAA_RT_METRICS=rtMetrics"
	};

	ShaderArgs args;
	args.entryPoint = smaaArgs.vertexShader ? "vs" : "ps";
	args.headerPath = smaaArgs.headerPath;
	args.shaderPath = smaaArgs.shaderPath;
	args.targetProfile = smaaArgs.vertexShader ? "vs_6_0" : "ps_6_0";
	CompileShader(args, _countof(extras), extras);
}

void ProcessSMAAShadersForPreset(const char* presetName, const char* presetMacro)
{
	SMAAArgs args;
	args.presetMacro = presetMacro;
	for(int pass = 0; pass < 3; ++pass)
	{
		for(int ps = 0; ps < 2; ++ps)
		{
			args.headerPath = va("smaa_%s_%d_%s.h", presetName, pass + 1, ps ? "ps" : "vs");
			args.shaderPath = va("smaa_%d.hlsl", pass + 1);
			args.variableName = va("%s_%d_%s", presetName, pass + 1, ps ? "ps" : "vs");
			args.vertexShader = ps == 0;
			CompileSMAAShader(args);
		}
	}
}

void CompileSMAAShaders()
{
	ProcessSMAAShadersForPreset("low", "-D SMAA_PRESET_LOW=1");
	ProcessSMAAShadersForPreset("medium", "-D SMAA_PRESET_MEDIUM=1");
	ProcessSMAAShadersForPreset("high", "-D SMAA_PRESET_HIGH=1");
	ProcessSMAAShadersForPreset("ultra", "-D SMAA_PRESET_ULTRA=1");
}

void CompileVS(const char* headerPath, const char* shaderPath)
{
	const char* extras[] = { "-D VERTEX_SHADER=1" };

	ShaderArgs args;
	args.entryPoint = "vs";
	args.headerPath = headerPath;
	args.shaderPath = shaderPath;
	args.targetProfile = "vs_6_0";
	CompileShader(args, _countof(extras), extras);
}

void CompilePS(const char* headerPath, const char* shaderPath)
{
	const char* extras[] = { "-D PIXEL_SHADER=1" };

	ShaderArgs args;
	args.entryPoint = "ps";
	args.headerPath = headerPath;
	args.shaderPath = shaderPath;
	args.targetProfile = "ps_6_0";
	CompileShader(args, _countof(extras), extras);
}

void CompileCS(const char* headerPath, const char* shaderPath)
{
	const char* extras[] = { "-D COMPUTE_SHADER=1" };

	ShaderArgs args;
	args.entryPoint = "cs";
	args.headerPath = headerPath;
	args.shaderPath = shaderPath;
	args.targetProfile = "cs_6_0";
	CompileShader(args, _countof(extras), extras);
}

void CompileVSAndPS(const char* headerPathPrefix, const char* shaderPath)
{
	CompileVS(va("%s_vs.h", headerPathPrefix), shaderPath);
	CompilePS(va("%s_ps.h", headerPathPrefix), shaderPath);
}

void CompileUberVS(const char* headerPath, const char* shaderPath, int stageCount)
{
	const char* extras[] =
	{
		"-D VERTEX_SHADER=1",
		va("-D STAGE_COUNT=%d", stageCount),
		va("-Vn g_vs_%d", stageCount)
	};

	ShaderArgs args;
	args.entryPoint = "vs";
	args.headerPath = headerPath;
	args.shaderPath = shaderPath;
	args.targetProfile = "vs_6_0";
	CompileShader(args, _countof(extras), extras);
}

void CompileUberPS(const char* stateString)
{
	UberPixelShaderState state;
	if(!ParseUberPixelShaderState(state, stateString))
	{
		fprintf(stderr, "ParseUberPixelShaderState failed!\n");
		exit(666);
	}

	const char* extras[16];
	int extraCount = 0;
	extras[extraCount++] = va("-Vn g_ps_%s", stateString);
	extras[extraCount++] = "-D USE_INCLUDES=1";
	extras[extraCount++] = "-D PIXEL_SHADER=1";
	if(state.globalState & UBERPS_DITHER_BIT)
	{
		extras[extraCount++] = "-D DITHER=1";
	}
	if(state.globalState & UBERPS_DEPTHFADE_BIT)
	{
		extras[extraCount++] = "-D DEPTH_FADE=1";
	}
	extras[extraCount++] = va("-D STAGE_COUNT=%d", state.stageCount);
	for(int s = 0; s < state.stageCount; ++s)
	{
		extras[extraCount++] = va("-D STAGE%d_BITS=0x%X", s, state.stageStates[s]);
	}

	ShaderArgs args;
	args.entryPoint = "ps";
	args.headerPath = va("uber_shader_ps_%s.h", stateString);
	args.shaderPath = "uber_shader.hlsl";
	args.targetProfile = "ps_6_0";
	CompileShader(args, extraCount, extras);
}

int main(int /*argc*/, const char** argv)
{
	char dirPath[MAX_PATH];
	strcpy(dirPath, argv[0]);
	int l = strlen(dirPath);
	while(l-- > 0)
	{
		if(dirPath[l] == '/' || dirPath[l] == '\\')
		{
			dirPath[l] = '\0';
			break;
		}
	}
	SetCurrentDirectoryA(dirPath);

	system("del *.h");

	CompileVSAndPS("post_gamma", "post_gamma.hlsl");
	CompileVSAndPS("post_inverse_gamma", "post_inverse_gamma.hlsl");
	CompileVSAndPS("imgui", "imgui.hlsl");
	CompileVSAndPS("nuklear", "nuklear.hlsl");
	CompileVSAndPS("ui", "ui.hlsl");
	CompileVSAndPS("depth_pre_pass", "depth_pre_pass.hlsl");
	CompileVSAndPS("dynamic_light", "dynamic_light.hlsl");
	CompileVS("fog_vs.h", "fog_inside.hlsl");
	CompilePS("fog_inside_ps.h", "fog_inside.hlsl");
	CompilePS("fog_outside_ps.h", "fog_outside.hlsl");
	CompileCS("mip_1_cs.h", "mip_1.hlsl");
	CompileCS("mip_2_cs.h", "mip_2.hlsl");
	CompileCS("mip_3_cs.h", "mip_3.hlsl");

	CompileSMAAShaders();
	system("type smaa*.h > complete_smaa.h");

	system("type shared.hlsli uber_shader.hlsl > uber_shader.temp"); // combines both files into one
	system("..\\..\\..\\tools\\bin2header.exe --output uber_shader.h --hname uber_shader_string uber_shader.temp");
	system("del uber_shader.temp");

	for(int i = 0; i < 8; ++i)
	{
		CompileUberVS(va("uber_shader_vs_%i.h", i + 1), "uber_shader.hlsl", i + 1);
	}
	system("type uber_shader_vs_*.h > complete_uber_vs.h");
	system("del uber_shader_vs_*.h");

	for(int i = 0; i < _countof(uberShaderPixelStates); ++i)
	{
		CompileUberPS(uberShaderPixelStates[i]);
	}
	system("type uber_shader_ps_*.h > complete_uber_ps.h");
	system("del uber_shader_ps_*.h");

	return 0;
}
