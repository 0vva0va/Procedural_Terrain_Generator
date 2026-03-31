#include <iostream>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "shader_c.h"
#include "shader_t.h"

#include "Camera.h"
#include "Scripts/CascadeShadows/ShadowMap.h"
#include "Scripts/Terrain/Terrain.h"
#include "Scripts/Skybox/Skybox.h"




#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT 720

#define TERRAIN_SIZE  512

#define SHADOW_MAP_RES 4096

constexpr int threadsX = (TERRAIN_SIZE*2 / 8) + 1;
constexpr int threadsY = (TERRAIN_SIZE*2 / 8) + 1;

const float padding   = 10.0f;
static bool wireframe = false;



// ------------- camera setup + callbacks -------------
static Camera* gCamera    = nullptr;
static bool gCameraActive = true;


void KeyCallback(GLFWwindow* window, int key, int, int action, int)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        gCameraActive = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        if (gCamera) gCamera->Reset_Mouse();
        return;
    }
    if (gCamera) gCamera->On_Key(key, action);
}

void MouseCallback(GLFWwindow*, double x, double y)
{
    if (gCamera && gCameraActive) gCamera->On_Mouse_Move(x, y);
}

void MouseButtonCallback(GLFWwindow* window, int button, int action, int)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse)
        {
            gCameraActive = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (gCamera) gCamera->Reset_Mouse();
        }
    }
}


// -------------------------------------------------------------------------------------
// RenderScene
//   Dispatches the noise compute shader then draws the terrain.
//
//   depthPassOnly — when true, skips uniforms that are irrelevant for a depth-only
//   render (PBR, IBL, fog…)
// -------------------------------------------------------------------------------------
void RenderScene(
    Shader&            shader,
    ComputeShader&     noiseComp,
    Terrain&           terrain,
    const Camera&      camera,
    const glm::mat4&   view,
    const glm::mat4&   proj,
    bool               depthPassOnly = false)
{
    glm::mat4 model = glm::mat4(1.0f);
    const TerrainParams& p = terrain.GetParams();

    // ------------- noise compute -------------
    noiseComp.Use();
    terrain.BindSSBOBuffers();
    noiseComp.SetInt  ("_TerrainSize",       TERRAIN_SIZE*2 + 1);
    noiseComp.SetInt  ("u_octaves",          p.octaves);
    noiseComp.SetInt  ("u_seed",             p.seed);
    noiseComp.SetFloat("u_frequency",        p.frequency);
    noiseComp.SetFloat("u_amplitude",        p.amplitude);
    noiseComp.SetFloat("u_lacunarity",       p.lacunarity);
    noiseComp.SetFloat("u_persistance",      p.persistence);
    noiseComp.SetFloat("u_hydro_strength",   p.hydro);
    noiseComp.SetFloat("u_thermal_strength", p.thermal);

    glDispatchCompute(threadsX, threadsY, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    // ------------- draw terrain -------------
    shader.Use();
    shader.SetMat4("_Model", model);
    shader.SetMat4("_MVP",   proj * view * model);
    shader.SetInt ("_TerrainWidth", TERRAIN_SIZE + 1);

    if (!depthPassOnly)
    {
        shader.SetVec3 ("_CamPos", camera.position);
        shader.SetFloat("_FogDensity", p.fogDensity);
    }

    terrain.Draw();
}


// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------

int main()
{
    // ------------- glfw init -------------
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
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);


    Camera camera;
    gCamera = &camera;

    glfwSetKeyCallback       (window, KeyCallback);
    glfwSetCursorPosCallback (window, MouseCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);


    // ------------- terrain + skybox init -------------
    ComputeShader noiseCalcComp("Shaders/noise.comp");

    Shader terrainShader    ("Shaders/terrain.vert",     "Shaders/terrain.frag");
    Shader simpleDepthShader("Shaders/shadows.vert",     "Shaders/shadows.frag", "Shaders/shadows.geometry");
    Shader skyboxShader     ("Shaders/skybox.vert",      "Shaders/skybox.frag");
    Shader irradianceShader ("Shaders/irradiance.vert",  "Shaders/irradiance.frag");

    Terrain terrain(TERRAIN_SIZE);
    terrain.SetupNoiseSSBO();

    Skybox skybox;
    skybox.BakeIrradiance(irradianceShader);


    // ------------- shadow map framebuffer -------------
    ShadowMap shadowMap;

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);


    // ------------- imgui init -------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io    = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");


    // ------------- timing -------------
    float lastTime = (float)glfwGetTime();


    // ------------- pbr lighting -------------
    float albedo[3]  = { 0.61f, 0.46f, 0.33f };
    float metallic   = 0.0f;
    float roughness  = 1.0f;
    float ambOcc     = 1.0f;

    float lightDir[2]    = { -5.0f, 5.0f };
    float lightCol[3]    = { 1.0f,  1.0f,  1.0f };
    float lightIntensity = 1.0f;
    float envLightStr    = 0.1f;

    // Point light shadow depth range — tune farPlane to match your world scale
    const float nearPlane = 0.1f;
    const float farPlane  = 1000.0f;


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

        // Build camera matrices once per frame
        glm::mat4 view = camera.Get_View_Matrix();
        glm::mat4 proj = camera.Get_Projection_Matrix(aspect);

        // ------------- point-light cubemap matrices -------------
        glm::vec3 lightDirV = glm::vec3(lightDir[0], -1.0f, lightDir[1]);

        // 90° FOV + 1:1 aspect so each face covers exactly one cube quadrant
        // glm::mat4 lightProj = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);

        // std::vector<glm::mat4> lightSpaceMatrices = {
        //     lightProj * glm::lookAt(lightPosV, lightPosV + glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
        //     lightProj * glm::lookAt(lightPosV, lightPosV + glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
        //     lightProj * glm::lookAt(lightPosV, lightPosV + glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
        //     lightProj * glm::lookAt(lightPosV, lightPosV + glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
        //     lightProj * glm::lookAt(lightPosV, lightPosV + glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
        //     lightProj * glm::lookAt(lightPosV, lightPosV + glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0)),
        // };

        // Pre passes
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                
        // =====================================================================
        // PASS 1 — Depth / shadow cubemap  (geometry shader fans to all 6 faces)
        // =====================================================================
        // simpleDepthShader.Use();
        // for (int i = 0; i < 6; ++i)
        //     simpleDepthShader.SetMat4("_LightSpaceMatrices[" + std::to_string(i) + "]", lightSpaceMatrices[i]);

        // // Needed by the frag shader to write normalised linear depth
        // simpleDepthShader.SetVec3 ("_LightPos",  lightPosV);
        // simpleDepthShader.SetFloat("_FarPlane",  farPlane);

        // glViewport(0, 0, SHADOW_MAP_RES, SHADOW_MAP_RES);
        // glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.depthMapFBO);
        //     glClear(GL_DEPTH_BUFFER_BIT);
        //     // view/proj are unused in the depth pass — geometry shader owns the transform
        //     RenderScene(simpleDepthShader, noiseCalcComp, terrain, camera,
        //                 glm::mat4(1.0f), glm::mat4(1.0f), /*depthPassOnly=*/true);
        // glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        // glViewport(0, 0, w, h);
        // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        
        // =====================================================================
        // PASS 2 — Full scene
        // =====================================================================
        terrainShader.Use();
        terrainShader.SetVec3 ("_Albedo",    glm::vec3(albedo[0], albedo[1], albedo[2]));
        terrainShader.SetFloat("_Metallic",  metallic);
        terrainShader.SetFloat("_Roughness", roughness);
        terrainShader.SetFloat("_AO",        ambOcc);
        terrainShader.SetVec3 ("_LightDir",  lightDirV);
        terrainShader.SetVec3 ("_LightCol",  glm::vec3(lightCol[0], lightCol[1], lightCol[2]));
        terrainShader.SetFloat("_LightIntensity",           lightIntensity);
        terrainShader.SetFloat("_EnvironmentLightStrength", envLightStr);
        // Point-light shadow: pass farPlane so the frag shader can un-normalise the stored depth
        terrainShader.SetFloat("_FarPlane",      farPlane);
        terrainShader.SetInt  ("_ShadowMap",     2);   // texture unit 2  (samplerCube)
        terrainShader.SetInt  ("_IrradianceMap", 1);   // texture unit 1

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.GetIrradianceMap());

        // Bind the depth cubemap — fragment shader samples it with samplerCube
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, shadowMap.depthMap);

        RenderScene(terrainShader, noiseCalcComp, terrain, camera, view, proj, /*depthPassOnly=*/false);

        // Skybox
        glDepthFunc(GL_LEQUAL);
        skyboxShader.Use();
        skyboxShader.SetInt ("skybox",     0);
        skyboxShader.SetMat4("view",       glm::mat4(glm::mat3(view)));
        skyboxShader.SetMat4("projection", proj);
        skybox.Draw();
        glDepthFunc(GL_LESS);


        // ------------- imgui frame -------------
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        static TerrainParams uiParams = terrain.GetParams();

        ImVec2 winPos   = ImVec2(ImGui::GetIO().DisplaySize.x - padding, padding);
        ImVec2 winPivot = ImVec2(1.0f, 0.0f);
        ImGui::SetNextWindowPos (winPos,         ImGuiCond_Always, winPivot);
        ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_Always);

        ImGui::Begin("Controls");

        bool changed = false;
        changed |= ImGui::SliderFloat("Frequency",       &uiParams.frequency,  0.001f, 0.1f);
        changed |= ImGui::SliderFloat("Amplitude",       &uiParams.amplitude,  1.0f,  50.0f);
        changed |= ImGui::SliderInt  ("Octaves",         &uiParams.octaves,    1,     12);
        changed |= ImGui::SliderFloat("Lacunarity",      &uiParams.lacunarity, 1.5f,  3.0f);
        changed |= ImGui::SliderFloat("Persistence",     &uiParams.persistence,0.0f,  1.0f);
        changed |= ImGui::SliderFloat("Hydro Erosion",   &uiParams.hydro,      0.0f,  1.0f);
        changed |= ImGui::SliderFloat("Thermal Erosion", &uiParams.thermal,    0.0f,  1.0f);
        changed |= ImGui::InputInt   ("Seed",            &uiParams.seed);
        changed |= ImGui::SliderFloat("Fog Density",     &uiParams.fogDensity, 0.00001f, 0.005f);

        if (changed) terrain.SetParams(uiParams);

        ImGui::Separator();

        ImGui::Text("Lighting");
        ImGui::ColorEdit3  ("Albedo (Colour)",          albedo);
        ImGui::SliderFloat ("Metallic",                &metallic,       0.0f, 1.0f);
        ImGui::SliderFloat ("Roughness",               &roughness,      0.0f, 1.0f);
        ImGui::SliderFloat ("Ambient Occlusion",       &ambOcc,         0.0f, 1.0f);
        ImGui::SliderFloat2("Light Direction",         lightDir,     -40.0f, 40.0f);
        ImGui::ColorEdit3  ("Light Colour",             lightCol);
        ImGui::SliderFloat ("Light Intensity",         &lightIntensity, 0.01f, 1.0f);
        ImGui::SliderFloat ("Environment Light Intensity", &envLightStr, 0.0f, 10.0f);

        ImGui::Separator();

        ImGui::Text("Camera");
        ImGui::SliderFloat("Speed", &camera.speed, 1.0f,  100.0f);
        ImGui::SliderFloat("FOV",   &camera.fov,  30.0f,  120.0f);

        ImGui::Separator();

        ImGui::Text("Wireframe Mode");
        ImGui::Text(wireframe ? "Enabled" : "Disabled");
        if (ImGui::IsItemClicked(ImGui::Button("Use Wireframe")))
        {
            wireframe = !wireframe;
            wireframe ? glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)
                      : glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
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