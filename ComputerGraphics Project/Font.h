#pragma once

struct Character {
	GLuint TextureID;       // GL_RGBA 텍스처 ID
	glm::ivec2 Size;        // 글리프 크기 (가로, 세로)
	glm::ivec2 Bearing;     // 기준선(Baseline)으로부터의 오프셋 (좌측, 상단)
	unsigned int Advance;   // 다음 글자까지의 거리 (1/64 픽셀 단위)
};

class Font {
public:
	Font() = default;
	Font(const Font&) = delete;
	Font& operator=(const Font&) = delete;
	Font& operator=(Font&& other) noexcept;
	Font(std::string_view path);
	~Font();

	void RenderText(const Shader& shader, const std::u32string& text, float x, float y, float scale, glm::vec3 color);
private:
	void init(std::string_view path);

	Character GetOrLoadCharacter(char32_t c);

	FT_Library ft = nullptr;
	FT_Face face = nullptr;
	std::string fPath;
	std::map<char32_t, Character> Characters;
};


