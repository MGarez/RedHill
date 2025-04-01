#include "SystemClass.h"

int WINAPI wWinMain(HINSTANCE hInstance,  HINSTANCE hPrevInstance,  PWSTR pCmdLine,  int nShowCmd)
{
	SystemClass* System;
	
	// Create the system object
	System = new SystemClass;
	
	// Initialize and run the System
	if (System->Initialize())
	{
		System->Run();
	}

	// Shutdown and release the system object
	System->Shutdown();
	delete System;
	System = nullptr;

	return 0;
}