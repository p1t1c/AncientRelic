#include "Graphics\window.h"
#include "Camera\camera.h"
#include "Shaders\shader.h"
#include "Model Loading\mesh.h"
#include "Model Loading\texture.h"
#include "Model Loading\meshLoaderObj.h"
#include <cmath>

bool chestShaking = true;
glm::vec3 chestBasePos(0.0f, -18.0f, -140.0f);   // same as your chest position

void processKeyboardInput ();

float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;

Window window("Game Engine", 800, 800);
Camera camera;

glm::vec3 lightColor = glm::vec3(1.0f);
glm::vec3 lightPos = glm::vec3(-180.0f, 100.0f, -200.0f);

int main()
{
	glClearColor(0.2f, 0.8f, 1.0f, 1.0f);

	//building and compiling shader program
	Shader shader("Shaders/vertex_shader.glsl", "Shaders/fragment_shader.glsl");
	Shader sunShader("Shaders/sun_vertex_shader.glsl", "Shaders/sun_fragment_shader.glsl");

	//Textures
	GLuint tex = loadBMP("Resources/Textures/wood.bmp");
	GLuint tex2 = loadBMP("Resources/Textures/rock.bmp");
	GLuint tex3 = loadBMP("Resources/Textures/orange.bmp");

	glEnable(GL_DEPTH_TEST);

	//Test custom mesh loading
	std::vector<Vertex> vert;
	vert.push_back(Vertex());
	vert[0].pos = glm::vec3(10.5f, 10.5f, 0.0f);
	vert[0].textureCoords = glm::vec2(1.0f, 1.0f);

	vert.push_back(Vertex());
	vert[1].pos = glm::vec3(10.5f, -10.5f, 0.0f);
	vert[1].textureCoords = glm::vec2(1.0f, 0.0f);

	vert.push_back(Vertex());
	vert[2].pos = glm::vec3(-10.5f, -10.5f, 0.0f);
	vert[2].textureCoords = glm::vec2(0.0f, 0.0f);

	vert.push_back(Vertex());
	vert[3].pos = glm::vec3(-10.5f, 10.5f, 0.0f);
	vert[3].textureCoords = glm::vec2(0.0f, 1.0f);

	vert[0].normals = glm::normalize(glm::cross(vert[1].pos - vert[0].pos, vert[3].pos - vert[0].pos));
	vert[1].normals = glm::normalize(glm::cross(vert[2].pos - vert[1].pos, vert[0].pos - vert[1].pos));
	vert[2].normals = glm::normalize(glm::cross(vert[3].pos - vert[2].pos, vert[1].pos - vert[2].pos));
	vert[3].normals = glm::normalize(glm::cross(vert[0].pos - vert[3].pos, vert[2].pos - vert[3].pos));

	std::vector<int> ind = { 0, 1, 3,   
		1, 2, 3 };

	std::vector<Texture> textures;
	textures.push_back(Texture());
	textures[0].id = tex;
	textures[0].type = "texture_diffuse";

	std::vector<Texture> textures2;
	textures2.push_back(Texture());
	textures2[0].id = tex2;
	textures2[0].type = "texture_diffuse";

	std::vector<Texture> textures3;
	textures3.push_back(Texture());
	textures3[0].id = tex3;
	textures3[0].type = "texture_diffuse";


	Mesh mesh(vert, ind, textures3);

	// Create Obj files - easier :)
	// we can add here our textures :)
	MeshLoaderObj loader;
	Mesh sun = loader.loadObj("Resources/Models/sphere.obj");
	Mesh box = loader.loadObj("Resources/Models/cube.obj", textures);
	Mesh plane = loader.loadObj("Resources/Models/plane.obj", textures3);

	//check if we close the window or press the escape button
	while (!window.isPressed(GLFW_KEY_ESCAPE) &&
		glfwWindowShouldClose(window.getWindow()) == 0)
	{
		window.clear();
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		processKeyboardInput();
		// --- Stop chest shaking when pressing E near it (one press = one action) ---
		static bool ePrevDown = false;
		bool eDown = window.isPressed(GLFW_KEY_E);

		if (eDown && !ePrevDown) // key JUST pressed
		{
			float distToChest = glm::length(camera.getCameraPosition() - chestBasePos);
			if (distToChest < 200.0f)
				// interaction radius (tweak)
			{
				chestShaking = false;  // stop shaking
			}
		}
		ePrevDown = eDown;


		//test mouse input
		if (window.isMousePressed(GLFW_MOUSE_BUTTON_LEFT))
		{
			std::cout << "Pressing mouse button" << std::endl;
		}
		 //// Code for the light ////

		sunShader.use();

		glm::mat4 ProjectionMatrix = glm::perspective(90.0f, window.getWidth() * 1.0f / window.getHeight(), 0.1f, 10000.0f);
		glm::mat4 ViewMatrix = glm::lookAt(camera.getCameraPosition(), camera.getCameraPosition() + camera.getCameraViewDirection(), camera.getCameraUp());

		GLuint MatrixID = glGetUniformLocation(sunShader.getId(), "MVP");

		//Test for one Obj loading = light source

		glm::mat4 ModelMatrix = glm::mat4(1.0);
		ModelMatrix = glm::translate(ModelMatrix, lightPos);
		glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

		sun.draw(sunShader);

		//// End code for the light ////

		shader.use();

		///// Test Obj files for box ////

		GLuint MatrixID2 = glGetUniformLocation(shader.getId(), "MVP");
		GLuint ModelMatrixID = glGetUniformLocation(shader.getId(), "model");
		glUniform3f(glGetUniformLocation(shader.getId(), "lightColor"), lightColor.x, lightColor.y, lightColor.z);
		glUniform3f(glGetUniformLocation(shader.getId(), "lightPos"), lightPos.x, lightPos.y, lightPos.z);
		glUniform3f(glGetUniformLocation(shader.getId(), "viewPos"),
			camera.getCameraPosition().x, camera.getCameraPosition().y, camera.getCameraPosition().z);


		///// Test plane Obj file //////

		ModelMatrix = glm::mat4(1.0);
		ModelMatrix = glm::translate(ModelMatrix, glm::vec3(0.0f, -20.0f, 0.0f));
		MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
		glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);

		plane.draw(shader);
		// ===== Portal LEFT =====
		ModelMatrix = glm::mat4(1.0f);
		// position (left)
		ModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-100.0f, -5.0f, -80.0f));
		ModelMatrix = glm::scale(ModelMatrix, glm::vec3(8.0f, 18.0f, 0.5f));

		MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
		glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);

		box.draw(shader);


		// ===== Portal RIGHT =====
		ModelMatrix = glm::mat4(1.0f);
		// position (right)
		ModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, -5.0f, -80.0f));
		ModelMatrix = glm::scale(ModelMatrix, glm::vec3(8.0f, 18.0f, 0.5f));

		MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
		glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);

		box.draw(shader);
		// ===== 8 Columns: 4 on LEFT, 4 on RIGHT =====
		float colXLeft = -180.0f;
		float colXRight = 180.0f;
		float colY = -5.0f;

		float zStart = -40.0f;
		float zStep = 120.0f;   // increase for more space

		float z0 = zStart + 0 * -zStep;
		float z1 = zStart + 1 * -zStep;
		float z2 = zStart + 2 * -zStep;
		float z3 = zStart + 3 * -zStep;



		glm::vec3 colScale(12.0f, 45.0f, 12.0f);

		auto drawColumn = [&](float x, float z)
			{
				ModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(x, colY, z));
				ModelMatrix = glm::scale(ModelMatrix, colScale);

				MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
				glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
				glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);

				box.draw(shader);
			};

		// Left side (4)
		drawColumn(colXLeft, z0);
		drawColumn(colXLeft, z1);
		drawColumn(colXLeft, z2);
		drawColumn(colXLeft, z3);

		// Right side (4)
		drawColumn(colXRight, z0);
		drawColumn(colXRight, z1);
		drawColumn(colXRight, z2);
		drawColumn(colXRight, z3);


		// ===== Side Columns (extreme left/right) =====

// LEFT column
		ModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-180.0f, -5.0f, -120.0f));
		ModelMatrix = glm::scale(ModelMatrix, glm::vec3(12.0f, 45.0f, 12.0f));
		MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
		glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
		box.draw(shader);

		// RIGHT column
		ModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(180.0f, -5.0f, -120.0f));
		ModelMatrix = glm::scale(ModelMatrix, glm::vec3(12.0f, 45.0f, 12.0f));
		MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
		glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
		box.draw(shader);
		// ===== Side Columns (extreme left/right) =====

// LEFT column
		ModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-180.0f, -5.0f, -120.0f));
		ModelMatrix = glm::scale(ModelMatrix, glm::vec3(12.0f, 45.0f, 12.0f));
		MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
		glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
		box.draw(shader);

		// RIGHT column
		ModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(180.0f, -5.0f, -120.0f));
		ModelMatrix = glm::scale(ModelMatrix, glm::vec3(12.0f, 45.0f, 12.0f));
		MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
		glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
		box.draw(shader);
		
		// ===== Rocks (small boxes) =====
		auto drawBox = [&](glm::vec3 pos, glm::vec3 scale)
			{
				ModelMatrix = glm::translate(glm::mat4(1.0f), pos);
				ModelMatrix = glm::scale(ModelMatrix, scale);
				MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
				glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
				glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
				box.draw(shader);
			};

		drawBox(glm::vec3(-40.0f, -18.0f, -60.0f), glm::vec3(6.0f, 4.0f, 6.0f));
		drawBox(glm::vec3(20.0f, -18.5f, -90.0f), glm::vec3(4.0f, 3.0f, 5.0f));
		drawBox(glm::vec3(55.0f, -18.0f, -70.0f), glm::vec3(5.0f, 4.0f, 4.0f));
		drawBox(glm::vec3(-75.0f, -18.5f, -110.0f), glm::vec3(4.0f, 2.5f, 4.0f));
		drawBox(glm::vec3(90.0f, -18.0f, -130.0f), glm::vec3(7.0f, 3.5f, 5.0f));
		
		// ===== Chest (bigger box) =====
// ===== Chest (bigger box) - SHAKING =====
		float t = glfwGetTime();

		float shakeSpeed = 8.0f;
		float shakeAmpX = 1.2f;
		float shakeAmpZ = 0.8f;
		float shakeAmpY = 0.4f;

		glm::vec3 chestScale(12.0f, 8.0f, 8.0f);

		glm::vec3 shakeOffset(0.0f);
		if (chestShaking)
		{
			shakeOffset = glm::vec3(
				shakeAmpX * std::sin(t * shakeSpeed),
				shakeAmpY * std::sin(t * shakeSpeed * 1.7f),
				shakeAmpZ * std::cos(t * shakeSpeed)
			);
		}

		ModelMatrix = glm::translate(glm::mat4(1.0f), chestBasePos + shakeOffset);
		ModelMatrix = glm::scale(ModelMatrix, chestScale);

		// uniforms + draw like you already do
		MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
		glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
		box.draw(shader);



		window.update();
	}
}

void processKeyboardInput()
{
	float cameraSpeed = 30 * deltaTime;

	//translation
	if (window.isPressed(GLFW_KEY_W))
		camera.keyboardMoveFront(cameraSpeed);
	if (window.isPressed(GLFW_KEY_S))
		camera.keyboardMoveBack(cameraSpeed);
	if (window.isPressed(GLFW_KEY_A))
		camera.keyboardMoveLeft(cameraSpeed);
	if (window.isPressed(GLFW_KEY_D))
		camera.keyboardMoveRight(cameraSpeed);
	if (window.isPressed(GLFW_KEY_R))
		camera.keyboardMoveUp(cameraSpeed);
	if (window.isPressed(GLFW_KEY_F))
		camera.keyboardMoveDown(cameraSpeed);

	//rotation
	if (window.isPressed(GLFW_KEY_LEFT))
		camera.rotateOy(cameraSpeed);
	if (window.isPressed(GLFW_KEY_RIGHT))
		camera.rotateOy(-cameraSpeed);
	if (window.isPressed(GLFW_KEY_UP))
		camera.rotateOx(cameraSpeed);
	if (window.isPressed(GLFW_KEY_DOWN))
		camera.rotateOx(-cameraSpeed);
}
