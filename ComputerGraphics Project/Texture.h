#pragma once
#include<stb_image.h>

struct Texture {
	Texture() = default;
	Texture(std::string_view file);
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;
	Texture& operator=(Texture&& other) noexcept;
	~Texture() { glDeleteTextures(1, &texture); }

	void BindTexture() const;

	std::string filename;
	int width = 0;
	int height = 0;
	int nrChannels = 0;
	unsigned int texture = 0;
};
