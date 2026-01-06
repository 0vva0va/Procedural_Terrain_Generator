#include <iostream>
#include <random>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "camera.h"
#include "terrain.h"
#include "shader_t.h"
#include "imgui_config.h"


#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT 720
#define TERRAIN_SIZE  512

const float padding   = 10.0f;  // imgui padding from screen edge
static bool wireframe = false;  // wireframe mode toggle

// ------------- camera setup + callbacks -------------
static Camera* g_camera     = nullptr;
static bool g_camera_active = true;

void KeyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        g_camera_active = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        if (g_camera) g_camera -> Reset_Mouse();
        return;
    }
    if (g_camera) g_camera -> On_Key(key, action);
}

void MouseCallback(GLFWwindow*, double x, double y) {
    if (g_camera && g_camera_active) g_camera -> On_Mouse_Move(x, y);
}

void MouseButtonCallback(GLFWwindow* window, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse) {
            g_camera_active = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

            if (g_camera) g_camera -> Reset_Mouse();
        }
    }
}

// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------

int main() {
    // -------------glfw init -------------
    if (!glfwInit()) {
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

    // -------------glad init -------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n"; 
        return -1;
    }

    glEnable(GL_CULL_FACE);  
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);  

    
    Camera camera;
    g_camera = &camera;

    glfwSetKeyCallback(window, KeyCallback);
    glfwSetCursorPosCallback(window, MouseCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // ------------- terrain init -------------
    Shader terrain_shader("shaders/terrain.vert", "shaders/terrain.frag");
    Terrain terrain(TERRAIN_SIZE);

    // ------------- imgui init -------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io     = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGuiStyleConfig::ApplyDarkTheme();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");

    // ------------- timing -------------
    float last_time = (float)glfwGetTime();

    
    // ------------------------------------------
    // ------------- GLFW MAIN LOOP -------------
    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        float dt  = now - last_time;
        last_time = now;

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

        terrain_shader.Use();
        terrain_shader.Set_Mat4("Model", model);
        terrain_shader.Set_Mat4("MVP", proj * view * model);

        const Terrain_Params& p = terrain.Get_Params();
        // vert shader uniforms
        terrain_shader.Set_Int  ("u_octaves", p.octaves);
        terrain_shader.Set_Int  ("u_seed",    p.seed);
        terrain_shader.Set_Float("u_frequency", p.frequency);
        terrain_shader.Set_Float("u_amplitude", p.amplitude);
        terrain_shader.Set_Float("u_lacunarity",  p.lacunarity);
        terrain_shader.Set_Float("u_persistance", p.persistence);
        terrain_shader.Set_Float("u_hydro_strength",   p.hydro);
        terrain_shader.Set_Float("u_thermal_strength", p.thermal);
        // frag shader uniforms
        terrain_shader.Set_Vec3("u_camera_pos", camera.position);
        terrain_shader.Set_Float("u_fog_density", p.fog_density);

        terrain.Draw();

        // ------------- imgui frame -------------
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        static Terrain_Params ui_params = terrain.Get_Params();

        ImVec2 window_pos = ImVec2(
            ImGui::GetIO().DisplaySize.x - padding,
            padding
        );
        ImVec2 window_pivot = ImVec2(1.0f, 0.0f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pivot);
        ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_Always);

        ImGui::Begin("Controls");

        bool changed = false;
        changed |= ImGui::SliderFloat("Frequency", &ui_params.frequency, 0.001f, 0.1f);
        changed |= ImGui::SliderFloat("Amplitude", &ui_params.amplitude, 1.0f, 50.0f);
        changed |= ImGui::SliderInt("Octaves", &ui_params.octaves, 1, 12);
        changed |= ImGui::SliderFloat("Lacunarity", &ui_params.lacunarity, 1.5f, 3.0f);
        changed |= ImGui::SliderFloat("Persistence", &ui_params.persistence, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Hydro Erosion", &ui_params.hydro, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Thermal Erosion", &ui_params.thermal, 0.0f, 1.0f);
        changed |= ImGui::InputInt("Seed", &ui_params.seed);
        changed |= ImGui::SliderFloat("Fog Density", &ui_params.fog_density, 0.00001f, 0.005f);

        if (changed)
            terrain.Set_Params(ui_params);

        ImGui::Separator();

        ImGui::Text("Camera");
        ImGui::SliderFloat("Speed", &camera.speed, 1.0f, 100.0f);
        ImGui::SliderFloat("FOV", &camera.fov, 30.0f, 120.0f);

        ImGui::Separator();

        ImGui::Text("Wireframe Mode");
        ImGui::Text(wireframe ? "Enabled" : "Disabled");
        if (ImGui::IsItemClicked(ImGui::Button("Use Wireframe"))) {
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
