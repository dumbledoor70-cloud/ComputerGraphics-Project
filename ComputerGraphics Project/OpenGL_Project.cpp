#include "OpenGL.h"
#include"Shader.h"
#include"Camera.h"
#include"Mesh.h"
#define STB_IMAGE_IMPLEMENTATION
#include<stb_image.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "assimp-vc145-mt.lib")
#pragma comment(lib, "freetype.lib")

struct Texture {
	Texture() = default;
	Texture(std::string_view file) : filename{file.data()} 
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
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;
	Texture& operator=(Texture&& other) noexcept {
		if (this != &other) {
			glDeleteTextures(1, &texture);
			texture = other.texture;
			other.texture = 0; // 임시 객체 소멸 시 삭제 방지
		}
		return *this;
	}
	~Texture() { glDeleteTextures(1, &texture); }

	void BindTexture() const {
		glBindTexture(GL_TEXTURE_2D, texture);
	}

	std::string filename;
	int width = 0;
	int height = 0;
	int nrChannels = 0;
	unsigned int texture = 0;
};

struct Character {
	GLuint TextureID;       // GL_RGBA 텍스처 ID
	glm::ivec2 Size;        // 글리프 크기 (가로, 세로)
	glm::ivec2 Bearing;     // 기준선(Baseline)으로부터의 오프셋 (좌측, 상단)
	unsigned int Advance;   // 다음 글자까지의 거리 (1/64 픽셀 단위)
};
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

class Model {
public:
	Model() = default;
	Model(std::string_view filename, glm::vec3 pos = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 sz = glm::vec3(1.0f), glm::vec3 Color = glm::vec3(0.5f)) : 
		mesh{}, center{ pos }, color{Color}, size{sz},
		pitch{}, yaw{}, roll{}
	{
		mesh.LoadModel(filename);
	}

	void draw(const Shader& shader) const {
		glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(GLfloat), mesh.vertices.data(), GL_STATIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(GLuint), mesh.indices.data(), GL_STATIC_DRAW);

		glUniform3fv(glGetUniformLocation(shader.GetShaderID(), "outColor"), 1, glm::value_ptr(color));
		glUniformMatrix4fv(glGetUniformLocation(shader.GetShaderID(), "world"), 1, GL_FALSE, glm::value_ptr(getWorldMat()));

		glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
	}

	void move(glm::vec3 delta) { center += delta; }
	glm::vec3 getPos() const { return center; }
	void SetPos(glm::vec3 pos) { center = pos; }

	glm::vec3 getsize() const { return size; }

	void rotatePitch(GLfloat delta) { pitch += delta; }
	GLfloat getPitch() const { return pitch; }
	void SetPitch(GLfloat newPth) { pitch = newPth; }

	void rotateYaw(GLfloat delta) { yaw += delta; }
	GLfloat getYaw() const { return yaw; }
	void SetYaw(GLfloat newYaw) { yaw = newYaw; }
		 
private:
 	glm::mat4 getWorldMat() const {
		glm::mat4 worldMat{ glm::mat4(1.0f) };


		worldMat = glm::translate(worldMat, center);
		worldMat = glm::rotate(worldMat, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
		worldMat = glm::rotate(worldMat, pitch, glm::vec3(1.0f, 0.0f, 0.0f));
		worldMat = glm::scale(worldMat, size);

		return worldMat;
	}
	Mesh mesh;
	glm::vec3 center;
	glm::vec3 color;
	glm::vec3 size;
	GLfloat pitch, yaw, roll;
};

class Bullet : public Model {
public:
	Bullet(glm::vec3 pos, glm::vec3 dir)
		: Model("object\\Sphere.obj", pos, glm::vec3(0.05f), glm::vec3(0.7f)), direct{dir}
	{

	}
	void update() {
		move(direct * 0.5f);
	}
private:
	glm::vec3 direct;

};

namespace {
	Shader shader;
	Texture ak47Text, floorText, bulletText;

	bool timer = true;
	double fps = 60;
	double dtime = 1 / fps;

	bool isPerspective = true;

	glm::vec3 lightPos = glm::vec3(0.0f, 0.0f, 0.0f);

	Model ak47;
	glm::vec3 aniPos = glm::vec3();
	glm::vec3 pos = glm::vec3(-0.8, -0.6, 1.6);
	int magazine = 30; // 잔탄 수

	// 마우스 커서 움직임
	bool MouseEntered = false;
	double lastX, lastY;

	float frontState, leftState;
	constexpr float meterpersec = 1.44f;

	// 점프 구현하기
	bool isJump = false;
	float delta = 0.0f;

	// 총 애니메이션
	bool shooting = false; // 총 사격모션
	bool reload = false; // 총 장전 모션
	float reloadTime = 3.0f; // 장전 시간
	float reloadFirstTime = 0.0f;

	std::vector<Bullet> bullets;
}

namespace {
	void drawFloor(const Shader& shader, float size = 50.0f) {
		if (size < 50.f) size = 50.f;
		static glm::vec3 color = glm::vec3(0.0f, 1.0f, 0.0f);
		GLfloat vertices[] = {
			size, 0.0f, size, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
			size, 0.0f, -size, 0.0f, 1.0f, 0.0f, size / 16, 0.0f,
			-size, 0.0f, -size, 0.0f, 1.0f, 0.0f, size / 16, size / 16,
			-size, 0.0f, size, 0.0f, 1.0f, 0.0f, 0.0f, size / 16,
		};
		GLuint indices[] = {
			0,1,2,
			0,2,3,
		};
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

		glUniform3fv(glGetUniformLocation(shader.GetShaderID(), "outColor"), 1, glm::value_ptr(color));
		glUniformMatrix4fv(glGetUniformLocation(shader.GetShaderID(), "world"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));

		glDrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(GLuint), GL_UNSIGNED_INT, 0);
	}

	glm::vec3 getAk47pos(glm::vec3 eye, glm::vec3 delta = glm::vec3()) {
		glm::mat4 mat = glm::mat4(1.0f);
		mat = glm::rotate(mat, ak47.getYaw(), glm::vec3(0.0f, 1.0f, 0.0f));
		mat = glm::rotate(mat, ak47.getPitch(), glm::vec3(1.0f, 0.0f, 0.0f));
		return eye + glm::vec3(mat * glm::vec4(pos + aniPos + delta, 1.0f));
	}

	std::u32string ascii_to_u32(const std::string& ascii) {
		return std::u32string(ascii.begin(), ascii.end()); // 아스키에서 u32로
	}
}

void OpenGL::SetBuffer()
{
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

	
}

void OpenGL::startPractice()
{
	shader = Shader("vertex.glsl", "fragment.glsl");
	ak47Text = Texture("image\\ak47.png");
	floorText = Texture("image\\stonefloor.png");
	bulletText = Texture("image\\bullet.png");
	init("C:\\Windows\\Fonts\\HMKMRHD.TTF");

	EnableState(GL_DEPTH_TEST);
	EnableState(GL_CULL_FACE);
	pCamera = std::make_unique<Camera>(Camera(glm::vec3(0.0, 4.0, 0.0)));

	lightPos = glm::vec3(0.0f, 5.0f, 0.0f);
	ak47 = Model("Object\\ak47.obj", pCamera->eye + pos, glm::vec3(1.0f));
}

void OpenGL::mainloop() {
	while (!glfwWindowShouldClose(window)) {
		if (timer) Timer(dtime);
		update();
		rendering();

		glfwPollEvents();
	}
}


// 렌더링 함수
void OpenGL::rendering() {
	glClearColor(0.5f, 0.5f, 0.9f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	shader.UseProgram();
	glActiveTexture(GL_TEXTURE0);

	glUniform3fv(glGetUniformLocation(shader.GetShaderID(), "viewPos"), 1, glm::value_ptr(pCamera->eye));
	glUniform3fv(glGetUniformLocation(shader.GetShaderID(), "lightColor"), 1, glm::value_ptr(glm::vec3(1.0f)));
	glUniform3fv(glGetUniformLocation(shader.GetShaderID(), "lightPos"), 1, glm::value_ptr(lightPos));
	glUniform1i(glGetUniformLocation(shader.GetShaderID(), "isText"), false);

	glUniformMatrix4fv(glGetUniformLocation(shader.GetShaderID(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));
	glUniformMatrix4fv(glGetUniformLocation(shader.GetShaderID(), "view"), 1, GL_FALSE, glm::value_ptr(pCamera->getViewMatrix()));

	// 그리기
	floorText.BindTexture();
	drawFloor(shader, 1024);


	ak47Text.BindTexture();
	ak47.draw(shader);

	bulletText.BindTexture();
	for (const auto& bullet : bullets) bullet.draw(shader);

	glUniform1i(glGetUniformLocation(shader.GetShaderID(), "ourTexture"), 0);

	glUniformMatrix4fv(glGetUniformLocation(shader.GetShaderID(), "projection"), 1, GL_FALSE, glm::value_ptr(ortho));
	glUniformMatrix4fv(glGetUniformLocation(shader.GetShaderID(), "view"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
	glUniformMatrix4fv(glGetUniformLocation(shader.GetShaderID(), "world"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
	// 2D UI, 텍스트 출력

	glDisable(GL_DEPTH_TEST);

	if(reload) RenderText(shader, ascii_to_u32("reloading..."), 0, 50, 0.5, glm::vec3(1.0f, 1.0f, 1.0f));
	RenderText(shader, ascii_to_u32(std::to_string(magazine)), 0, 0, 1, glm::vec3(1.0f, 1.0f, 1.0f));


	glEnable(GL_DEPTH_TEST);
	glfwSwapBuffers(window);
}

void OpenGL::update() {
	// ak47를 화면에 고정시키기
	ak47.SetPos(getAk47pos(pCamera->eye));
}

void OpenGL::Timer(double time)
{
	static GLdouble fir = glfwGetTime(), sec;

	sec = glfwGetTime();
	if (sec - fir >= time) {
		fir = sec;
		// 작업 처리	
		auto front = pCamera->getFront();
		pCamera->eye += frontState * glm::vec3(front.x, 0, front.z);
		front = glm::rotate(glm::mat4(1.0f), glm::radians(90.f), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::vec4(front, 1.0f);
		pCamera->eye += leftState * glm::vec3(front.x, 0, front.z);

		if (shooting and not reload and magazine > 0 and glm::vec3() == aniPos) {
			magazine -= 1;
			aniPos = glm::vec3(0.0f, 0.0f, -0.3f);
			bullets.push_back(Bullet(getAk47pos(pCamera->eye, glm::vec3(0.0f, 0.255f, 2.5f)), pCamera->getFront()));
		}
		else {
			aniPos += glm::vec3(0.0f, 0.0f, 0.05f);
			if (aniPos.z >= 0.0f) aniPos = glm::vec3();
		}
		if (isJump) {
			pCamera->eye += glm::vec3(0.0f, glm::cos(delta), 0.0f);
			delta += glm::radians(5.0f);

			if (delta >= glm::radians(180.0f)) {
				pCamera->eye = glm::vec3(pCamera->eye.x, 4.0f, pCamera->eye.z);
				isJump = false;
			}
		}
		if (bullets.size() > 0) {

			for (int i = 0; i < bullets.size(); ++i) {
				bullets[i].update();
				glm::vec3 pos = bullets[i].getPos();
				if (abs(pos.x) >= 1024.f or
					abs(pos.y) >= 100.f or
					abs(pos.z) >= 1024.f) {
					bullets.erase(bullets.begin() + i--);
				}
			}
		}
		if (reload) {
			if (sec - reloadFirstTime >= reloadTime) {
				reload = false;
				magazine = 30;
			}
		}
		// std::cout << "(" << pCamera->eye.x << ", " << pCamera->eye.y << ", " << pCamera->eye.z << ")\n";
	}
}

// 콜백 함수
void OpenGL::keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	switch (key) {
	case GLFW_KEY_Q:
		glfwSetWindowShouldClose(window, true);
		break;
	case GLFW_KEY_ESCAPE:
		if (action == GLFW_RELEASE) return;
		if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			MouseEntered = false;
		}
		else {
			glfwSetWindowShouldClose(window, true);
		}
		break;
	case GLFW_KEY_R:
		if (not reload and magazine < 30)
		{
			reload = true;
			reloadFirstTime = glfwGetTime();
		}
		break;
	case GLFW_KEY_W:
		if (action == GLFW_PRESS)
			frontState += 1.0f;
		if (action == GLFW_RELEASE)
			frontState -= 1.0f;
			
		break;
	case GLFW_KEY_A:
		if (action == GLFW_PRESS)
			leftState += meterpersec;
		if (action == GLFW_RELEASE)
			leftState -= meterpersec;
		break;
	case GLFW_KEY_S:
		if (action == GLFW_PRESS)
			frontState -= meterpersec;
		if (action == GLFW_RELEASE)
			frontState += meterpersec;
		break;
	case GLFW_KEY_D:
		if (action == GLFW_PRESS)
			leftState -= meterpersec;
		if (action == GLFW_RELEASE)
			leftState += meterpersec;
		break;
	case GLFW_KEY_F1:
		pCamera->move(glm::vec3(0.1f, 0.0f, 0.0f));
		break;
	case GLFW_KEY_F2:
		pCamera->move(glm::vec3(-0.1f, 0.0f, 0.0f));
		break;
	case GLFW_KEY_F3:
		pCamera->move(glm::vec3(0.0f, -0.1f, 0.0f));
		break;
	case GLFW_KEY_F4:
		pCamera->move(glm::vec3(0.0f, 0.1f, 0.0f));
		break;
	case GLFW_KEY_F5:
		pCamera->move(glm::vec3(0.0f, 0.0f, -0.1f));
		break;
	case GLFW_KEY_F6:
		pCamera->move(glm::vec3(0.0f, 0.0f, 0.1f));
		break;
	case GLFW_KEY_F7:
		pCamera->RotateYaw(glm::radians(-1.0f));
		break;
	case GLFW_KEY_F8:
		pCamera->RotateYaw(glm::radians(1.0f));
		break;
	}

}
void OpenGL::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) 
{
	switch (button) {
	case GLFW_MOUSE_BUTTON_LEFT:
		if (action == GLFW_PRESS) {
			if (MouseEntered) {
				shooting = true;
			}

			if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL) {
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				glfwGetCursorPos(window, &lastX, &lastY);
				MouseEntered = true;
			}

		}
		else {
			shooting = false;
		}
		break;
	}
}
void OpenGL::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) 
{
	if (MouseEntered) {
		auto dirX = xpos - lastX;
		auto dirY = ypos - lastY;


		float sensitivity = 0.003f;
		pCamera->yaw += sensitivity  * -dirX;
		pCamera->pitch += sensitivity * dirY;

		if (pCamera->pitch > glm::radians(89.0f)) pCamera->pitch = glm::radians(89.0f);
		if (pCamera->pitch < glm::radians(-89.0f)) pCamera->pitch = glm::radians(-89.0f);

		ak47.SetPitch(pCamera->pitch);
		ak47.SetYaw(pCamera->yaw);
		lastX = xpos, lastY = ypos;
	}
}
void OpenGL::CursorEnterCallback(GLFWwindow* window, int entered)
{
	
}
void OpenGL::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {}

void OpenGL::CharCallback(GLFWwindow* window, unsigned int codepoint)
{
	switch (codepoint) {

	case ' ':
		if (not isJump) {
			isJump = true;
			delta = 0.0f;
		}
		break;
	}
}
void OpenGL::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {}
void OpenGL::windowSizeCallback(GLFWwindow* window, int width, int height) { SetViewport(0, 0, width, height); }




