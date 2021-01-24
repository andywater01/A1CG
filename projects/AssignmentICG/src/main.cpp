#include <Logging.h>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <json.hpp>
#include <fstream>

#include <Attributes.h>

#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>

#include "Graphics/IndexBuffer.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/VertexArrayObject.h"
#include "Graphics/Shader.h"
#include "Graphics/TextureCubeMap.h"
#include "Graphics/TextureCubeMapData.h"
#include "Gameplay/ShaderMaterial.h"
#include "Gameplay/IBehaviour.h"

#include "Gameplay/Camera.h"
#include "Gameplay/Scene.h"
#include "Gameplay/Application.h"
#include "Gameplay/RendererComponent.h"
#include "Gameplay/GameObjectTag.h"
#include "Gameplay/Texture.h"

#include "Behaviours/SimpleMoveBehaviour.h"

#include <Windows.h>
#include <MMSystem.h>

#pragma comment(lib, "winmm.lib")

#include <fmod.h>



#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Gameplay/Transform.h"
#include "Graphics/Texture2D.h"
#include "Graphics/Texture2DData.h"
#include "Utilities/InputHelpers.h"
#include "Utilities/MeshBuilder.h"
#include "Utilities/MeshFactory.h"
#include "Utilities/NotObjLoader.h"
#include "Utilities/ObjLoader.h"
#include "MorphLoader.h"
#include "MorphRenderer.h"
#include "Utilities/VertexTypes.h"
#include "bullet/btBulletCollisionCommon.h"
#include "bullet/btBulletDynamicsCommon.h"


#define LOG_GL_NOTIFICATIONS

/*
	Handles debug messages from OpenGL
	https://www.khronos.org/opengl/wiki/Debug_Output#Message_Components
	@param source    Which part of OpenGL dispatched the message
	@param type      The type of message (ex: error, performance issues, deprecated behavior)
	@param id        The ID of the error or message (to distinguish between different types of errors, like nullref or index out of range)
	@param severity  The severity of the message (from High to Notification)
	@param length    The length of the message
	@param message   The human readable message from OpenGL
	@param userParam The pointer we set with glDebugMessageCallback (should be the game pointer)
*/
void GlDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
	std::string sourceTxt;
	switch (source) {
	case GL_DEBUG_SOURCE_API: sourceTxt = "DEBUG"; break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM: sourceTxt = "WINDOW"; break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceTxt = "SHADER"; break;
	case GL_DEBUG_SOURCE_THIRD_PARTY: sourceTxt = "THIRD PARTY"; break;
	case GL_DEBUG_SOURCE_APPLICATION: sourceTxt = "APP"; break;
	case GL_DEBUG_SOURCE_OTHER: default: sourceTxt = "OTHER"; break;
	}
	switch (severity) {
	case GL_DEBUG_SEVERITY_LOW:          LOG_INFO("[{}] {}", sourceTxt, message); break;
	case GL_DEBUG_SEVERITY_MEDIUM:       LOG_WARN("[{}] {}", sourceTxt, message); break;
	case GL_DEBUG_SEVERITY_HIGH:         LOG_ERROR("[{}] {}", sourceTxt, message); break;
		#ifdef LOG_GL_NOTIFICATIONS
	case GL_DEBUG_SEVERITY_NOTIFICATION: LOG_INFO("[{}] {}", sourceTxt, message); break;
		#endif
	default: break;
	}
}

GLFWwindow* window;
Camera::sptr camera = nullptr;

Attributes PhantomAttrib;
bool isRotate = true;
bool isLeft = true;
int CoinCount = 0;

//Scene number
int RenderGroupBool = 0;
int PlayerHealth = 3;

bool canMoveForward = true;
bool canMoveLeft = true;
bool canMoveBack = true;
bool canMoveRight = true;



void GlfwWindowResizedCallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
	camera->ResizeWindow(width, height);
}

bool initGLFW() {
	if (glfwInit() == GLFW_FALSE) {
		LOG_ERROR("Failed to initialize GLFW");
		return false;
	}

#ifdef _DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif
	
	//Create a new GLFW window
	window = glfwCreateWindow(800, 800, "Skybound", nullptr, nullptr);
	glfwMakeContextCurrent(window);

	// Set our window resized callback
	glfwSetWindowSizeCallback(window, GlfwWindowResizedCallback);

	return true;
}

bool initGLAD() {
	if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) {
		LOG_ERROR("Failed to initialize Glad");
		return false;
	}
	return true;
}

void InitImGui() {
	// Creates a new ImGUI context
	ImGui::CreateContext();
	// Gets our ImGUI input/output 
	ImGuiIO& io = ImGui::GetIO();
	// Enable keyboard navigation
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	// Allow docking to our window
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// Allow multiple viewports (so we can drag ImGui off our window)
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	// Allow our viewports to use transparent backbuffers
	io.ConfigFlags |= ImGuiConfigFlags_TransparentBackbuffers;

	// Set up the ImGui implementation for OpenGL
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 410");

	// Dark mode FTW
	ImGui::StyleColorsDark();

	// Get our imgui style
	ImGuiStyle& style = ImGui::GetStyle();
	//style.Alpha = 1.0f;
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 0.8f;
	}
}

void ShutdownImGui()
{
	// Cleanup the ImGui implementation
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	// Destroy our ImGui context
	ImGui::DestroyContext();
}

std::vector<std::function<void()>> imGuiCallbacks;
void RenderImGui() {
	// Implementation new frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	// ImGui context new frame
	ImGui::NewFrame();

	if (ImGui::Begin("Debug")) {
		// Render our GUI stuff
		for (auto& func : imGuiCallbacks) {
			func();
		}
		ImGui::End();
	}
	
	// Make sure ImGui knows how big our window is
	ImGuiIO& io = ImGui::GetIO();
	int width{ 0 }, height{ 0 };
	glfwGetWindowSize(window, &width, &height);
	io.DisplaySize = ImVec2((float)width, (float)height);

	// Render all of our ImGui elements
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// If we have multiple viewports enabled (can drag into a new window)
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		// Update the windows that ImGui is using
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		// Restore our gl context
		glfwMakeContextCurrent(window);
	}
}

void RenderVAO(
	const Shader::sptr& shader,
	const VertexArrayObject::sptr& vao,
	const glm::mat4& viewProjection,
	const Transform& transform)
{
	shader->SetUniformMatrix("u_ModelViewProjection", viewProjection * transform.LocalTransform());
	shader->SetUniformMatrix("u_Model", transform.LocalTransform());
	shader->SetUniformMatrix("u_NormalMatrix", transform.NormalMatrix());
	vao->Render();
}

void ManipulateTransformWithInput(Transform& transform, float dt) {
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		transform.MoveLocal(0.0f, -1.0f * dt, 0.0f);
		camera->SetPosition(glm::vec3(0.0f, -1.0f * dt, 0.0f));
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { 
		transform.MoveLocal(0.0f,  1.0f * dt, 0.0f);
		camera->SetPosition(glm::vec3(0.0f, 1.0f * dt, 0.0f));
	}
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		transform.MoveLocal(-1.0f * dt, 0.0f, 0.0f);
		camera->SetPosition(glm::vec3(-1.0f * dt, 0.0f, 0.0f));
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		transform.MoveLocal( 1.0f * dt, 0.0f,  0.0f);
		camera->SetPosition(glm::vec3(1.0f * dt, 0.0f, 0.0f));
	}
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		transform.MoveLocal(0.0f, 0.0f,  1.0f * dt);
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
		transform.MoveLocal(0.0f, 0.0f, -1.0f * dt);
	}

	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) { 
		transform.RotateLocal(0.0f, -45.0f * dt, 0.0f);
	}
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
		transform.RotateLocal(0.0f,  45.0f * dt,0.0f);
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
		transform.RotateLocal( 45.0f * dt, 0.0f,0.0f);
	}
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
		transform.RotateLocal(-45.0f * dt, 0.0f, 0.0f);
	}
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
		transform.RotateLocal(0.0f, 0.0f, 45.0f * dt);
	}
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
		transform.RotateLocal(0.0f, 0.0f, -45.0f * dt);
	}
}


inline btVector3 glm2bt(const glm::vec3& vec)
{
	return { vec.x, vec.y, vec.z };
}

int firstFrame = 0;
int lastFrame = 4;


void PlayerInput(GameObject& transform, float dt, float speed) {
if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS && canMoveLeft == true && RenderGroupBool != 0) {
	//transform->MoveLocal(0.0f, 0.0f, -1.0f * dt * speed);
	//transform.MoveLocalFixed(0.0f, -1.0f * dt * speed, 0.0f);
	transform.get<Transform>().SetLocalPosition(transform.get<Transform>().GetLocalPosition() + glm::vec3(0.0f, -1.0f * dt * speed, 0.0f));
	transform.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
	firstFrame = 0;
	lastFrame = 4;
	//transform.SetLocalRotation(90.0f, 0.0f, 282.0f);
}
if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS && canMoveRight == true && RenderGroupBool != 0) {
	transform.get<Transform>().SetLocalPosition(transform.get<Transform>().GetLocalPosition() + glm::vec3(0.0f, 1.0f * dt * speed, 0.0f));
	transform.get<Transform>().SetLocalRotation(90.0f, 0.0f, 270.0f);
	firstFrame = 0;
	lastFrame = 4;
	//body->activate(true);
	//body->setLinearVelocity(btVector3(0, 4, 0));
	//body->applyForce(btVector3(0, 1000, 0), btVector3(0, 1000, 0));
	//transform.MoveLocalFixed(0.0f, 1.0f * dt * speed, 0.0f);
	//transform.SetLocalRotation(90.0f, 0.0f, 102.0f);
}
if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS && canMoveForward == true && RenderGroupBool != 0) {
	//transform->MoveLocal(1.0f * dt * speed, 0.0f, 0.0f);
	//transform.MoveLocalFixed(-1.0f * dt * speed, 0.0f, 0.0f);
	transform.get<Transform>().SetLocalPosition(transform.get<Transform>().GetLocalPosition() + glm::vec3(-1.0f * dt * speed, 0.0f, 0.0f));
	transform.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
	firstFrame = 0;
	lastFrame = 4;
	//transform.SetLocalRotation(90.0f, 0.0f, 192.0f);
	//camera->SetPosition(camera->GetPosition() + glm::vec3(-1.0f, 0.0f, 0.0f) * dt);
}
if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && canMoveBack == true && RenderGroupBool != 0) {
	//transform->MoveLocal(-1.0f * dt * speed, 0.0f, 0.0f);
	//transform.MoveLocalFixed(1.0f * dt * speed, 0.0f, 0.0f);

	transform.get<Transform>().SetLocalPosition(transform.get<Transform>().GetLocalPosition() + glm::vec3(1.0f * dt * speed, 0.0f, 0.0f));
	transform.get<Transform>().SetLocalRotation(90.0f, 0.0f, 180.0f);
	firstFrame = 0;
	lastFrame = 4;
	//transform.SetLocalRotation(90.0f, 0.0f, 12.0f);
	//camera->SetPosition(camera->GetPosition() + glm::vec3(1.0f, 0.0f, 0.0f) * dt);
}
if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && RenderGroupBool != 0) {
	//transform.MoveLocal(0.0f, 0.0f, 1.0f * dt);
}
if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS && RenderGroupBool != 0) {
	//transform.MoveLocal(0.0f, 0.0f, -1.0f * dt);
}



else 
{
	firstFrame = 5;
	lastFrame = 7;
}

//body->setWorldTransform(phyTransform);
}







struct Material
{
	Texture2D::sptr Albedo;
	Texture2D::sptr Specular;
	Texture2D::sptr NewTexture;

	float           Shininess;
	float			TextureMix;
	float           OutlineThickness;
};

void SetupShaderForFrame(const Shader::sptr& shader, const glm::mat4& view, const glm::mat4& projection) {
	shader->Bind();
	// These are the uniforms that update only once per frame
	shader->SetUniformMatrix("u_View", view);
	shader->SetUniformMatrix("u_ViewProjection", projection * view);
	shader->SetUniformMatrix("u_SkyboxMatrix", projection * glm::mat4(glm::mat3(view)));
	glm::vec3 camPos = glm::inverse(view) * glm::vec4(0, 0, 0, 1);
	shader->SetUniform("u_CamPos", camPos);
}

/*
inline btVector3 glm2bt(const glm::vec3& vec)
{
	return { vec.x, vec.y, vec.z };
}
*/

template<typename T>
T LERP(const T& p0, const T& p1, float t)
{
	return (1.0f - t) * p0 + t * p1;
}


//Templated Catmull-Rom function.
//(This will work for any type that supports addition and scalar multiplication.)
template<typename T>
T Catmull(const T& p0, const T& p1, const T& p2, const T& p3, float t)
{
	//TODO: Implement Catmull-Rom interpolation.
	return 0.5f * (2.f * p1 + t * (-p0 + p2)
		+ t * t * (2.f * p0 - 5.f * p1 + 4.f * p2 - p3)
		+ t * t * t * (-p0 + 3.f * p1 - 3.f * p2 + p3));
}

//Catmull-Rom Variables
float m_segmentTravelTime = 1.0f;
float m_segmentTimer;
size_t m_segmentIndex;


std::vector<glm::vec3> phantomWaypoints{glm::vec3(-36.0f, 12.0f, -1.0f), 
										glm::vec3(-40.0f, 0.1f, -1.0f),
										glm::vec3(-36.0f, -12.0f, -1.0f), 
										glm::vec3(-33.0f, -6.8f, -1.0f), 
										glm::vec3(-32.7f, 0.38f, -1.0f),
										glm::vec3(-33.8f, 7.01f, -1.0f)};


//std::vector<glm::vec3> phantomWaypoints;


void UpdateCatmull(std::vector<glm::vec3> points, GameObject object, float deltaTime)
{
	if (points.size() == 0 || m_segmentTravelTime == 0)
		return;

	m_segmentTimer += deltaTime;


	if (m_segmentIndex == 2)
	{
		object.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
	}

	if (m_segmentIndex == 5)
	{
		object.get<Transform>().SetLocalRotation(90.0f, 0.0f, 180.0f);
	}



	while (m_segmentTimer > m_segmentTravelTime)
	{
		m_segmentTimer -= m_segmentTravelTime;

		m_segmentIndex += 1;

		if (m_segmentIndex >= points.size())
			m_segmentIndex = 0;
	}

	float t = m_segmentTimer / m_segmentTravelTime;

	if (points.size() < 4)
	{
		object.get<Transform>().SetLocalPosition(points[0]);
		return;
	}

	size_t p0_ind, p1_ind, p2_ind, p3_ind;
	glm::vec3 p0, p1, p2, p3;

	//For Catmull, the path segment between p1 and p2
	//Our segment index is gonna be p1
	p1_ind = m_segmentIndex;

	p0_ind = (p1_ind == 0) ? points.size() - 1 : p1_ind - 1;

	p2_ind = (p1_ind + 1) % points.size();

	p3_ind = (p2_ind + 1) % points.size();

	//Setting the vec3s
	p0 = points[p0_ind];
	p1 = points[p1_ind];
	p2 = points[p2_ind];
	p3 = points[p3_ind];

	object.get<Transform>().SetLocalPosition(Catmull(p0, p1, p2, p3, t));
}



float Distance(GameObject obj1, GameObject obj2)
{
	float product = glm::pow((obj1.get<Transform>().GetLocalPosition().x - obj2.get<Transform>().GetLocalPosition().x), 2) +
					glm::pow((obj1.get<Transform>().GetLocalPosition().y - obj2.get<Transform>().GetLocalPosition().y), 2) +
					glm::pow((obj1.get<Transform>().GetLocalPosition().z - obj2.get<Transform>().GetLocalPosition().z), 2);

	float distance = glm::sqrt(product);

	return distance;
}



bool DistanceCheck(GameObject obj1, GameObject obj2)
{
	bool check;
	//glm::vec3 distance = glm::distance<glm::vec3>(obj1.get<Transform>().GetLocalPosition(), obj2.get<Transform>().GetLocalPosition());

	float distance = Distance(obj1, obj2);

	if (distance <= 18)
	{
		check = false;
	}
	else
	{
		check = true;
	}

	return check;

}



bool gotCoin = false;


void GetDistance(GameObject player, GameObject object, glm::vec3 distance)
{
	distance = player.get<Transform>().GetLocalPosition() - object.get<Transform>().GetLocalPosition();

	if (distance.x <= 2.0f && distance.y <= 2.0f && glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS && gotCoin == false)
	{
		CoinCount = 1;
		object.get<Transform>().SetLocalPosition(100.0f, 100.0f, 100.0f);
		gotCoin = true;
	}
}

void MoveWizard(GameObject player, GameObject wizard, glm::vec3 distance2, int coinCount)
{
	distance2 = player.get<Transform>().GetLocalPosition() - wizard.get<Transform>().GetLocalPosition();

	if (distance2.x <= 2.0f && distance2.y <= 2.0f && coinCount >= 1 && glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS && gotCoin == true)
	{
		CoinCount = 0;
		wizard.get<Transform>().SetLocalPosition(wizard.get<Transform>().GetLocalPosition() + glm::vec3(-33.0f, 5.0f, 0.0f));
	}
}


void CollisionResolver()
{
	canMoveForward = true;
	canMoveLeft = true;
	canMoveBack = true;
	canMoveRight = true;
}

void inputChecker()
{
	if (canMoveForward)
		std::cout << "Can move forwards" << std::endl;

	if (canMoveLeft)
		std::cout << "Can move left" << std::endl;

	if (canMoveBack)
		std::cout << "Can move back" << std::endl;

	if (canMoveRight)
		std::cout << "Can move right" << std::endl;
}


void CheckCollision(GameObject player, GameObject otherObject, float xRangePos, float xRangeNeg, float yRangePos, float yRangeNeg)
{
	//Forward
	if (player.get<Transform>().GetLocalPosition().x - xRangePos <= otherObject.get<Transform>().GetLocalPosition().x + xRangeNeg &&
		player.get<Transform>().GetLocalPosition().x >= otherObject.get<Transform>().GetLocalPosition().x - xRangeNeg &&
		player.get<Transform>().GetLocalPosition().y <= otherObject.get<Transform>().GetLocalPosition().y + yRangePos &&
		player.get<Transform>().GetLocalPosition().y >= otherObject.get<Transform>().GetLocalPosition().y - yRangeNeg)
	{
		std::cout << "Can't move forwards" << std::endl;
		canMoveForward = false;
	}
	else
	{
		//std::cout << "Exited forward block" << std::endl;
		canMoveForward = true;
		
	}
	//Backward
	if (player.get<Transform>().GetLocalPosition().x + xRangePos >= otherObject.get<Transform>().GetLocalPosition().x - xRangePos &&
		player.get<Transform>().GetLocalPosition().x <= otherObject.get<Transform>().GetLocalPosition().x + xRangeNeg &&
		player.get<Transform>().GetLocalPosition().y <= otherObject.get<Transform>().GetLocalPosition().y + yRangePos &&
		player.get<Transform>().GetLocalPosition().y >= otherObject.get<Transform>().GetLocalPosition().y - yRangeNeg)
	{
		canMoveBack = false;
		std::cout << "Can't move backwards" << std::endl;
	}
	else
	{
		//std::cout << "Exited backwards block" << std::endl;
		canMoveBack = true;
		
	}

	//Left
	if (player.get<Transform>().GetLocalPosition().y - yRangeNeg <= otherObject.get<Transform>().GetLocalPosition().y + yRangePos &&
		player.get<Transform>().GetLocalPosition().y >= otherObject.get<Transform>().GetLocalPosition().y - yRangePos &&
		player.get<Transform>().GetLocalPosition().x <= otherObject.get<Transform>().GetLocalPosition().x + xRangeNeg &&
		player.get<Transform>().GetLocalPosition().x >= otherObject.get<Transform>().GetLocalPosition().x - xRangePos)
	{
		canMoveLeft = false;
		std::cout << "Can't move Left" << std::endl;
	}
	else
	{
		//std::cout << "Exited left block" << std::endl;
		canMoveLeft = true;
		
	}

	if (player.get<Transform>().GetLocalPosition().y + yRangeNeg >= otherObject.get<Transform>().GetLocalPosition().y - yRangePos &&
		player.get<Transform>().GetLocalPosition().y <= otherObject.get<Transform>().GetLocalPosition().y + yRangePos &&
		player.get<Transform>().GetLocalPosition().x <= otherObject.get<Transform>().GetLocalPosition().x + xRangeNeg &&
		player.get<Transform>().GetLocalPosition().x >= otherObject.get<Transform>().GetLocalPosition().x - xRangePos)
	{
		canMoveRight = false;
		std::cout << "Can't move right" << std::endl;
	}
	else
	{
		//std::cout << "Exited right block" << std::endl;
		canMoveRight = true;
		
	}

}




void CheckPhantomCollision(GameObject player, GameObject other, float xRangePos, float xRangeNeg, float yRangePos, float yRangeNeg)
{
	//Forward
	if (player.get<Transform>().GetLocalPosition().x - xRangePos <= other.get<Transform>().GetLocalPosition().x + xRangeNeg &&
		player.get<Transform>().GetLocalPosition().x >= other.get<Transform>().GetLocalPosition().x - xRangeNeg &&
		player.get<Transform>().GetLocalPosition().y <= other.get<Transform>().GetLocalPosition().y + yRangePos &&
		player.get<Transform>().GetLocalPosition().y >= other.get<Transform>().GetLocalPosition().y - yRangeNeg)
	{
		PlayerHealth--;
		player.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.8f);
	}
	else
	{
		
	}
	//Backward
	if (player.get<Transform>().GetLocalPosition().x + xRangePos >= other.get<Transform>().GetLocalPosition().x - xRangePos &&
		player.get<Transform>().GetLocalPosition().x <= other.get<Transform>().GetLocalPosition().x + xRangeNeg &&
		player.get<Transform>().GetLocalPosition().y <= other.get<Transform>().GetLocalPosition().y + yRangePos &&
		player.get<Transform>().GetLocalPosition().y >= other.get<Transform>().GetLocalPosition().y - yRangeNeg)
	{
		PlayerHealth--;
		player.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.8f);
	}
	else
	{
		

	}

	//Left
	if (player.get<Transform>().GetLocalPosition().y - yRangeNeg <= other.get<Transform>().GetLocalPosition().y + yRangePos &&
		player.get<Transform>().GetLocalPosition().y >= other.get<Transform>().GetLocalPosition().y - yRangePos &&
		player.get<Transform>().GetLocalPosition().x <= other.get<Transform>().GetLocalPosition().x + xRangeNeg &&
		player.get<Transform>().GetLocalPosition().x >= other.get<Transform>().GetLocalPosition().x - xRangePos)
	{
		PlayerHealth--;
		player.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.8f);
	}
	else
	{
		
	}

	if (player.get<Transform>().GetLocalPosition().y + yRangeNeg >= other.get<Transform>().GetLocalPosition().y - yRangePos &&
		player.get<Transform>().GetLocalPosition().y <= other.get<Transform>().GetLocalPosition().y + yRangePos &&
		player.get<Transform>().GetLocalPosition().x <= other.get<Transform>().GetLocalPosition().x + xRangeNeg &&
		player.get<Transform>().GetLocalPosition().x >= other.get<Transform>().GetLocalPosition().x - xRangePos)
	{
		PlayerHealth--;
		player.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.8f);
	}
	else
	{
		
	}

}




void checkPosition(GameObject object)
{
	std::cout << "Player X" << object.get<Transform>().GetLocalPosition().x << std::endl;
	std::cout << "Player Y" << object.get<Transform>().GetLocalPosition().y << std::endl;
	std::cout << "Player Z" << object.get<Transform>().GetLocalPosition().z << std::endl;
}






int main() {
	Logger::Init(); // We'll borrow the logger from the toolkit, but we need to initialize it

	//Initialize GLFW
	if (!initGLFW())
		return 1;

	//Initialize GLAD
	if (!initGLAD())
		return 1;

	//BG music
	//PlaySound(TEXT("Music/Island1Music.wav"), NULL, SND_ASYNC | SND_FILENAME | SND_LOOP);



	//initPhysics();

	
	//https://github.com/bulletphysics/bullet3/blob/master/examples/HelloWorld/HelloWorld.cpp
	//https://www.raywenderlich.com/2606-bullet-physics-tutorial-getting-started#toc-anchor-001

	// Build the broadphase
	//btBroadphaseInterface* broadphase = new btDbvtBroadphase();

	///collision configuration contains default setup for memory, collision setup. Advanced users can create their own configuration.
	btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();

	///use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
	btCollisionDispatcher* dispatcher = new btCollisionDispatcher(collisionConfiguration);

	///btDbvtBroadphase is a good general purpose broadphase. You can also try out btAxis3Sweep.
	btBroadphaseInterface* overlappingPairCache = new btDbvtBroadphase();

	///the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
	btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver;

	btDiscreteDynamicsWorld* dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, overlappingPairCache, solver, collisionConfiguration);

	dynamicsWorld->setGravity(btVector3(0.f, 0.f, 0.f));


	//keep track of the shapes, we release memory at exit.
	//make sure to re-use collision shapes among rigid bodies whenever possible!
	btAlignedObjectArray<btCollisionShape*> collisionShapes;
	
	float planeHeight = 0.1f;

	/*
	//Plane
	btTransform t;
	t.setIdentity();
	t.setOrigin(btVector3(0, 0, planeHeight));
	btStaticPlaneShape* plane = new btStaticPlaneShape(btVector3(0, 0, 1), 0);
	btMotionState* motion = new btDefaultMotionState(t);
	btRigidBody::btRigidBodyConstructionInfo info(0.0, motion, plane);
	btRigidBody* body = new btRigidBody(info);
	dynamicsWorld->addRigidBody(body);
	*/

	//Player Physics
	btCollisionShape* playerShape = new btBoxShape(btVector3(1.f, 1.f, 1.f));

	btTransform playerTransform;
	
	btScalar playerMass(2.f);

	//rigidbody is dynamic if and only if mass is non zero, otherwise static
	bool isPlayerDynamic = (playerMass != 0.f);

	btVector3 localPlayerInertia(0, 0, 0);

	//using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
	btDefaultMotionState* playerMotionState;
	btRigidBody* playerBody;


	//Wizard Physics
	btCollisionShape* wizardShape = new btBoxShape(btVector3(1.f, 1.f, 1.f));

	btTransform wizardTransform;

	btScalar wizardMass(2.f);

	//rigidbody is dynamic if and only if mass is non zero, otherwise static
	bool isWizardDynamic = (wizardMass != 0.f);

	btVector3 localWizardInertia(0, 0, 0);

	//using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
	btDefaultMotionState* wizardMotionState;
	btRigidBody* wizardBody;


	////////////////// LERPING Variables ////////////////////////////////////////////////

	//Phantom Position Lerp
	glm::vec3 endPos = glm::vec3(-35.0f, -9.5f, -1.0f);
	glm::vec3 startPos = glm::vec3(-35.0f, 9.5f, -1.0f);

	glm::vec3 endPos2 = glm::vec3(-13.0f, 17.0f, -4.5f);
	glm::vec3 startPos2 = glm::vec3(-13.0f, 17.0f, -4.0f);

	glm::vec3 endPos3 = glm::vec3(-13.0f, 17.0f, -1.35f);
	glm::vec3 startPos3 = glm::vec3(-13.0f, 17.0f, -0.85f);

	float PhantomTimer = 0.0f;
	float ObjectTimer2 = 0.0f;
	float JumpTimer = 0.0f;

	float PhantomTimeLimit = 1.0f;
	float ObjectTimeLimit = 2.0f;
	float JumpTimeLimit = 0.15f;

	bool PhantomMove = true;
	bool PhantomMove2 = true;

	//Phantom Rotation Lerp
	glm::quat startPhantomRot = glm::angleAxis(glm::radians(0.0f), glm::vec3(1.0f, 1.0f, 1.0f));
	glm::quat endPhantomRot = glm::angleAxis(glm::radians(180.0f), glm::vec3(1.0f, 1.0f, 1.0f));

	glm::quat startPhantomRot2 = glm::angleAxis(glm::radians(0.0f), glm::vec3(1.0f, 1.0f, 1.0f));
	glm::quat endPhantomRot2 = glm::angleAxis(glm::radians(180.0f), glm::vec3(1.0f, 1.0f, 1.0f));

	float PhantomRotTimer = 0.0f;
	float PhantomRotTimeLimit = 4.0f;

	bool PhantomRot = true;
	bool PhantomRot2 = true;

	bool flipPhantom = false;
	bool flipObject = false;

	//////////////////////////////////////////////////////////////////////////////////////

	{


		// Let OpenGL know that we want debug output, and route it to our handler function
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(GlDebugMessage, nullptr);

		// Enable texturing
		glEnable(GL_TEXTURE_2D);


		// We'll use the provided mesh builder to build a new mesh with a few elements
		MeshBuilder<VertexPosNormTexCol> builder = MeshBuilder<VertexPosNormTexCol>();
		MeshFactory::AddCube(builder, glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(0.0f), glm::vec4(1.0f, 0.5f, 0.5f, 1.0f));
		VertexArrayObject::sptr vao3 = builder.Bake();

		//VertexArrayObject::sptr playerVao = ObjLoader::LoadFromFile("models/SkyBoundGuyCol.obj");

		//VertexArrayObject::sptr islandVao = ObjLoader::LoadFromFile("models/Island1ObjectTex.obj");



		// Load our shaders

		//Default Shader (Static Mesh Shader)
		Shader::sptr shader = Shader::Create();
		shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		shader->LoadShaderPartFromFile("shaders/frag_blinn_phong_textured.glsl", GL_FRAGMENT_SHADER);
		shader->Link();

		glm::vec3 lightPos = glm::vec3(0.0f, 0.0f, 6.0f);
		glm::vec3 lightCol = glm::vec3(0.9f, 0.85f, 0.5f);
		float     lightAmbientPow = 0.05f;
		float     lightSpecularPow = 1.0f;
		glm::vec3 ambientCol = glm::vec3(1.0f);
		float     ambientPow = 0.8f;
		float     textureMix = 0.2f;
		float     shininess = 4.0f;
		float     lightLinearFalloff = 0.09f;
		float     lightQuadraticFalloff = 0.032f;
		float     outlineThickness = 0.15;
		
		// These are our application / scene level uniforms that don't necessarily update
		// every frame
		shader->SetUniform("u_LightPos", lightPos);
		shader->SetUniform("u_LightCol", lightCol);
		shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
		shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
		shader->SetUniform("u_AmbientCol", ambientCol);
		shader->SetUniform("u_AmbientStrength", ambientPow);
		shader->SetUniform("u_TextureMix", textureMix);
		shader->SetUniform("u_Shininess", shininess);
		shader->SetUniform("u_LightAttenuationConstant", 1.0f);
		shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
		shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
		shader->SetUniform("u_OutlineThickness", outlineThickness);


		//Morph Shader (Animated Shader)
		Shader::sptr morphShader = Shader::Create();
		morphShader->LoadShaderPartFromFile("shaders/morphvertex_shader.glsl", GL_VERTEX_SHADER);
		morphShader->LoadShaderPartFromFile("shaders/frag_blinn_phong_textured.glsl", GL_FRAGMENT_SHADER);
		morphShader->Link();


		morphShader->SetUniform("u_LightPos", lightPos);
		morphShader->SetUniform("u_LightCol", lightCol);
		morphShader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
		morphShader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
		morphShader->SetUniform("u_AmbientCol", ambientCol);
		morphShader->SetUniform("u_AmbientStrength", ambientPow);
		morphShader->SetUniform("u_TextureMix", textureMix);
		morphShader->SetUniform("u_Shininess", shininess);
		morphShader->SetUniform("u_LightAttenuationConstant", 1.0f);
		morphShader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
		morphShader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
		morphShader->SetUniform("u_OutlineThickness", outlineThickness);


		Texture2D::sptr PlayerDiffuse = Texture2D::LoadFromFile("images/SkyBoundCharUV2.png");
		Texture2D::sptr diffuseMp02 = Texture2D::LoadFromFile("images/GrassIslandColours.png");
		Texture2D::sptr diffuseMp03 = Texture2D::LoadFromFile("images/WizardColours.png");
		Texture2D::sptr diffuseMp04 = Texture2D::LoadFromFile("images/PhantomColours.png");
		Texture2D::sptr diffuseMp05 = Texture2D::LoadFromFile("images/CoinTex.png");
		Texture2D::sptr diffuseMp06 = Texture2D::LoadFromFile("images/MoundColours.png");
		Texture2D::sptr diffuseMp07 = Texture2D::LoadFromFile("images/WishingWellColours.png");
		Texture2D::sptr diffuseMp08 = Texture2D::LoadFromFile("images/FlowerUVsColor.png");
		Texture2D::sptr diffuseMp09 = Texture2D::LoadFromFile("images/portal_texture.png");
		Texture2D::sptr diffuseMp10 = Texture2D::LoadFromFile("images/crystal_texture.png");
		Texture2D::sptr diffuseMp11 = Texture2D::LoadFromFile("images/taiga_island_texture.png");
		Texture2D::sptr diffuseMp12 = Texture2D::LoadFromFile("images/BushColour.png");
		Texture2D::sptr diffuseMp13 = Texture2D::LoadFromFile("images/BlackColour.png");
		Texture2D::sptr diffuseMp14 = Texture2D::LoadFromFile("images/brown.png");
		Texture2D::sptr diffuseMp15 = Texture2D::LoadFromFile("images/SignTexture.png");
		Texture2D::sptr diffuseMp16 = Texture2D::LoadFromFile("images/TreeTexture.png");
		Texture2D::sptr diffuseMp17 = Texture2D::LoadFromFile("images/PlatformTexture.png");
		Texture2D::sptr diffuseMp18 = Texture2D::LoadFromFile("images/StoneTexture.png");
		Texture2D::sptr diffuseMp19 = Texture2D::LoadFromFile("images/MailboxColor.png");
		Texture2D::sptr diffuseMp20 = Texture2D::LoadFromFile("images/flowerTexture.png");
		Texture2D::sptr diffuseMp21 = Texture2D::LoadFromFile("images/HouseColors.png");
		Texture2D::sptr diffuseMp22 = Texture2D::LoadFromFile("images/PineTreeColours.png");
		Texture2D::sptr diffuseMp23 = Texture2D::LoadFromFile("images/TreasureChestTexture.png");

		Texture2DData::sptr specularMp02 = Texture2DData::LoadFromFile("images/Stone_001_Specular.png");

		// Load the cube map
		TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/ocean.jpg");
		//TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/Sky.jpg");


		//Image Textures
		//Texture2D health = TextureImage::Image2D::Image2D("MoundColours.png", true);



		///////////////////////////////////// Scene Generation //////////////////////////////////////////////////
		#pragma region Scene Generation

		// We need to tell our scene system what extra component types we want to support
		GameScene::RegisterComponentType<RendererComponent>();
		GameScene::RegisterComponentType<BehaviourBinding>();
		GameScene::RegisterComponentType<Camera>();



		GameScene::sptr scene = GameScene::Create("test");
		Application::Instance().ActiveScene = scene;

		auto renderGroup = scene->Registry().group<RendererComponent, Transform>();

		ShaderMaterial::sptr material0 = ShaderMaterial::Create();
		material0->Shader = morphShader;
		material0->Set("s_Diffuse", PlayerDiffuse);
		//material0->Set("s_Diffuse2", Boxdiffuse2);
		//material0->Set("s_Specular", Boxspecular);
		material0->Set("u_Shininess", 8.0f);
		material0->Set("u_OutlineThickness", 0.53f);
		//material0->Set("u_TextureMix", 0.5f);
		//material0->Set("u_Reflectivity", 0.6f);

		ShaderMaterial::sptr material1 = ShaderMaterial::Create();
		material1->Shader = shader;
		material1->Set("s_Diffuse", diffuseMp02);
		material1->Set("u_Shininess", 8.0f);
		material1->Set("u_OutlineThickness", 0.08f);
		
		ShaderMaterial::sptr material2 = ShaderMaterial::Create();
		material2->Shader = morphShader;
		material2->Set("s_Diffuse", diffuseMp03);
		material2->Set("u_Shininess", 8.0f);
		material2->Set("u_OutlineThickness", 0.5f);

		ShaderMaterial::sptr material3 = ShaderMaterial::Create();
		material3->Shader = morphShader;
		material3->Set("s_Diffuse", diffuseMp04);
		material3->Set("u_Shininess", 8.0f);
		material3->Set("u_OutlineThickness", 0.5f);


		ShaderMaterial::sptr material4 = ShaderMaterial::Create();
		material4->Shader = shader;
		material4->Set("s_Diffuse", diffuseMp05);
		material4->Set("u_Shininess", 8.0f);
		material4->Set("u_OutlineThickness", 0.4f);

		ShaderMaterial::sptr material5 = ShaderMaterial::Create();
		material5->Shader = shader;
		material5->Set("s_Diffuse", diffuseMp06);
		material5->Set("u_Shininess", 8.0f);
		//material5->Set("u_OutlineThickness", 0.18f);

		ShaderMaterial::sptr material6 = ShaderMaterial::Create();
		material6->Shader = shader;
		material6->Set("s_Diffuse", diffuseMp07);
		material6->Set("u_Shininess", 8.0f);
		//material6->Set("u_OutlineThickness", 0.22f);

		ShaderMaterial::sptr material7 = ShaderMaterial::Create();
		material7->Shader = shader;
		material7->Set("s_Diffuse", diffuseMp08);
		material7->Set("u_Shininess", 8.0f);
		material7->Set("u_OutlineThickness", 0.08f);

		ShaderMaterial::sptr material8 = ShaderMaterial::Create();
		material8->Shader = shader;
		material8->Set("s_Diffuse", diffuseMp09);
		material8->Set("u_Shininess", 8.0f);
		material8->Set("u_OutlineThickness", 0.08f);

		ShaderMaterial::sptr material9 = ShaderMaterial::Create();
		material9->Shader = shader;
		material9->Set("s_Diffuse", diffuseMp10);
		material9->Set("u_Shininess", 8.0f);
		material9->Set("u_OutlineThickness", 0.08f);

		ShaderMaterial::sptr material10 = ShaderMaterial::Create();
		material10->Shader = shader;
		material10->Set("s_Diffuse", diffuseMp11);
		material10->Set("u_Shininess", 8.0f);
		material10->Set("u_OutlineThickness", 0.08f);

		ShaderMaterial::sptr material11 = ShaderMaterial::Create();
		material11->Shader = shader;
		material11->Set("s_Diffuse", diffuseMp12);
		material11->Set("u_Shininess", 8.0f);
		material11->Set("u_OutlineThickness", 0.42f);

		ShaderMaterial::sptr material12 = ShaderMaterial::Create();
		material12->Shader = shader;
		material12->Set("s_Diffuse", diffuseMp13);
		material12->Set("u_Shininess", 8.0f);
		material12->Set("u_OutlineThickness", 0.12f);

		ShaderMaterial::sptr material13 = ShaderMaterial::Create();
		material13->Shader = shader;
		material13->Set("s_Diffuse", diffuseMp14);
		material13->Set("u_Shininess", 8.0f);
		material13->Set("u_OutlineThickness", 0.4f);

		ShaderMaterial::sptr material14 = ShaderMaterial::Create();
		material14->Shader = shader;
		material14->Set("s_Diffuse", diffuseMp15);
		material14->Set("u_Shininess", 8.0f);
		material14->Set("u_OutlineThickness", 0.15f);

		ShaderMaterial::sptr material15 = ShaderMaterial::Create();
		material15->Shader = shader;
		material15->Set("s_Diffuse", diffuseMp16);
		material15->Set("u_Shininess", 8.0f);
		material15->Set("u_OutlineThickness", 0.35f);

		ShaderMaterial::sptr material16 = ShaderMaterial::Create();
		material16->Shader = shader;
		material16->Set("s_Diffuse", diffuseMp17);
		material16->Set("u_Shininess", 8.0f);
		material16->Set("u_OutlineThickness", 0.15f);

		ShaderMaterial::sptr material17 = ShaderMaterial::Create();
		material17->Shader = shader;
		material17->Set("s_Diffuse", diffuseMp18);
		material17->Set("u_Shininess", 8.0f);
		material17->Set("u_OutlineThickness", 0.3f);

		ShaderMaterial::sptr material18 = ShaderMaterial::Create();
		material18->Shader = shader;
		material18->Set("s_Diffuse", diffuseMp19);
		material18->Set("u_Shininess", 8.0f);
		material18->Set("u_OutlineThickness", 0.15f);

		ShaderMaterial::sptr material19 = ShaderMaterial::Create();
		material19->Shader = shader;
		material19->Set("s_Diffuse", diffuseMp20);
		material19->Set("u_Shininess", 8.0f);
		material19->Set("u_OutlineThickness", 0.15f);

		ShaderMaterial::sptr material20 = ShaderMaterial::Create();
		material20->Shader = shader;
		material20->Set("s_Diffuse", diffuseMp21);
		material20->Set("u_Shininess", 8.0f);
		material20->Set("u_OutlineThickness", 0.15f);

		ShaderMaterial::sptr material21 = ShaderMaterial::Create();
		material21->Shader = morphShader;
		material21->Set("s_Diffuse", diffuseMp22);
		material21->Set("u_Shininess", 3.0f);
		material21->Set("u_OutlineThickness", 0.00000000000001f);

		ShaderMaterial::sptr material22 = ShaderMaterial::Create();
		material22->Shader = shader;
		material22->Set("s_Diffuse", diffuseMp23);
		material22->Set("u_Shininess", 3.0f);
		material22->Set("u_OutlineThickness", 0.3f);


		/////////////////////////////////////////////////////////////////////////////

		//X = In and Out
		//Y = Left and Right
		//Z = up and down



		GameObject player = scene->CreateEntity("player");
		{
			player.emplace<MorphRenderer>();


			std::string walkPrefix = "models/Player/Walk/SkyBoundCharacter0";
			std::string walkFileName;



			for (int i = 0; i < 4; i++)
			{
				walkFileName = walkPrefix + std::to_string(i) + ".obj";

				player.get<MorphRenderer>().addFrame(MorphLoader::LoadFromFile(walkFileName));

			}

			player.get<MorphRenderer>().SetFrameTime(0.25);

			player.get<MorphRenderer>().SetMesh(player.get<MorphRenderer>().vao).SetMaterial(material0);



			
			
			player.get<Transform>().SetLocalPosition(-30.0f, -30.0f, -30.0f);
			player.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			player.get<Transform>().SetLocalScale(0.01f, 0.01f, 0.01f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(player);


		}

		GameObject island1 = scene->CreateEntity("Island1");
		{
			VertexArrayObject::sptr Island1VAO = ObjLoader::LoadFromFile("models/plains_island.obj");
			island1.emplace<RendererComponent>().SetMesh(Island1VAO).SetMaterial(material1);
			island1.get<Transform>().SetLocalPosition(0.0f, 0.0f, -4.5f);
			island1.get<Transform>().SetLocalRotation(-90.0f, 180.0f, 0.0f);
			island1.get<Transform>().SetLocalScale(1.5f, 1.5f, 1.5f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(island1);
			
		}

		GameObject island2 = scene->CreateEntity("Island2");
		{

			VertexArrayObject::sptr Island2VAO = ObjLoader::LoadFromFile("models/plains_island.obj");
			island2.emplace<RendererComponent>().SetMesh(Island2VAO).SetMaterial(material1);
			island2.get<Transform>().SetLocalPosition(-35.0f, 0.0f, -4.5f);
			island2.get<Transform>().SetLocalRotation(-90.0f, 180.0f, 0.0f);
			island2.get<Transform>().SetLocalScale(1.5f, 1.5f, 1.5f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(island2);
			//SetLocalPosition(-40.0f, 0.0f, -50.0f)->SetLocalRotation(90.0f, 0.0f, 0.0f)->SetLocalScale(8.0f, 8.0f, 8.0f);
		}


		

		GameObject Phantom2 = scene->CreateEntity("Phantom2");
		{
			Phantom2.emplace<MorphRenderer>();

			std::string Phantom2Prefix = "models/Phantom/Phantom0";
			std::string Phantom2FileName;

			for (int i = 0; i < 4; i++)
			{
				Phantom2FileName = Phantom2Prefix + std::to_string(i) + ".obj";

				Phantom2.get<MorphRenderer>().addFrame(MorphLoader::LoadFromFile(Phantom2FileName));

			}

			Phantom2.get<MorphRenderer>().SetFrameTime(0.3f);


			Phantom2.get<MorphRenderer>().SetMesh(Phantom2.get<MorphRenderer>().vao).SetMaterial(material3);
			Phantom2.get<Transform>().SetLocalPosition(-30.0f, -9.5f, -1.0f);
			Phantom2.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			Phantom2.get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(Phantom2);
			//SetLocalPosition(-40.0f, 0.0f, -50.0f)->SetLocalRotation(90.0f, 0.0f, 0.0f)->SetLocalScale(8.0f, 8.0f, 8.0f);
		}


	



		
		// Load a second material for our reflective material!
		Shader::sptr reflectiveShader = Shader::Create();
		reflectiveShader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		reflectiveShader->LoadShaderPartFromFile("shaders/frag_reflection.frag.glsl", GL_FRAGMENT_SHADER);
		reflectiveShader->Link();

		ShaderMaterial::sptr reflectiveMat = ShaderMaterial::Create();
		reflectiveMat->Shader = reflectiveShader;
		reflectiveMat->Set("s_Environment", environmentMap);
		// TODO: send the rotation to apply to the skybox
		reflectiveMat->Set("u_EnvironmentRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f),
			glm::vec3(1, 0, 0))));
		
		#pragma endregion
		//////////////////////////////////////////////////////////////////////////////////////////

		///////////////////////////////////// Scene Generation 2 //////////////////////////////////////////////////
		#pragma region Scene Generation 2

		// We need to tell our scene system what extra component types we want to support
		GameScene::RegisterComponentType<RendererComponent>();
		GameScene::RegisterComponentType<BehaviourBinding>();
		GameScene::RegisterComponentType<Camera>();

		GameScene::sptr scene2 = GameScene::Create("Scene 2");
		Application::Instance().ActiveScene = scene2;

		auto renderGroup2 = scene2->Registry().group<RendererComponent, Transform>();



		//X = In and Out
		//Y = Left and Right
		//Z = up and down

		

		


		

		GameScene::RegisterComponentType<RendererComponent>();
		GameScene::RegisterComponentType<BehaviourBinding>();
		GameScene::RegisterComponentType<Camera>();

		GameScene::sptr scene3 = GameScene::Create("Scene 3");
		Application::Instance().ActiveScene = scene3;

		auto renderGroup3 = scene3->Registry().group<RendererComponent, Transform>();






		#pragma endregion


	#pragma region MenuScene

		GameScene::RegisterComponentType<RendererComponent>();
		GameScene::RegisterComponentType<BehaviourBinding>();
		GameScene::RegisterComponentType<Camera>();

		GameScene::sptr scene0 = GameScene::Create("Scene 0");
		Application::Instance().ActiveScene = scene0;

		auto renderGroup0 = scene0->Registry().group<RendererComponent, Transform>();



		

		GameObject island1_2 = scene0->CreateEntity("Island1");
		{
			VertexArrayObject::sptr Island1VAO = ObjLoader::LoadFromFile("models/Island1ObjectTex.obj");
			island1_2.emplace<RendererComponent>().SetMesh(Island1VAO).SetMaterial(material1);
			island1_2.get<Transform>().SetLocalPosition(0.0f, 0.0f, -9.5f);
			island1_2.get<Transform>().SetLocalRotation(-90.0f, 180.0f, 0.0f);
			island1_2.get<Transform>().SetLocalScale(2.0f, 2.0f, 2.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(island1_2);


			
		}



		GameObject Mound_2 = scene0->CreateEntity("Mound");
		{
			VertexArrayObject::sptr MoundVAO = ObjLoader::LoadFromFile("models/Mound.obj");
			Mound_2.emplace<RendererComponent>().SetMesh(MoundVAO).SetMaterial(material5);
			Mound_2.get<Transform>().SetLocalPosition(-13.0f, 17.0f, -4.0f);
			Mound_2.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			Mound_2.get<Transform>().SetLocalScale(1.5f, 1.5f, 1.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(Mound_2);
			//SetLocalPosition(-40.0f, 0.0f, -50.0f)->SetLocalRotation(90.0f, 0.0f, 0.0f)->SetLocalScale(8.0f, 8.0f, 8.0f);

			//GameObject Mound2_2 = scene0->CreateEntity("Mound2");
			//Mound2_2.emplace<RendererComponent>().SetMesh(MoundVAO).SetMaterial(material5);
			//Mound2_2.get<Transform>().SetLocalPosition(-46.5f, -17.5f, -4.0f);
			//Mound2_2.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			//Mound2_2.get<Transform>().SetLocalScale(1.5f, 1.5f, 1.0f);
			//BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(Mound2_2);
			////SetLocalPosition(-40.0f, 0.0f, -50.0f)->SetLocalRotation(90.0f, 0.0f, 0.0f)->SetLocalScale(8.0f, 8.0f, 8.0f);

			GameObject Mound3_2 = scene0->CreateEntity("Mound3");
			Mound3_2.emplace<RendererComponent>().SetMesh(MoundVAO).SetMaterial(material5);
			Mound3_2.get<Transform>().SetLocalPosition(-17.5f, -9.5f, -4.0f);
			Mound3_2.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			Mound3_2.get<Transform>().SetLocalScale(1.5f, 1.5f, 1.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(Mound3_2);
			//SetLocalPosition(-40.0f, 0.0f, -50.0f)->SetLocalRotation(90.0f, 0.0f, 0.0f)->SetLocalScale(8.0f, 8.0f, 8.0f);

			//GameObject Mound4_2 = scene0->CreateEntity("Mound4");
			//Mound4_2.emplace<RendererComponent>().SetMesh(MoundVAO).SetMaterial(material5);
			//Mound4_2.get<Transform>().SetLocalPosition(-13.0f, -23.5f, -4.0f);
			//Mound4_2.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			//Mound4_2.get<Transform>().SetLocalScale(2.5f, 1.5f, 2.5f);
			//BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(Mound4_2);
			////SetLocalPosition(-40.0f, 0.0f, -50.0f)->SetLocalRotation(90.0f, 0.0f, 0.0f)->SetLocalScale(8.0f, 8.0f, 8.0f);
		}

		GameObject WishingWell_2 = scene0->CreateEntity("Wishing Well");
		{
			VertexArrayObject::sptr WishingWellVAO = ObjLoader::LoadFromFile("models/Wishing Well.obj");
			WishingWell_2.emplace<RendererComponent>().SetMesh(WishingWellVAO).SetMaterial(material6);
			WishingWell_2.get<Transform>().SetLocalPosition(-13.0f, 17.0f, -0.85f);
			WishingWell_2.get<Transform>().SetLocalRotation(90.0f, 0.0f, 125.0f);
			WishingWell_2.get<Transform>().SetLocalScale(0.4f, 0.4f, 0.4f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(WishingWell_2);
			//SetLocalPosition(-40.0f, 0.0f, -50.0f)->SetLocalRotation(90.0f, 0.0f, 0.0f)->SetLocalScale(8.0f, 8.0f, 8.0f);
		}

		GameObject PineTree = scene->CreateEntity("PineTree");
		{
			PineTree.emplace<MorphRenderer>();


			std::string PinePrefix = "models/PineTree";
			std::string PineFileName;



			for (int i = 0; i < 2; i++)
			{
				PineFileName = PinePrefix + std::to_string(i) + ".obj";

				PineTree.get<MorphRenderer>().addFrame(MorphLoader::LoadFromFile(PineFileName));

			}

			PineTree.get<MorphRenderer>().SetFrameTime(0.9);

			PineTree.get<MorphRenderer>().SetMesh(PineTree.get<MorphRenderer>().vao).SetMaterial(material21);




			PineTree.get<Transform>().SetLocalPosition(-7.0f, 0.0f, -1.75f);
			PineTree.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			PineTree.get<Transform>().SetLocalScale(0.75f, 0.75f, 0.75f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(PineTree);


		}

		GameObject PineTree2 = scene->CreateEntity("PineTree2");
		{
			PineTree2.emplace<MorphRenderer>();


			std::string PinePrefix = "models/PineTree";
			std::string PineFileName;



			for (int i = 0; i < 2; i++)
			{
				PineFileName = PinePrefix + std::to_string(i) + ".obj";

				PineTree2.get<MorphRenderer>().addFrame(MorphLoader::LoadFromFile(PineFileName));

			}

			PineTree2.get<MorphRenderer>().SetFrameTime(0.9);

			PineTree2.get<MorphRenderer>().SetMesh(PineTree2.get<MorphRenderer>().vao).SetMaterial(material21);




			PineTree2.get<Transform>().SetLocalPosition(-7.5f, -7.5f, -1.75f);
			PineTree2.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			PineTree2.get<Transform>().SetLocalScale(0.75f, 0.75f, 0.75f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(PineTree2);


		}


		GameObject PineTree3 = scene->CreateEntity("PineTree3");
		{
			PineTree3.emplace<MorphRenderer>();


			std::string PinePrefix = "models/PineTree";
			std::string PineFileName;



			for (int i = 0; i < 2; i++)
			{
				PineFileName = PinePrefix + std::to_string(i) + ".obj";

				PineTree3.get<MorphRenderer>().addFrame(MorphLoader::LoadFromFile(PineFileName));

			}

			PineTree3.get<MorphRenderer>().SetFrameTime(0.9);

			PineTree3.get<MorphRenderer>().SetMesh(PineTree3.get<MorphRenderer>().vao).SetMaterial(material21);




			PineTree3.get<Transform>().SetLocalPosition(7.5f, -6.0f, -1.75f);
			PineTree3.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			PineTree3.get<Transform>().SetLocalScale(0.75f, 0.75f, 0.75f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(PineTree3);


		}

		GameObject PineTree4 = scene->CreateEntity("PineTree3");
		{
			PineTree4.emplace<MorphRenderer>();


			std::string PinePrefix = "models/PineTree";
			std::string PineFileName;



			for (int i = 0; i < 2; i++)
			{
				PineFileName = PinePrefix + std::to_string(i) + ".obj";

				PineTree4.get<MorphRenderer>().addFrame(MorphLoader::LoadFromFile(PineFileName));

			}

			PineTree4.get<MorphRenderer>().SetFrameTime(0.9);

			PineTree4.get<MorphRenderer>().SetMesh(PineTree4.get<MorphRenderer>().vao).SetMaterial(material21);




			PineTree4.get<Transform>().SetLocalPosition(6.5f, 8.0f, -1.75f);
			PineTree4.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			PineTree4.get<Transform>().SetLocalScale(0.75f, 0.75f, 0.75f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(PineTree4);


		}

		GameObject TreasureChest = scene0->CreateEntity("TreasureChest");
		{
			VertexArrayObject::sptr TreasureChestVAO = ObjLoader::LoadFromFile("models/TreasureChest.obj");
			TreasureChest.emplace<RendererComponent>().SetMesh(TreasureChestVAO).SetMaterial(material22);
			TreasureChest.get<Transform>().SetLocalPosition(0.5f, 0.5f, -1.3f);
			TreasureChest.get<Transform>().SetLocalRotation(90.0f, 0.0f, 180.0f);
			TreasureChest.get<Transform>().SetLocalScale(0.6f, 0.6f, 0.6f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(TreasureChest);
			//SetLocalPosition(-40.0f, 0.0f, -50.0f)->SetLocalRotation(90.0f, 0.0f, 0.0f)->SetLocalScale(8.0f, 8.0f, 8.0f);
		}
		

		

		//GameObject Bush_2 = scene0->CreateEntity("Bush");
		//{
		//	VertexArrayObject::sptr BushVAO = ObjLoader::LoadFromFile("models/Bush.obj");
		//	Bush_2.emplace<RendererComponent>().SetMesh(BushVAO).SetMaterial(material11);
		//	Bush_2.get<Transform>().SetLocalPosition(6.0f, 11.0f, -2.7f);
		//	Bush_2.get<Transform>().SetLocalRotation(90.0f, 0.0f, 40.0f);
		//	Bush_2.get<Transform>().SetLocalScale(2.0f, 2.0f, 2.0f);
		//	BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(Bush_2);
		//	//SetLocalPosition(-40.0f, 0.0f, -50.0f)->SetLocalRotation(90.0f, 0.0f, 0.0f)->SetLocalScale(8.0f, 8.0f, 8.0f);
		//}

		//GameObject Sign_2 = scene0->CreateEntity("Sign");
		//{
		//	VertexArrayObject::sptr SignVAO = ObjLoader::LoadFromFile("models/Fardeen/Sign.obj");
		//	Sign_2.emplace<RendererComponent>().SetMesh(SignVAO).SetMaterial(material14);
		//	Sign_2.get<Transform>().SetLocalPosition(-6.0f, -5.0f, -2.7f);
		//	Sign_2.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
		//	Sign_2.get<Transform>().SetLocalScale(0.8f, 0.8f, 0.8f);
		//	BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(Sign_2);
		//	//SetLocalPosition(-40.0f, 0.0f, -50.0f)->SetLocalRotation(90.0f, 0.0f, 0.0f)->SetLocalScale(8.0f, 8.0f, 8.0f);
		//}

		//GameObject Tree_2 = scene0->CreateEntity("Tree");
		//{
		//	VertexArrayObject::sptr TreeVAO = ObjLoader::LoadFromFile("models/Fardeen/Tree.obj");
		//	Tree_2.emplace<RendererComponent>().SetMesh(TreeVAO).SetMaterial(material15);
		//	Tree_2.get<Transform>().SetLocalPosition(-17.5f, -9.5f, -2.7f);
		//	Tree_2.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
		//	Tree_2.get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
		//	BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(Tree_2);
		//	//SetLocalPosition(-40.0f, 0.0f, -50.0f)->SetLocalRotation(90.0f, 0.0f, 0.0f)->SetLocalScale(8.0f, 8.0f, 8.0f);
		//}

		//GameObject Platform_2 = scene0->CreateEntity("Platform");
		//{
		//	VertexArrayObject::sptr PlatformVAO = ObjLoader::LoadFromFile("models/Fardeen/Platform.obj");
		//	Platform_2.emplace<RendererComponent>().SetMesh(PlatformVAO).SetMaterial(material16);
		//	Platform_2.get<Transform>().SetLocalPosition(9.5f, 9.5f, -2.7f);
		//	Platform_2.get<Transform>().SetLocalRotation(90.0f, 0.0f, -60.0f);
		//	Platform_2.get<Transform>().SetLocalScale(2.0f, 2.0f, 2.0f);
		//	BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(Platform_2);
		//	//SetLocalPosition(-40.0f, 0.0f, -50.0f)->SetLocalRotation(90.0f, 0.0f, 0.0f)->SetLocalScale(8.0f, 8.0f, 8.0f);
		//}

		//GameObject Mailbox_2 = scene0->CreateEntity("mailbox");
		//{
		//	VertexArrayObject::sptr MailboxVAO = ObjLoader::LoadFromFile("models/hossain/mailbox.obj");
		//	Mailbox_2.emplace<RendererComponent>().SetMesh(MailboxVAO).SetMaterial(material18);
		//	Mailbox_2.get<Transform>().SetLocalPosition(11.0f, 7.5f, -2.7f);
		//	Mailbox_2.get<Transform>().SetLocalRotation(90.0f, 0.0f, -60.0f);
		//	Mailbox_2.get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
		//	BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(Mailbox_2);
		//	//SetLocalPosition(-40.0f, 0.0f, -50.0f)->SetLocalRotation(90.0f, 0.0f, 0.0f)->SetLocalScale(8.0f, 8.0f, 8.0f);
		//}

		//GameObject House_2 = scene0->CreateEntity("House");
		//{
		//	VertexArrayObject::sptr HouseVAO = ObjLoader::LoadFromFile("models/CompletedHouse.obj");
		//	House_2.emplace<RendererComponent>().SetMesh(HouseVAO).SetMaterial(material20);
		//	House_2.get<Transform>().SetLocalPosition(-13.0f, -23.5f, -1.1f);
		//	House_2.get<Transform>().SetLocalRotation(90.0f, 0.0f, 225.0f);
		//	House_2.get<Transform>().SetLocalScale(0.45f, 0.8f, 0.8f);
		//	BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(House_2);
		//	//SetLocalPosition(-40.0f, 0.0f, -50.0f)->SetLocalRotation(90.0f, 0.0f, 0.0f)->SetLocalScale(8.0f, 8.0f, 8.0f);

		//	
		//}

	
		

	#pragma endregion
		
		//////////////////////////////////////////////////////////////////////////////////////////

		{
			// Load our shaders 
			Shader::sptr skybox = std::make_shared<Shader>();
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.vert.glsl", GL_VERTEX_SHADER);
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.frag.glsl", GL_FRAGMENT_SHADER);
			skybox->Link();

			
			ShaderMaterial::sptr skyboxMat = ShaderMaterial::Create();
			skyboxMat->Shader = skybox;
			skyboxMat->Set("s_Environment", environmentMap);
			// TODO: send the rotation to apply to the skybox
			skyboxMat->RenderLayer = 100;
			skyboxMat->Set("u_EnvironmentRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f),
				glm::vec3(1, 0, 0))));
			

			MeshBuilder<VertexPosNormTexCol> mesh;
			MeshFactory::AddIcoSphere(mesh, glm::vec3(0.0f), 1.0f);
			MeshFactory::InvertFaces(mesh);
			VertexArrayObject::sptr meshVao = mesh.Bake();

			GameObject skyboxObj0 = scene0->CreateEntity("skybox");
			skyboxObj0.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			skyboxObj0.get_or_emplace<RendererComponent>().SetMesh(meshVao).SetMaterial(skyboxMat);

			GameObject skyboxObj = scene->CreateEntity("skybox");
			skyboxObj.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			skyboxObj.get_or_emplace<RendererComponent>().SetMesh(meshVao).SetMaterial(skyboxMat);

			GameObject skyboxObj2 = scene2->CreateEntity("skybox2");
			skyboxObj2.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			skyboxObj2.get_or_emplace<RendererComponent>().SetMesh(meshVao).SetMaterial(skyboxMat);
		}


		// We'll add some ImGui controls to control our shader
		imGuiCallbacks.push_back([&]() {
			if (ImGui::CollapsingHeader("Scene Level Lighting Settings"))
			{
				if (ImGui::ColorPicker3("Ambient Color", glm::value_ptr(ambientCol))) {
					shader->SetUniform("u_AmbientCol", ambientCol);
				}
				if (ImGui::SliderFloat("Fixed Ambient Power", &ambientPow, 0.01f, 1.0f)) {
					shader->SetUniform("u_AmbientStrength", ambientPow);
				}
			}
			if (ImGui::CollapsingHeader("Light Level Lighting Settings"))
			{
				if (ImGui::DragFloat3("Light Pos", glm::value_ptr(lightPos), 0.01f, -10.0f, 10.0f)) {
					shader->SetUniform("u_LightPos", lightPos);
				}
				if (ImGui::ColorPicker3("Light Col", glm::value_ptr(lightCol))) {
					shader->SetUniform("u_LightCol", lightCol);
				}
				if (ImGui::SliderFloat("Light Ambient Power", &lightAmbientPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
				}
				if (ImGui::SliderFloat("Light Specular Power", &lightSpecularPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
				if (ImGui::DragFloat("Light Linear Falloff", &lightLinearFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
				}
				if (ImGui::DragFloat("Light Quadratic Falloff", &lightQuadraticFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
				}
			}
			if (ImGui::CollapsingHeader("Material Level Lighting Settings"))
			{
				if (ImGui::SliderFloat("Shininess", &shininess, 0.1f, 128.0f)) {
					shader->SetUniform("u_Shininess", shininess);
				}
			}
			if (ImGui::CollapsingHeader("Light Settings"))
			{
				if (ImGui::Button("No Lighting"))
				{
					material21->Shader = morphShader;


					shader->SetUniform("u_AmbientLightStrength", 0.0f);
					shader->SetUniform("u_AmbientStrength", 0.0f);
					shader->SetUniform("u_SpecularLightStrength", 0.0f);

					morphShader->SetUniform("u_SpecularLightStrength", 0.0f);
					morphShader->SetUniform("u_AmbientStrength", 0.0f);
					morphShader->SetUniform("u_SpecularLightStrength", 0.0f);
				}
				if (ImGui::Button("Ambient"))
				{
					material21->Shader = morphShader;


					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					shader->SetUniform("u_AmbientStrength", ambientPow);
					shader->SetUniform("u_SpecularLightStrength", 0.0f);

					morphShader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					morphShader->SetUniform("u_AmbientStrength", ambientPow);
					morphShader->SetUniform("u_SpecularLightStrength", 0.0f);
					morphShader->SetUniform("u_OutlineThickness", 0.0f);
				}
				if (ImGui::Button("Specular"))
				{
					material21->Shader = morphShader;


					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
					morphShader->SetUniform("u_SpecularLightStrength", lightSpecularPow);

					shader->SetUniform("u_AmbientLightStrength", 0.0f);
					morphShader->SetUniform("u_AmbientLightStrength", 0.0f);

					shader->SetUniform("u_AmbientStrength", 0.0f);
					morphShader->SetUniform("u_AmbientStrength", 0.0f);
				}
				if (ImGui::Button("Ambient + Specular"))
				{
					material21->Shader = morphShader;


					shader->SetUniform("u_OutlineThickness", 0.0f);
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
					morphShader->SetUniform("u_SpecularLightStrength", lightSpecularPow);

					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					shader->SetUniform("u_AmbientStrength", ambientPow);
					morphShader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					morphShader->SetUniform("u_AmbientStrength", ambientPow);
				}
				if (ImGui::Button("Ambient + Specular + Toon Shading"))
				{
					material21->Shader = shader;

					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
					morphShader->SetUniform("u_SpecularLightStrength", lightSpecularPow);

					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
					morphShader->SetUniform("u_AmbientLightStrength", lightAmbientPow);

					shader->SetUniform("u_AmbientStrength", ambientPow);
					morphShader->SetUniform("u_AmbientStrength", ambientPow);
				}
			}
			});

		// GL states
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glDepthFunc(GL_LEQUAL);

		// NEW STUFF

		// Create some transforms and initialize them
		/*Transform playerTransform;
		Transform islandTransform;
		Transform islandTransform2;


		playerTransform = Transform::Create();
		islandTransform = Transform::Create();
		islandTransform2 = Transform::Create();

		Transform::sptr transforms[4];
		transforms[0] = Transform::Create();
		transforms[1] = Transform::Create();
		transforms[2] = Transform::Create();
		transforms[3] = Transform::Create();

		// We can use operator chaining, since our Set* methods return a pointer to the instance, neat!
		playerTransform->SetLocalPosition(0.0f, 10.0f, 0.0f)->SetLocalRotation(90.0f, 0.0f, 192.0f)->SetLocalScale(0.5f, 0.5f, 0.5f);
		islandTransform->SetLocalPosition(-40.0f, 0.0f, -50.0f)->SetLocalRotation(90.0f, 0.0f, 0.0f)->SetLocalScale(8.0f, 8.0f, 8.0f);
		islandTransform2->SetLocalPosition(-175.0f, 0.0f, -50.0f)->SetLocalRotation(90.0f, 0.0f, 0.0f)->SetLocalScale(8.0f, 8.0f, 8.0f);

		transforms[1]->SetLocalPosition(2.0f, 0.0f, 0.5f)->SetLocalRotation(00.0f, 0.0f, 45.0f);
		transforms[2]->SetLocalPosition(-2.0f, 0.0f, 0.5f)->SetLocalRotation(00.0f, 0.0f, -45.0f);
		transforms[3]->SetLocalPosition(0.0f, 0.0f, 0.5f)->SetLocalRotation(00.0f, 0.0f, 0.0f);*/

		// We'll store all our VAOs into a nice array for easy access
		VertexArrayObject::sptr vaos[4];
		vaos[3] = vao3;



		// Load our texture data from a file
		Texture2DData::sptr diffuseMp = Texture2DData::LoadFromFile("images/SkyBoundCharUV2.png");
		Texture2DData::sptr diffuseMp2 = Texture2DData::LoadFromFile("images/GrassIslandColours.png");
		Texture2DData::sptr specularMp = Texture2DData::LoadFromFile("images/Stone_001_Specular.png");

		// Create a texture from the data
		Texture2D::sptr diffuse = Texture2D::Create();
		diffuse->LoadData(diffuseMp);

		// Create a texture from the data
		Texture2D::sptr diffuse2 = Texture2D::Create();
		diffuse2->LoadData(diffuseMp2);

		Texture2D::sptr specular = Texture2D::Create();
		specular->LoadData(specularMp);

		// Creating an empty texture
		Texture2DDescription desc = Texture2DDescription();
		desc.Width = 1;
		desc.Height = 1;
		desc.Format = InternalFormat::RGB8;
		Texture2D::sptr texture2 = Texture2D::Create(desc);
		texture2->Clear();

		

		// We'll use a temporary lil structure to store some info about our material (we'll expand this later)
		Material materials[4];
		Material playerMaterial;
		Material islandMaterial;

		playerMaterial.Albedo = diffuse;
		playerMaterial.Specular = specular;
		playerMaterial.Shininess = 4.0f;
		playerMaterial.OutlineThickness = 0.0001f;

		islandMaterial.Albedo = diffuse2;
		islandMaterial.Specular = specular;
		islandMaterial.Shininess = 4.0f;
		islandMaterial.OutlineThickness = 0.15f;

		materials[3].Albedo = diffuse;
		materials[3].NewTexture = diffuse2;
		materials[3].Specular = specular;
		materials[3].Shininess = 64.0f;
		materials[3].TextureMix = 0.6f;

		camera = Camera::Create();
		camera->SetPosition(glm::vec3(4.0f, 1.2f, 2.0f)); // Set initial position
		camera->SetUp(glm::vec3(0, 0, 1)); // Use a z-up coordinate system
		camera->LookAt(glm::vec3(0.0f)); // Look at center of the screen
		camera->SetFovDegrees(84.0f); // Set an initial FOV
		camera->SetOrthoHeight(3.0f);

		// We'll use a vector to store all our key press events for now
		std::vector<KeyPressWatcher> keyToggles;
		// This is an example of a key press handling helper. Look at InputHelpers.h an .cpp to see
		// how this is implemented. Note that the ampersand here is capturing the variables within
		// the scope. If you wanted to do some method on the class, your best bet would be to give it a method and
		// use std::bind
		keyToggles.emplace_back(GLFW_KEY_T, [&]() { camera->ToggleOrtho(); });

		int selectedVao = 3; // select cube by default
		keyToggles.emplace_back(GLFW_KEY_KP_ADD, [&]() {
			selectedVao++;
			if (selectedVao >= 4)
				selectedVao = 1;
			});
		keyToggles.emplace_back(GLFW_KEY_KP_SUBTRACT, [&]() {
			selectedVao--;
			if (selectedVao <= 0)
				selectedVao = 3;
			});
		
		

		InitImGui();

		// Our high-precision timer
		double lastFrame = glfwGetTime();

		//Speed Variables
		float speed = 3.0f;
		float speedTimer = 0.0f;
		float speedTimeLimit = 2.0f;
		bool canSprint = true;
		glm::vec3 CoinDistance = glm::vec3();
		glm::vec3 WizardDistance = glm::vec3();

		//Jump Variables
		bool canJump = true;
		//float jumpTimeLimit


		

		

		//Key Watcher for changing camera modes
		bool jumpSwitch = true;
		KeyPressWatcher jumpWatcher = KeyPressWatcher(GLFW_KEY_SPACE, [&]() {
			jumpSwitch = !jumpSwitch;
			//player.get<Transform>().SetLocalPosition(LERP(currentPosition, JumpPosition, 0.5f));
			});

		camera->SetPosition(glm::vec3(16.0f, 10.0f, 5.5f));
		camera->SetRotation(glm::vec3(-95.0f, -35.0f, -10.0f));

		
		///// Game loop /////
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();

			//glm::vec3 currentPosition = player.get<Transform>().GetLocalPosition();
			//glm::vec3 offset = glm::vec3(0.0f, 0.0f, 2.0f);
			//glm::vec3 JumpPosition = glm::vec3 (player.get<Transform>().GetLocalPosition().x, player.get<Transform>().GetLocalPosition().y, player.get<Transform>().GetLocalPosition().z + offset.z);

			// Calculate the time since our last frame (dt)
			double thisFrame = glfwGetTime();
			float dt = static_cast<float>(thisFrame - lastFrame);

			//Jump Check
			jumpWatcher.Poll(window);

			std::cout << std::to_string(CoinCount) << std::endl;

			dynamicsWorld->stepSimulation(1.f / 60.f, 10);

			//print positions of all objects
			for (int j = dynamicsWorld->getNumCollisionObjects() - 1; j >= 0; j--)
			{
				btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[j];
				btRigidBody* body = btRigidBody::upcast(obj);
				btTransform trans;
				if (body && body->getMotionState())
				{
					body->getMotionState()->getWorldTransform(trans);
				}
				else
				{
					trans = obj->getWorldTransform();
				}
				//printf("world pos object %d = %f,%f,%f\n", j, float(trans.getOrigin().getX()), float(trans.getOrigin().getY()), float(trans.getOrigin().getZ()));
			}
			
			


			


			/*if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
				player.get<Transform>().SetLocalPosition(LERP(currentPosition, JumpPosition, 0.02f) * dt);
			}*/

			if (player.get<Transform>().GetLocalPosition().z <= -7.0f)
			{
				player.get<Transform>().SetLocalPosition(0.5f, 0.0f, 5.0f);
				PlayerHealth--;
			}
			


			//Switching scenes when player reaches a certain point
			if (player.get<Transform>().GetLocalPosition().x <= -49 && gotCoin == true)
			{
				player.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
				RenderGroupBool = 2;
			}
			if (PlayerHealth <= 0)
			{
				player.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
				RenderGroupBool = 3;
			}

			//player.get<Transform>().SetLocalPosition(player.get<Transform>().GetLocalPosition() - glm::vec3(0.0f, 0.0f, 2.0f * dt));
			

			//Updating Physics Body
			//playerTransform = playerBody->getCenterOfMassTransform();
			//playerTransform.setOrigin(glm2bt(player.get<Transform>().GetLocalPosition()));
			//playerBody->setCenterOfMassTransform(playerTransform);

			//wizardTransform = wizardBody->getCenterOfMassTransform();
			//wizardTransform.setOrigin(glm2bt(Wizard.get<Transform>().GetLocalPosition()));
			//wizardBody->setCenterOfMassTransform(wizardTransform);
			
			
			
			//playerTransform.setFromOpenGLMatrix(glm::value_ptr(player.get<Transform>().LocalTransform()));
			//playerBody->getMotionState()->setWorldTransform(playerTransform);

			/*
			btTransform playerT = playerBody->getCenterOfMassTransform();
			playerT.setOrigin(glm2bt(player.get<Transform>().GetLocalPosition()));
			playerBody->setWorldTransform(playerT);

			btTransform wizardT = wizardBody->getCenterOfMassTransform();
			wizardT.setOrigin(glm2bt(Wizard.get<Transform>().GetLocalPosition()));
			wizardBody->setWorldTransform(wizardT);


			playerT.setFromOpenGLMatrix(glm::value_ptr(player.get<Transform>().LocalTransform()));
			playerBody->getMotionState()->setWorldTransform(playerT);

			wizardT.setFromOpenGLMatrix(glm::value_ptr(Wizard.get<Transform>().LocalTransform()));
			wizardBody->getMotionState()->setWorldTransform(wizardT);
			*/


			PhantomTimer = 1.0f;

			//UpdateCatmull(phantomWaypoints, Phantom, dt);

			
			

			/*
			//Phantom LERP Position
			PhantomTimer += dt;

			if (PhantomTimer >= PhantomTimeLimit)
			{
				PhantomTimer = 0.0f;
				PhantomMove = !PhantomMove;
			}

			float phantomTPos = PhantomTimer / PhantomTimeLimit;

			if (PhantomMove == true)
			{
				Phantom.get<Transform>().SetLocalPosition(LERP(startPos, endPos, phantomTPos));
			}
			else if (PhantomMove == false)
			{
				Phantom.get<Transform>().SetLocalPosition(LERP(endPos, startPos, phantomTPos));
			}


			if (Phantom.get<Transform>().GetLocalPosition() == startPos)
			{
				if (flipPhantom)
				{
					Phantom.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
					//Phantom.get<Transform>().SetLocalScale(Phantom.get<Transform>().GetLocalScale() * glm::vec3(1.0f, 1.0f, -1.0f));
				}
			}
			
			if (Phantom.get<Transform>().GetLocalPosition() == endPos)
			{
				flipPhantom = true;

				if (flipPhantom)
				{
					Phantom.get<Transform>().SetLocalRotation(90.0f, 0.0f, 180.0f);
					//Phantom.get<Transform>().SetLocalScale(Phantom.get<Transform>().GetLocalScale() * glm::vec3(1.0f, 1.0f, -1.0f));
				}
			}
			*/


			ObjectTimer2 += dt;

			if (ObjectTimer2 >= ObjectTimeLimit)
			{
				ObjectTimer2 = 0.0f;
				PhantomMove2 = !PhantomMove2;
			}

			float phantomTPos2 = ObjectTimer2 / ObjectTimeLimit;

			if (PhantomMove2 == true)
			{
				Mound_2.get<Transform>().SetLocalPosition(LERP(startPos2, endPos2, phantomTPos2));
			}
			else if (PhantomMove2 == false)
			{
				Mound_2.get<Transform>().SetLocalPosition(LERP(endPos2, startPos2, phantomTPos2));
			}

			if (PhantomMove2 == true)
			{
				WishingWell_2.get<Transform>().SetLocalPosition(LERP(startPos3, endPos3, phantomTPos2));
			}
			else if (PhantomMove2 == false)
			{
				WishingWell_2.get<Transform>().SetLocalPosition(LERP(endPos3, startPos3, phantomTPos2));
			}

			



			//Jump Stuff
			
			glm::vec3 currentPosition = player.get<Transform>().GetLocalPosition();

			glm::vec3 offset = glm::vec3(0.0f, 0.0f, 6.0f);
			glm::vec3 JumpPosition = player.get<Transform>().GetLocalPosition() + offset;
			float tPos = JumpTimer / JumpTimeLimit;

			if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
				JumpTimer += dt;
				if (JumpTimer <= JumpTimeLimit)
					player.get<Transform>().SetLocalPosition(LERP(currentPosition, JumpPosition, 0.08f));

			}
			else
			{

				//JumpTimer = 0.0f;
			}

			if (player.get<Transform>().GetLocalPosition().z <= planeHeight + 0.01)
			{
				JumpTimer = 0.0f;
			}

			

			


			

			// We'll make sure our UI isn't focused before we start handling input for our game
			if (!ImGui::IsAnyWindowFocused()) {
				// We need to poll our key watchers so they can do their logic with the GLFW state
				// Note that since we want to make sure we don't copy our key handlers, we need a const
				// reference!
				for (const KeyPressWatcher& watcher : keyToggles) {
					watcher.Poll(window);
				}

				// We'll run some basic input to move our transform around
				//ManipulateTransformWithInput(transforms[selectedVao], dt);
				PlayerInput(player, dt, speed);
				

				//Sprinting Function
				#pragma region Sprint Stuff
				if (canSprint)
				{
					if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
					{
						speed = 9.0f;

						speedTimer += dt;

						if (speedTimer >= speedTimeLimit)
						{
							speed = 3.0f;
							canSprint = false;
						}
					}
					if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_RELEASE)
					{
						speed = 3.0f;
					}
				}

				if (!canSprint)
				{
					speedTimer -= dt;

					if (speedTimer <= 0.0f)
					{
						speedTimer == 0.0f;
						canSprint = true;
					}
				}
				
				#pragma endregion
			}

			

			



			glClearColor(0.99f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			shader->Bind();
			// These are the uniforms that update only once per frame
			//shader->SetUniformMatrix("u_View", camera->GetView());
			//shader->SetUniform("u_CamPos", camera->GetPosition());

			// Tell OpenGL that slot 0 will hold the diffuse, and slot 1 will hold the specular
			shader->SetUniform("s_Diffuse", 0);
			shader->SetUniform("s_Specular", 1);
			shader->SetUniform("s_Diffuse2", 2);

			// Render all VAOs in our scene
			//for(int ix = 0; ix < 4; ix++) {
				// TODO: Apply materials

				// Apply material properties for each instance
				//materials[ix].Albedo->Bind(0);
				///materials[ix].Specular->Bind(1);
				///materials[ix].NewTexture->Bind(2);
				//shader->SetUniform("u_Shininess", materials[ix].Shininess);
				//shader->SetUniform("u_TextureMix", materials[ix].TextureMix);
				//shader->SetUniform("u_TextureMix", materials[ix].TextureMix);

				//RenderVAO(shader, vaos[ix], camera, transforms[ix]);			
			//}


			//RenderVAO(shader, vao3, camera, transforms[3]);


			playerMaterial.Albedo->Bind(0);
			playerMaterial.Specular->Bind(1);
			//shader->SetUniform("u_OutlineThickness", playerMaterial.OutlineThickness);
			//RenderVAO(shader, playerVao, camera, playerTransform);


			islandMaterial.Albedo->Bind(0);
			islandMaterial.Specular->Bind(1);
			//shader->SetUniform("u_OutlineThickness", islandMaterial.OutlineThickness);
			//RenderVAO(shader, islandVao, camera, islandTransform);

			islandMaterial.Albedo->Bind(0);
			islandMaterial.Specular->Bind(1);
			//RenderVAO(shader, islandVao, camera, islandTransform2);


			/*if (RenderGroupBool != 0)
			{
				camera->SetPosition(player.get<Transform>().GetLocalPosition() + glm::vec3(6.0f, 0.0f, 2.5f));
				camera->SetRotation(glm::vec3(-95.0f, 0.0f, 0.0f));
			}
			else
			{
				camera->SetPosition(glm::vec3(16.0f, 10.0f, 8.0f));
				camera->SetRotation(glm::vec3(-95.0f, -35.0f, -10.0f));
			}*/
			
			if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
			{
				camera->SetPosition(camera->GetPosition() + glm::vec3(-5.0f, 0.0f, 0.0f) * dt);
			}
			if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
			{
				camera->SetPosition(camera->GetPosition() + glm::vec3(5.0f, 0.0f, 0.0f) * dt);
			}
			if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			{
				camera->SetPosition(camera->GetPosition() + glm::vec3(0.0f, -5.0f, 0.0f) * dt);
			}
			if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			{
				camera->SetPosition(camera->GetPosition() + glm::vec3(0.0f, 5.0f, 0.0f) * dt);
			}
			if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
			{
				camera->SetPosition(camera->GetPosition() - glm::vec3(0.0f, 0.0f, 5.0f) * dt);
			}
			if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
			{
				camera->SetPosition(camera->GetPosition() + glm::vec3(0.0f, 0.0f, 5.0f) * dt);
			}
			else
			{
				camera->SetPosition(camera->GetPosition());
				camera->SetRotation(glm::vec3(-95.0f, -35.0f, -10.0f));
			}


			// Lighting Toggles

			// No lighting
			if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
			{
				material21->Shader = morphShader;


				shader->SetUniform("u_AmbientLightStrength", 0.0f);
				shader->SetUniform("u_AmbientStrength", 0.0f);
				shader->SetUniform("u_SpecularLightStrength", 0.0f);

				morphShader->SetUniform("u_SpecularLightStrength", 0.0f);
				morphShader->SetUniform("u_AmbientStrength", 0.0f);
				morphShader->SetUniform("u_SpecularLightStrength", 0.0f);


			}
			// Ambient Only
			else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
			{
				material21->Shader = morphShader;


				shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
				shader->SetUniform("u_AmbientStrength", ambientPow);
				shader->SetUniform("u_SpecularLightStrength", 0.0f);

				morphShader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
				morphShader->SetUniform("u_AmbientStrength", ambientPow);
				morphShader->SetUniform("u_SpecularLightStrength", 0.0f);
				morphShader->SetUniform("u_OutlineThickness", 0.0f);
				

				

			}
			// Specular Only
			else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
			{
				material21->Shader = morphShader;


				shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				morphShader->SetUniform("u_SpecularLightStrength", lightSpecularPow);

				shader->SetUniform("u_AmbientLightStrength", 0.0f);
				morphShader->SetUniform("u_AmbientLightStrength", 0.0f);

				shader->SetUniform("u_AmbientStrength", 0.0f);
				morphShader->SetUniform("u_AmbientStrength", 0.0f);

				
				
			}
			// Ambient + Specular
			else if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS)
			{
				material21->Shader = morphShader;


				shader->SetUniform("u_OutlineThickness", 0.0f);
				shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				morphShader->SetUniform("u_SpecularLightStrength", lightSpecularPow);

				shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
				shader->SetUniform("u_AmbientStrength", ambientPow);
				morphShader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
				morphShader->SetUniform("u_AmbientStrength", ambientPow);

				
				
				
			}
			// Ambient + Specular + Toon Shading
			else if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS)
			{
				material21->Shader = shader;

				shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				morphShader->SetUniform("u_SpecularLightStrength", lightSpecularPow);

				shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
				morphShader->SetUniform("u_AmbientLightStrength", lightAmbientPow);

				shader->SetUniform("u_AmbientStrength", ambientPow);		
				morphShader->SetUniform("u_AmbientStrength", ambientPow);

				
			}












			
			//CheckCollision(player, Sign, 0.6f, 0.6f, 0.6f, 0.6f);
			//CheckCollision(player, Wizard, 0.85f, 0.85f, 0.85f, 0.85f);
			//CheckPhantomCollision(player, Phantom, 0.8f, 0.8f, 0.8f, 0.8f);
			CheckPhantomCollision(player, Phantom2, 0.8f, 0.8f, 0.8f, 0.8f);

			//checkPosition(player);
			//inputChecker();
			

			//std::cout << "Distance: " << Distance(player, island1) << std::endl;

			//Transform& camTransform = cameraObject.get<Transform>();
			glm::mat4 view = (camera->GetView());
			glm::mat4 projection = camera->GetProjection();
			glm::mat4 viewProjection = projection * view;

			// Sort the renderers by shader and material, we will go for a minimizing context switches approach here,
			// but you could for instance sort front to back to optimize for fill rate if you have intensive fragment shaders
			if (RenderGroupBool == 0)
			{
				renderGroup0.sort<RendererComponent>([](const RendererComponent& l, const RendererComponent& r) {
					// Sort by render layer first, higher numbers get drawn last
					if (l.Material->RenderLayer < r.Material->RenderLayer) return true;
					if (l.Material->RenderLayer > r.Material->RenderLayer) return false;

					// Sort by shader pointer next (so materials using the same shader run sequentially where possible)
					if (l.Material->Shader < r.Material->Shader) return true;
					if (l.Material->Shader > r.Material->Shader) return false;

					// Sort by material pointer last (so we can minimize switching between materials)
					if (l.Material < r.Material) return true;
					if (l.Material > r.Material) return false;

					return false;
					});
			}

			if (RenderGroupBool == 1)
			{
				renderGroup.sort<RendererComponent>([](const RendererComponent& l, const RendererComponent& r) {
					// Sort by render layer first, higher numbers get drawn last
					if (l.Material->RenderLayer < r.Material->RenderLayer) return true;
					if (l.Material->RenderLayer > r.Material->RenderLayer) return false;

					// Sort by shader pointer next (so materials using the same shader run sequentially where possible)
					if (l.Material->Shader < r.Material->Shader) return true;
					if (l.Material->Shader > r.Material->Shader) return false;

					// Sort by material pointer last (so we can minimize switching between materials)
					if (l.Material < r.Material) return true;
					if (l.Material > r.Material) return false;

					return false;
					});
			}
			else if (RenderGroupBool == 2)
			{
				renderGroup2.sort<RendererComponent>([](const RendererComponent& l, const RendererComponent& r) {
					// Sort by render layer first, higher numbers get drawn last
					if (l.Material->RenderLayer < r.Material->RenderLayer) return true;
					if (l.Material->RenderLayer > r.Material->RenderLayer) return false;

					// Sort by shader pointer next (so materials using the same shader run sequentially where possible)
					if (l.Material->Shader < r.Material->Shader) return true;
					if (l.Material->Shader > r.Material->Shader) return false;

					// Sort by material pointer last (so we can minimize switching between materials)
					if (l.Material < r.Material) return true;
					if (l.Material > r.Material) return false;

					return false;
					});
			}
			else if (RenderGroupBool == 3)
			{
				renderGroup3.sort<RendererComponent>([](const RendererComponent& l, const RendererComponent& r) {
					// Sort by render layer first, higher numbers get drawn last
					if (l.Material->RenderLayer < r.Material->RenderLayer) return true;
					if (l.Material->RenderLayer > r.Material->RenderLayer) return false;

					// Sort by shader pointer next (so materials using the same shader run sequentially where possible)
					if (l.Material->Shader < r.Material->Shader) return true;
					if (l.Material->Shader > r.Material->Shader) return false;

					// Sort by material pointer last (so we can minimize switching between materials)
					if (l.Material < r.Material) return true;
					if (l.Material > r.Material) return false;

					return false;
					});
			}
			
			

			// Start by assuming no shader or material is applied
			Shader::sptr current = nullptr;
			ShaderMaterial::sptr currentMat = nullptr;

			// Iterate over the render group components and draw them
			if (RenderGroupBool == 0)
			{
				renderGroup0.each([&](entt::entity, RendererComponent& renderer, Transform& transform) {
					// If the shader has changed, bind it and set up it's uniforms
					if (current != renderer.Material->Shader) {
						current = renderer.Material->Shader;
						current->Bind();
						SetupShaderForFrame(current, view, projection);
					}
					// If the material has changed, apply it
					if (currentMat != renderer.Material) {
						currentMat = renderer.Material;
						currentMat->Apply();
					}
					// Render the mesh
					RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
					});
			}


			if (RenderGroupBool == 1)
			{
				renderGroup.each([&](entt::entity, RendererComponent& renderer, Transform& transform) {
					// If the shader has changed, bind it and set up it's uniforms
					if (current != renderer.Material->Shader) {
						current = renderer.Material->Shader;
						current->Bind();
						SetupShaderForFrame(current, view, projection);
					}
					// If the material has changed, apply it
					if (currentMat != renderer.Material) {
						currentMat = renderer.Material;
						currentMat->Apply();
					}
					// Render the mesh
					RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
					});
			}
			else if (RenderGroupBool == 2)
			{
				renderGroup2.each([&](entt::entity, RendererComponent& renderer, Transform& transform) {
					// If the shader has changed, bind it and set up it's uniforms
					if (current != renderer.Material->Shader) {
						current = renderer.Material->Shader;
						current->Bind();
						SetupShaderForFrame(current, view, projection);
					}
					// If the material has changed, apply it
					if (currentMat != renderer.Material) {
						currentMat = renderer.Material;
						currentMat->Apply();
					}
					// Render the mesh
					RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
					});
			}
			else if (RenderGroupBool == 3)
			{
				renderGroup3.each([&](entt::entity, RendererComponent& renderer, Transform& transform) {
					// If the shader has changed, bind it and set up it's uniforms
					if (current != renderer.Material->Shader) {
						current = renderer.Material->Shader;
						current->Bind();
						SetupShaderForFrame(current, view, projection);
					}
					// If the material has changed, apply it
					if (currentMat != renderer.Material) {
						currentMat = renderer.Material;
						currentMat->Apply();
					}
					// Render the mesh
					RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
					});
			}
			






			//Menu = scene 0
			//Level 1 = scene 1
			//Level 2 = scene 2
			//Game over = scene 3

			//Update Animation
			if (RenderGroupBool != 3 || RenderGroupBool != 0)
			{
				player.get<MorphRenderer>().nextFrame(dt);


				// SetupShaderForFrame(morphShader, view, projection);
				player.get<MorphRenderer>().render(morphShader, viewProjection, player.get<Transform>(), view, viewProjection);
			}


			//PineTree Animations
			PineTree.get<MorphRenderer>().nextFrame(dt);
			PineTree.get<MorphRenderer>().render(morphShader, viewProjection, PineTree.get<Transform>(), view, viewProjection);

			PineTree2.get<MorphRenderer>().nextFrame(dt);
			PineTree2.get<MorphRenderer>().render(morphShader, viewProjection, PineTree2.get<Transform>(), view, viewProjection);

			PineTree3.get<MorphRenderer>().nextFrame(dt);
			PineTree3.get<MorphRenderer>().render(morphShader, viewProjection, PineTree3.get<Transform>(), view, viewProjection);

			PineTree4.get<MorphRenderer>().nextFrame(dt);
			PineTree4.get<MorphRenderer>().render(morphShader, viewProjection, PineTree4.get<Transform>(), view, viewProjection);




			//Update Animation
			/*if (RenderGroupBool == 1 || RenderGroupBool == 0)
			{
				Wizard.get<MorphRenderer>().nextFrame(dt);


				// SetupShaderForFrame(morphShader, view, projection);
				Wizard.get<MorphRenderer>().render(morphShader, viewProjection, Wizard.get<Transform>(), view, viewProjection);



				Phantom.get<MorphRenderer>().nextFrame(dt);


				// SetupShaderForFrame(morphShader, view, projection);
				Phantom.get<MorphRenderer>().render(morphShader, viewProjection, Phantom.get<Transform>(), view, viewProjection);

				//Update Animation
				Phantom2.get<MorphRenderer>().nextFrame(dt);


				// SetupShaderForFrame(morphShader, view, projection);
				Phantom2.get<MorphRenderer>().render(morphShader, viewProjection, Phantom2.get<Transform>(), view, viewProjection);



				Coin.get<MorphRenderer>().nextFrame(dt);


				// SetupShaderForFrame(morphShader, view, projection);
				Coin.get<MorphRenderer>().render(morphShader, viewProjection, Coin.get<Transform>(), view, viewProjection);
			}*/
			

			

			


			RenderImGui();   

			glfwSwapBuffers(window);
			lastFrame = thisFrame;    
		}
		
		
		//deletePhysics();
		///-----cleanup_start-----
		
		//remove the rigidbodies from the dynamics world and delete them
		for (int i = dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--)
		{
			btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
			btRigidBody* body = btRigidBody::upcast(obj);
			if (body && body->getMotionState())
			{
				delete body->getMotionState();
			}
			dynamicsWorld->removeCollisionObject(obj);
			delete obj;
		}

		//delete collision shapes
		for (int j = 0; j < collisionShapes.size(); j++)
		{
			btCollisionShape* shape = collisionShapes[j];
			collisionShapes[j] = 0;
			delete shape;
		}
		
		//delete dynamics world
		delete dynamicsWorld;

		//delete solver
		delete solver;

		//delete broadphase
		delete overlappingPairCache;

		//delete dispatcher
		delete dispatcher;

		delete collisionConfiguration;
		

		ShutdownImGui();

		// Clean up the toolkit logger so we don't leak memory
		Application::Instance().ActiveScene = nullptr;
	}
	Logger::Uninitialize();
	return 0;
}