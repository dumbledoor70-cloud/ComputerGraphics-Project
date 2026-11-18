#define STB_IMAGE_IMPLEMENTATION
#include<iostream>
#include<gl/glew.h>
#include<gl/glfw3.h>
#include"Texture.h"

Texture::Texture(std::string_view file) : filename{ file.data() }
{
	glGenTextures(1, &texture);

	unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrChannels, 0);
	if (data) {
		GLenum format = GL_RGB;
		if (nrChannels == 4)
			format = GL_RGBA;
		glBindTexture(GL_TEXTURE_2D, texture);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // 밉맵 사용 시 추천
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else {
		std::cout << "텍스처 로드 실패: " << filename << std::endl;
		std::cerr << "[ERROR] Texture load failed: " << file.data()
			<< " | Reason: " << stbi_failure_reason() << std::endl;
	}
	stbi_image_free(data);
}
Texture& Texture::operator=(Texture&& other) noexcept {
	if (this != &other) {
		glDeleteTextures(1, &texture);
		texture = other.texture;
		other.texture = 0; // 임시 객체 소멸 시 삭제 방지
	}
	return *this;
}

void Texture::BindTexture() const {
	glBindTexture(GL_TEXTURE_2D, texture);
}