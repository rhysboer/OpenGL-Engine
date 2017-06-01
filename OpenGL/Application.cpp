#include "Application.h"

// remove
#include "BoundingSphere.h"
#include "Frustum.h"


Application::Application(int width, int height, const char* name) : BaseApplication::BaseApplication(width, height, name) {
}

Application::~Application() {
}

// Opengl ERROR CHECK
static void APIENTRY openglCallbackFunction( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam ) {
	(void)source; (void)type; (void)id; (void)severity; (void)length; (void)userParam;
	fprintf(stderr, "%s\n", message);
	if(severity == GL_DEBUG_SEVERITY_HIGH) {
		fprintf(stderr, "Aborting...\n");
		abort();
	}
}

// Start Up
const bool Application::Startup() {
	if(window == nullptr) {
		glfwTerminate();
		return false;
	}

	glfwMakeContextCurrent(window);

	if(ogl_LoadFunctions() == ogl_LOAD_FAILED) {
		glfwDestroyWindow(window);
		glfwTerminate();
		return false;
	}

	// Background color
	glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
	glEnable(GL_DEPTH_TEST);


	// Enable the debug callback
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(openglCallbackFunction, nullptr);
	glDebugMessageControl(
		GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, true
	);

	// Camera Settings
	m_camera.SetLookAt(vec3(1), vec3(0), vec3(0, 1, 0));
	m_camera.SetPerspective(glm::pi<float>() * 0.25f, 16 / 9.f, 0.1f, 1000.0f);
	m_camera.SetPosition(vec3(0, 10, 38));

	// QUANTERNIONS TEST
	m_position[0] = vec3(10, 5, 10);
	m_position[1] = vec3(-10, 0, -10);
	m_rotation[0] = quat(vec3(0, -1, 0));
	m_rotation[1] = quat(vec3(0, 1, 0));

	// Terrain 
	m_terrain.init(50, 50);
	m_terrain.TotalTextureRepeat(uvec2(10, 10));

	m_objLoader = new OBJLoader();
	m_objLoader->LoadObject("../bin/objs/soulspear.obj");

	m_effects = new PostProcessing();

	// Particle Emitter
	m_particleEmitter = new ParticleEmitter();
	m_particleEmitter->Initialise(1000, 500, 0.1f, 2.0f, 1, 5, 1, 0.1f, Colours::Black, Colours::Purple);

	m_sunMat = glm::translate(m_sunMat, vec3(0));
	m_earthLocal = glm::translate(m_earthLocal, vec3(5, 0, 0));
	m_moonLocal = glm::translate(m_moonLocal, vec3(2, 0, 0));

	// --------------------------------------
	//			 Physics Manager
	// --------------------------------------
	m_physicsManager = new PhysicsManager();
	m_physicsManager->SetGravity(vec2(0, -9.8f)); // -0.08f
	m_physicsManager->SetTimeStep(0.01f);
	
	for(int i = 0; i < 2; i++) {

		Box* box = new Box(vec2(-10 * i, 20), 2, 2, 5,Colours::Blue);
		//box->ApplyForce(vec2(0, -3.0f));
		m_physicsManager->AddActor(box);

	}

	m_sphere[0] = new Sphere(vec2(0 * 10, 55), vec2(0, 0), 100, 5, Colours::Orange);
	m_sphere[1] = new Sphere(vec2(1 * 50, 55), vec2(0, 0), 100, 5, Colours::Yellow);
	m_sphere[1]->ApplyForce(vec2(-250, 0));
	m_physicsManager->AddActor(m_sphere[0]);
	m_physicsManager->AddActor(m_sphere[1]);

	Plane* plane = new Plane(vec2(0, 1), 0);
	m_physicsManager->AddActor(plane);



	Gizmos::create();
	return true;
}

// Update
const bool Application::Update() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	Gizmos::clear();
	Gizmos::addTransform(glm::mat4(1));

	// Quit is ESC is pressed or window is closed
	if(glfwWindowShouldClose(window) == (int)true || InputManager::IsKeyDown(GLFW_KEY_ESCAPE)) {
		return false;
	}

	// Rotate Planets
	m_sunMat = glm::rotate(m_sunMat, -0.01f, vec3(0, 1, 0));
	m_earthLocal = glm::rotate(m_earthLocal, -0.01f, vec3(0, 1, 0));
	m_moonLocal = glm::rotate(m_moonLocal, 1.0f, vec3(0, 1, 0));

	m_earthMat = m_sunMat * m_earthLocal;
	m_moonMat = m_earthMat * m_moonLocal;

	Gizmos::addSphere(vec3(m_sunMat[3]), 1.f, 25, 25, Colours::Red, &m_sunMat);
	Gizmos::addSphere(vec3(m_earthMat[3]), 0.5f, 20, 20, Colours::Green, &m_earthMat);
	Gizmos::addSphere(vec3(m_moonMat[3]), 0.2f, 10, 10, Colours::Purple, &m_moonMat);

	// Grid
	for(int i = 0; i < 21; ++i) {
		Gizmos::addLine(vec3(-10 + i, 0, 10), vec3(-10 + i, 0, -10), i == 10 ? Colours::White : Colours::Black);
		Gizmos::addLine(vec3(10, 0, -10 + i), vec3(-10, 0, -10 + i), i == 10 ? Colours::White : Colours::Black);
	}


	/* Frustum Culling */
	//////////////////////////////////////////
	vec4 planes[6];
	Frustum::GetFrustumPlanes(m_camera.GetProjectionView(), planes);

	vec4 plane(0, 1, 0, -1);
	vec4 planeColour = Colours::Green;

	BoundingSphere sphere;
	sphere.centre = vec3(5, cosf((float)glfwGetTime()) + 1, 0);
	sphere.radius = 0.5f;

	for(int i = 0; i < 6; i++) {
		float d = glm::dot(vec3(planes[i]), sphere.centre) + planes[i].w;

		if(d < -sphere.radius) {
			printf("Behind, dont render it! \n");
			break;
		}

		if(i == 5) {
			Gizmos::addSphere(sphere.centre, sphere.radius, 8, 8, Colours::Purple);
		}
	}
	//////////////////////////////////////////

	if(InputManager::IsKeyDown(GLFW_KEY_T))
		m_sphere[0]->ApplyForceToActor(m_sphere[1], vec2(0.1f, 0));
	
	m_physicsManager->Update(Time::DeltaTime());
	m_physicsManager->UpdateGizmos();
	m_physicsManager->DebugScene();
	


	// Update Particles
	m_particleEmitter->Update(m_camera.GetWorldTransform());

	// Camera Movement
	m_camera.Update();

	// Terrain GUI
	//ImGui::Begin("Terrain Settings");
	//if(ImGui::Button("Generate New Terrain")) {
	//	m_terrain.GenerateGrid();
	//}
	//ImGui::End();

	return true;
}

void Application::Draw() {
	// Start Post Processing
	m_effects->BeginRender();

	m_particleEmitter->Draw(m_camera);
	m_terrain.Draw(m_camera);
	m_objLoader->Draw(m_camera);

	Gizmos::draw(m_camera.GetProjectionView());
	m_physicsManager->Draw(m_camera.GetProjectionView());

	// Render Post Processing
	m_effects->EndRender();
}

void Application::Shutdown() {
	Gizmos::destroy();
	LightManager::Destroy();

	// Delete 
	if(m_effects != nullptr) {
		delete m_effects;
		m_effects = nullptr;
	}
	if(m_particleEmitter != nullptr) {
		delete m_particleEmitter;
		m_particleEmitter = nullptr;
	}
	if(m_objLoader != nullptr) {
		delete m_objLoader;
		m_objLoader = nullptr;
	}
	if(m_animation != nullptr) {
		delete m_animation;
		m_animation = nullptr;
	}

	glfwDestroyWindow(window);
	glfwTerminate();
}