#pragma once
#include<string>
#include<fstream>
#include<D3Dcompiler.h>
#include<dxgi1_6.h>
#include<d3d12.h>
#include<wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace DXUtil {
	ComPtr<ID3DBlob> CompileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint,
		const std::string& target
		);
};