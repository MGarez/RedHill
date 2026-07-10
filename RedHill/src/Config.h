#pragma once

namespace RHConfig
{
	static constexpr wchar_t className[] = L"RedHill Window Class";
	static constexpr wchar_t windowName[] = L"RedHill";
	static constexpr uint32_t frameNumber = 2;
	static constexpr uint32_t width = 1280;
	static constexpr uint32_t height = 720;
	static constexpr uint32_t gbuffersNumber = 3; // Albedo + normal + material
	static constexpr uint32_t environmentsNumber = 4;
	static constexpr uint32_t shadowMapSize = 2048;
}