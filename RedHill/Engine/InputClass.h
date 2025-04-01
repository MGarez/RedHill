#pragma once

// Temporal Input class
// #TODO: it should be updated to use DirectInput
class InputClass
{
public:

	InputClass();
	InputClass(const InputClass& other);
	~InputClass();

	void Initialize();
	void KeyDown(unsigned int input);
	void KeyUp(unsigned int input);

	bool IsKeyDown(unsigned int key);

private:

	bool m_keys[256];
};

