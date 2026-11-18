#include<iostream>
#include<map>
#include<string>
#include<gl/glew.h>
#include<gl/glfw3.h>
#include<gl/glm/glm.hpp> 
#include<gl/glm/gtc/type_ptr.hpp>
#include<gl/glm/gtc/matrix_transform.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include"Shader.h"
#include"Character.h"

std::map<char32_t, Character> Characters;

FT_Library ft;
FT_Face face;
int init(std::string_view path) {
	if (FT_Init_FreeType(&ft)) {
		std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
		return -1;
	}
	// Windows 기준 맑은 고딕 폰트 경로 예시
	if (FT_New_Face(ft, path.data(), 0, &face)) {
		std::cerr << "ERROR::FREETYPE: Failed to load font" << std::endl;
		return -1;
	}

	FT_Set_Pixel_Sizes(face, 0, 50);
	return 0;
}
Character GetOrLoadCharacter(char32_t c) {
	// 이미 맵에 캐싱되어 있다면 즉시 반환
	if (Characters.find(c) != Characters.end()) {
		return Characters[c];
	}

	// FreeType으로 글리프 렌더링 (8비트 그레이스케일)
	if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
		std::cerr << "ERROR::FREETYPE: Failed to load Glyph" << std::endl;
		return { 0, {0, 0}, {0, 0}, 0 };
	}

	int width = face->glyph->bitmap.width;
	int height = face->glyph->bitmap.rows;
	unsigned char* srcBuffer = face->glyph->bitmap.buffer;

	GLuint texture = 0;
	if (width > 0 && height > 0) {
		// 8비트 그레이스케일 -> 32비트 RGBA 변환 (R=255, G=255, B=255, A=비트맵값)
		std::vector<unsigned char> rgbaBuffer(width * height * 4, 255);
		for (int i = 0; i < width * height; ++i) {
			rgbaBuffer[i * 4 + 3] = srcBuffer[i];
		}

		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaBuffer.data());

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	Character character = {
		texture,
		glm::ivec2(width, height),
		glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
		static_cast<unsigned int>(face->glyph->advance.x)
	};

	// 맵에 저장 후 반환
	Characters[c] = character;
	return character;
}
void RenderText(const Shader& shader, const std::u32string& text, float x, float y, float scale, glm::vec3 color) {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUniform3fv(glGetUniformLocation(shader.GetShaderID(), "outColor"), 1, glm::value_ptr(color));
	glUniform1i(glGetUniformLocation(shader.GetShaderID(), "isText"), true);
	glActiveTexture(GL_TEXTURE0);

	for (char32_t c : text) {
		Character ch = GetOrLoadCharacter(c);

		float xpos = x + ch.Bearing.x * scale;
		float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;
		float w = ch.Size.x * scale;
		float h = ch.Size.y * scale;

		// 공백 문자가 아닐 때만 사각형 그리기
		if (ch.TextureID != 0 && w > 0 && h > 0) {
			float vertices[] = {
				 xpos,     ypos + h,0.0f, 0.0f,0.0f,0.0f, 0.0f,0.0f,
				 xpos,     ypos,    0.0f, 0.0f,0.0f,0.0f, 0.0f,1.0f,
				 xpos + w, ypos,    0.0f, 0.0f,0.0f,0.0f, 1.0f,1.0f,

				 xpos,     ypos + h, 0.0f, 0.0f,0.0f,0.0f, 0.0f,0.0f,
				 xpos + w, ypos,     0.0f, 0.0f,0.0f,0.0f, 1.0f,1.0f,
				 xpos + w, ypos + h, 0.0f, 0.0f,0.0f,0.0f, 1.0f,0.0f
			};

			glBindTexture(GL_TEXTURE_2D, ch.TextureID);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		// 다음 글자 위치로 커서 이동
		x += (ch.Advance >> 6) * scale;
	}
	glDisable(GL_BLEND);
}
