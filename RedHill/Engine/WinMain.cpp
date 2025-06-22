#include "D3D12App.h"
#include "Win32Application.h"
_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	D3D12App sample(1280, 720, L"D3D12 Hello Window");
	return Win32Application::Run(&sample, hInstance, nCmdShow);
}
