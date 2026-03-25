#include <iostream>
#include <random>

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "shader_c.h"
#include "shader_t.h"

#include "Camera.h"
#include "Scripts/Terrain/Terrain.h"
#include "Scripts/Skybox/Skybox.h"



#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT 720

#define TERRAIN_SIZE  512

constexpr int threadsX = (TERRAIN_SIZE / 8) + 1;
constexpr int threadsY = (TERRAIN_SIZE / 8) + 1;

const float padding   = 10.0f;  // imgui padding from screen edge
static bool wireframe = false;  // wireframe mode toggle



// ------------- camera setup + callbacks -------------
static Camera* gCamera     = nullptr;
static bool gCameraActive  = true;


void KeyCallback(GLFWwindow* window, int key, int, int action, int) 
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) 
    {
        gCameraActive = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        if (gCamera) gCamera -> Reset_Mouse();
        return;
    }
    if (gCamera) gCamera -> On_Key(key, action);
}

void MouseCallback(GLFWwindow*, double x, double y) 
{
    if (gCamera && gCameraActive) gCamera -> On_Mouse_Move(x, y);
}

void MouseButtonCallback(GLFWwindow* window, int button, int action, int) 
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse) 
        {
            gCameraActive = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

            if (gCamera) gCamera -> Reset_Mouse();
        }
    }
}


// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------

int main() 
{
    // -------------glfw init -------------
    if (!glfwInit()) 
    {
        std::cerr << "Failed to init GLFW\n"; 
        return -1; 
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); 
    glfwWindowHint(GLFW_SAMPLES, 4);
     

    // ------------- glfw window -------------
    GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Procedural Terrain", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    
    // ------------- glad init -------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) 
    {
        std::cerr << "Failed to init GLAD\n"; 
        return -1;
    }

    glEnable(GL_CULL_FACE);  
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);  

    
    Camera camera;
    gCamera = &camera;

    glfwSetKeyCallback(window, KeyCallback);
    glfwSetCursorPosCallback(window, MouseCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    
    // ------------- terrain init -------------
    ComputeShader noiseCalcComp("Shaders/noise.comp");
     
    Shader terrainShader("Shaders/terrain.vert", "Shaders/terrain.frag");
    Terrain terrain(TERRAIN_SIZE);

    terrain.SetupNoiseSSBO();
    
    Shader skyboxShader("Shaders/skybox.vert", "Shaders/skybox.frag");
    Skybox skybox;
    skyboxShader.Use();
    skyboxShader.SetInt("skybox", 0);
    
    
    // ------------- imgui init -------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io     = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");

    
    // ------------- timing -------------
    float lastTime = (float)glfwGetTime();

    
    // ------------------------------------------
    // ------------- GLFW MAIN LOOP -------------
    while (!glfwWindowShouldClose(window)) 
    {
        float now = (float)glfwGetTime();
        float dt  = now - lastTime;
        lastTime  = now;

        glfwPollEvents();

        camera.Update(dt);

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        float aspect = (float)w / (float)h;

        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ------------- render terrain -------------
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view  = camera.Get_View_Matrix();
        glm::mat4 proj  = camera.Get_Projection_Matrix(aspect);
        
        const TerrainParams& p = terrain.GetParams();
        
        noiseCalcComp.Use();
        terrain.BindSSBOBuffers();
        noiseCalcComp.SetInt  ("_TerrainSize", TERRAIN_SIZE + 1);
        noiseCalcComp.SetInt  ("u_octaves", p.octaves);
        noiseCalcComp.SetInt  ("u_seed",    p.seed);
        noiseCalcComp.SetFloat("u_frequency", p.frequency);
        noiseCalcComp.SetFloat("u_amplitude", p.amplitude);
        noiseCalcComp.SetFloat("u_lacunarity",  p.lacunarity);
        noiseCalcComp.SetFloat("u_persistance", p.persistence);
        noiseCalcComp.SetFloat("u_hydro_strength",   p.hydro);
        noiseCalcComp.SetFloat("u_thermal_strength", p.thermal);
        noiseCalcComp.SetVec3("u_camera_pos", camera.position);
        noiseCalcComp.SetFloat("u_fog_density", p.fog_density);
        
        glDispatchCompute(threadsX, threadsY, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);

        terrainShader.Use();
        terrainShader.SetMat4("Model", model);
        terrainShader.SetMat4("MVP", proj * view * model);
        terrainShader.SetInt("_TerrainWidth", TERRAIN_SIZE + 1);

        terrain.Draw();
        
        glDepthFunc(GL_LEQUAL);
        skyboxShader.Use();

        view = glm::mat4(glm::mat3(camera.Get_View_Matrix()));
        skyboxShader.SetMat4("view",       view);
        skyboxShader.SetMat4("projection", proj);

        skybox.Draw();
        glDepthFunc(GL_LESS);

        
        // ------------- imgui frame -------------
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        static TerrainParams uiParams = terrain.GetParams();

        ImVec2 winPos   = ImVec2(ImGui::GetIO().DisplaySize.x - padding,padding);
        ImVec2 winPivot = ImVec2(1.0f, 0.0f);
        ImGui::SetNextWindowPos(winPos, ImGuiCond_Always, winPivot);
        ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_Always);

        ImGui::Begin("Controls");

        bool changed = false;
        changed |= ImGui::SliderFloat("Frequency", &uiParams.frequency, 0.001f, 0.1f);
        changed |= ImGui::SliderFloat("Amplitude", &uiParams.amplitude, 1.0f, 50.0f);
        changed |= ImGui::SliderInt("Octaves", &uiParams.octaves, 1, 12);
        changed |= ImGui::SliderFloat("Lacunarity", &uiParams.lacunarity, 1.5f, 3.0f);
        changed |= ImGui::SliderFloat("Persistence", &uiParams.persistence, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Hydro Erosion", &uiParams.hydro, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Thermal Erosion", &uiParams.thermal, 0.0f, 1.0f);
        changed |= ImGui::InputInt("Seed", &uiParams.seed);
        changed |= ImGui::SliderFloat("Fog Density", &uiParams.fog_density, 0.00001f, 0.005f);

        if (changed) { terrain.SetParams(uiParams); }

        ImGui::Separator();

        ImGui::Text("Camera");
        ImGui::SliderFloat("Speed", &camera.speed, 1.0f, 100.0f);
        ImGui::SliderFloat("FOV",   &camera.fov,  30.0f, 120.0f);

        ImGui::Separator();

        ImGui::Text("Wireframe Mode");
        ImGui::Text(wireframe ? "Enabled" : "Disabled");
        if (ImGui::IsItemClicked(ImGui::Button("Use Wireframe"))) 
        {
            wireframe = !wireframe;
            wireframe ? glPolygonMode(GL_FRONT_AND_BACK, GL_LINE) : glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        ImGui::Separator();
        ImGui::Text("FPS: %.1f", 1.0f / dt);

        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // ------------- cleanup -------------
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}
