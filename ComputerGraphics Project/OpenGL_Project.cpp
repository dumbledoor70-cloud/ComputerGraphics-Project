#include "OpenGL.h"
#include"Shader.h"
#include"Camera.h"
#include"Mesh.h"
#define STB_IMAGE_IMPLEMENTATION
#include<stb_image.h>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "assimp-vc145-mt.lib")

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
		}
		stbi_image_free(data);
	}

	void BindTexture() const {
		glBindTexture(GL_TEXTURE_2D, texture);
	}

	std::string filename;
	int width = 0;
	int height = 0;
	int nrChannels = 0;
	unsigned int texture = 0;
};

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
		: Model("object\\Sphere.obj", pos, glm::vec3(0.02f), glm::vec3(0.7f)), direct{dir}
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
	Texture ak47Text, floorText;

	bool timer = true;
	double fps = 60;
	double dtime = 1 / fps;

	bool isPerspective = true;

	glm::vec3 lightPos = glm::vec3(0.0f, 0.0f, 0.0f);

	Model ak47;
	glm::vec3 aniPos = glm::vec3();
	glm::vec3 pos = glm::vec3(-0.466667, -0.4, 0.8);

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

	EnableState(GL_DEPTH_TEST);
	EnableState(GL_CULL_FACE);
	pCamera = std::make_unique<Camera>(Camera(glm::vec3(0.0, 2.0, 0.0)));

	lightPos = glm::vec3(0.0f, 5.0f, 0.0f);
	ak47 = Model("Object\\ak47.obj", pCamera->eye + pos, glm::vec3(0.5f));
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
	glUniform1i(glGetUniformLocation(shader.GetShaderID(), "ourTexture"), 0);
	
	glUniform3fv(glGetUniformLocation(shader.GetShaderID(), "viewPos"), 1, glm::value_ptr(pCamera->eye));
	glUniform3fv(glGetUniformLocation(shader.GetShaderID(), "lightColor"), 1, glm::value_ptr(glm::vec3(1.0f)));
	glUniform3fv(glGetUniformLocation(shader.GetShaderID(), "lightPos"), 1, glm::value_ptr(lightPos));

	glUniformMatrix4fv(glGetUniformLocation(shader.GetShaderID(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));
	glUniformMatrix4fv(glGetUniformLocation(shader.GetShaderID(), "view"), 1, GL_FALSE, glm::value_ptr(pCamera->getViewMatrix()));

	// 그리기
	floorText.BindTexture();
	drawFloor(shader, 1024);


	ak47Text.BindTexture();
	ak47.draw(shader);

	for (const auto& bullet : bullets) bullet.draw(shader);

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

		if (shooting and glm::vec3() == aniPos) {
			aniPos = glm::vec3(0.0f, 0.0f, -0.3f);
			bullets.push_back(Bullet(getAk47pos(pCamera->eye, glm::vec3(0.0f, 0.13f, 0.0f)), pCamera->getFront()));
		}
		else {
			aniPos += glm::vec3(0.0f, 0.0f, 0.05f);
			

			if (aniPos.z >= 0.0f) aniPos = glm::vec3();
		}
		if (isJump) {
			pCamera->eye += glm::vec3(0.0f, glm::cos(delta), 0.0f);
			delta += glm::radians(5.0f);

			if (delta >= glm::radians(180.0f)) {
				pCamera->eye = glm::vec3(pCamera->eye.x, 2.0f, pCamera->eye.z);
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

		// std::cout << "(" << pCamera->eye.x << ", " << pCamera->eye.y << ", " << pCamera->eye.z << ")\n";
	}
}

void OpenGL::UpdateProjection()
{
	projection = isPerspective ? pCamera->getPerspectiveMatrix(width / (float)height, Znear, Zfar) : pCamera->getOrthographicMatrix(-1., 1, -0.75,0.75, 0.1, 100);
}

// 콜백 함수
void OpenGL::keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	switch (key) {
	case GLFW_KEY_ESCAPE:
		if (action == GLFW_RELEASE) return;
		if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			MouseEntered = false;
		}
		else {
			glfwSetWindowShouldClose(window, true);
			std::cout << pCamera->eye.x << ", " << pCamera->eye.y << ", " << pCamera->eye.z << '\n';
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
	case 'q':
		glfwSetWindowShouldClose(window, true);
		std::cout << "(" << pCamera->eye.x << ", " << pCamera->eye.y << ", " << pCamera->eye.z << ")\n";
		std::cout << pCamera->yaw << '\n';
		break;
	case 'p':
		isPerspective ^= 1;
		UpdateProjection();
		break;
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




