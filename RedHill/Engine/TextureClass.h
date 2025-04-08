#pragma once

#include <d3d11.h>
#include <cstdio>

class TextureClass
{
private:

	struct TargaHeader
	{
		unsigned char dat1[12];
		unsigned short width;
		unsigned short height;
		unsigned char bpp;
		unsigned char data2;
	};

public:

	TextureClass();
	TextureClass(const TextureClass& other);
	~TextureClass();

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, char* filename);
	void Shutdown();

	ID3D11ShaderResourceView* GetTexture() const;

	int GetWidth() const;
	int GetHeight() const;

private:

	bool LoadTarga32Bit(char*);

private:

	unsigned char* m_targaData;
	ID3D11Texture2D* m_texture;
	ID3D11ShaderResourceView* m_textureView;
	int m_width;
	int m_height;

};

