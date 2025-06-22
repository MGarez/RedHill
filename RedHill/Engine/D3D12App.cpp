#include "D3D12App.h"
#include "Common.h"
#include "DXHelper.h"

/*

Initialize the pipeline:

	Enable the debug layer
	Create the device
	Create the command queue
	Create the swap chain
	Create a render target view descriptor heap
	Create frame resources (a render target view for each frame)
	Create a command allocator

Initialize the assets:

	Create an empty root signature
	Compile the shaders
	Create the vertex input layout
	Create a pipeline state object description, then the object
	Create the command list
	Close the command list
	Create and load the vertex buffers
	Create the vertex buffer views
	Create a fence
	Create an event handle
	Wait for the GPU to finish

Update:

	Modify the constant, vertex, index buffers and everything else as necessary

Render:

	Populate the command list
		Reset the command list allocator
		Reset the command list
		Set the graphics root signature
		Set the viewport and scissor rectangles
		Set a resource barrier, indicating the back buffer is to be used as a render target
		Record commands into the command list
		Indicate the back buffer will be used to present after the command list has executed
		Close the command list to further recording
	Execute the command list
	Present the frame
	Wait for the GPU to finish

Destroy:

	Wait for the GPU to finish
	Close the event handle

*/

D3D12App::D3D12App(UINT width, UINT height, std::wstring name)
	:DXSample(width, height, name),
	m_frameIndex(0),
	m_rtvDescriptorSize(0)
{
}

void D3D12App::OnInit()
{
	InitPipeline();
	InitAssets();
}

void D3D12App::OnUpdate()
{
	// Modify the constant, vertex, index buffers and everything else as necessary
}

void D3D12App::OnRender()
{
	// Populate the command list
	// Present the frame
	// Wait for the GPU to finish
}

void D3D12App::OnDestroy()
{
	// Wait for the GPU to finish
	// Close the event handle
}

void D3D12App::InitPipeline()
{
	// Enable the debug layer
	// Create the device
	// Create the command queue
	// Create the swap chain
	// Create a render target view descriptor heap
	// Create frame resources (a render target view for each frame)
	// Create a command allocator
}

void D3D12App::InitAssets()
{
	// Create an empty root signature
	// Compile the shaders
	// Create the vertex input layout
	// Create a pipeline state object description, then the object
	// Create the command list
	// Close the command list
	// Create and load the vertex buffers
	// Create the vertex buffer views
	// Create a fence
	// Create an event handle
	// Wait for the GPU to finish
}

void D3D12App::PopulateCommandList()
{
	// Reset the command list allocator
	// Reset the command list
	// Set the graphics root signature
	// Set the viewport and scissor rectangles
	// Set a resource barrier, indicating the back buffer is to be used as a render target
	// Record commands into the command list
	// Indicate the back buffer will be used to present after the command list has executed
	// Close the command list to further recording
	// Execute the command list
}

void D3D12App::WaitForPreviousFrame()
{
}
