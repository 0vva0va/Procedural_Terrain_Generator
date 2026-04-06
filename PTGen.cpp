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

constexpr int threadsX = (TERRAIN_SIZE * 2 / 8) + 1;
constexpr int threadsY = (TERRAIN_SIZE * 2 / 8) + 1;

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
void ComputeNoiseDispatch(Terrain& terrain, ComputeShader& noiseComp)
{
    const TerrainParams& p = terrain.GetParams();
    
    noiseComp.Use();
    terrain.BindSSBOBuffers();
    noiseComp.SetInt  ("_TerrainSize",     TERRAIN_SIZE * 2 + 1);
    noiseComp.SetInt  ("_Octaves",         p.octaves);
    noiseComp.SetInt  ("_Seed",            p.seed);
    noiseComp.SetFloat("_Frequency",       p.frequency);
    noiseComp.SetFloat("_Amplitude",       p.amplitude);
    noiseComp.SetFloat("_Lacunarity",      p.lacunarity);
    noiseComp.SetFloat("_Persistance",     p.persistence);
    noiseComp.SetFloat("_HydroStrength",   p.hydro);
    noiseComp.SetFloat("_ThermalStrength", p.thermal);

    glDispatchCompute(threadsX, threadsY, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
}


void RenderScene(
    Shader&           shader,
    Terrain&          terrain,
    const Camera&     camera,
    const glm::mat4&  view,
    const glm::mat4&  proj,
    bool              depthPassOnly = false
)
{
    glm::mat4 model = glm::mat4(1.0f);

    // ------------- draw terrain -------------
    shader.Use();
    shader.SetMat4("_Model", model);
    shader.SetMat4("_MVP",   proj * view * model);
    shader.SetInt ("_TerrainWidth", TERRAIN_SIZE + 1);

    if (!depthPassOnly)
    {
        shader.SetVec3 ("_CamPos", camera.position);
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

    Shader terrainShader    ("Shaders/terrain.vert", "Shaders/terrain.frag");
    Shader simpleDepthShader("Shaders/shadows.vert", "Shaders/shadows.frag", "Shaders/shadows.geometry");
    Shader skyboxShader     ("Shaders/skybox.vert",  "Shaders/skybox.frag");
    Shader irradianceShader ("Shaders/cubemap.vert", "Shaders/irradiance.frag");
    Shader prefilterShader  ("Shaders/cubemap.vert", "Shaders/prefilter.frag");
    Shader brdfShader       ("Shaders/brdf.vert",    "Shaders/brdf.frag");

    Terrain terrain(TERRAIN_SIZE);
    terrain.SetupNoiseSSBO();

    Skybox skybox;
    skybox.BakeIrradiance(irradianceShader);
    skybox.SetupPrefilterMap(prefilterShader);
    skybox.SetupBrdfLUTTex(brdfShader);
    
    
    
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
    float frameTime = 0.0f;

    // -------------------------------------------------------------------------------------
    // -----------------   All parameters relating to the terrain --------------------------
    // -------------------------------------------------------------------------------------
    // ------------- pbr lighting -------------
    float albedo[3] = { 0.61f, 0.46f, 0.33f };
    float metallic  = 0.0f;
    float roughness = 1.0f;
    float ambOcc    = 1.0f;

    float lightDir[2]    = { -40.0f, 2.0f };
    float lightCol[3]    = { 1.0f, 1.0f, 1.0f };
    float lightIntensity = 10.0f;
    float envLightStr    = 0.1f;

    const float nearPlane = 0.1f;
    const float farPlane  = 1000.0f;
    
    // ------------- fog settings -------------
    float fogDensity    = 0.003f;
    float fogHeightMin  = 0.0f;      
    float fogHeightMax  = 10.0f;     
    
    float wispScale     = 0.1f;   // Spatial scale of fog blobs (smaller = bigger blobs)
    float wispSpeed     = 1.0f;   // Drift speed
    float wispStrength  = 0.8f;   // How much density varies (0 = constant, 1 = full variation)
    float wispHeightAtt = 2.0f;   // How much vertical variation differs from horizontal
    
    float be[3] = { 0.25f, 0.10f, 0.05f }; 
    float bi[3] = { 0.05f, 0.15f, 0.40f }; 
    
    float fogColorBase[3] = { 0.2f, 0.3f, 0.4f };  
    float fogColorSun[3]  = { 1.0f, 0.9f, 0.7f };  
    float sunPower = 8.0f;  
    
    
    // ------------------------------------------
    ComputeNoiseDispatch(terrain, noiseCalcComp);
    
    
    // ------------------------------------------
    // ------------- GLFW MAIN LOOP -------------
    // ------------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        float now  = (float)glfwGetTime();
        float dt   = now - lastTime;
        lastTime   = now;
        frameTime += dt;

        glfwPollEvents();
        camera.Update(dt);

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        float aspect = (float)w / (float)h;

        // Build camera matrices once per frame
        glm::mat4 view = camera.Get_View_Matrix();
        glm::mat4 proj = camera.Get_Projection_Matrix(aspect);

        // ------------ shadow map parameters --------------
        glm::vec3 lightDirV = glm::normalize(glm::vec3(lightDir[0], -1.0f, lightDir[1]));

        shadowMap.cam = camera;
        shadowMap.lightDir = lightDirV;
        shadowMap.camNearPlane = nearPlane;
        shadowMap.camFarPlane  = farPlane;
        shadowMap.fbHeight = h;
        shadowMap.fbWidth  = w;

        // Pre pass
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
           
        
        // PASS 1 — Depth / shadow cubemap  
        // ===============================
        const auto lightMatrices = shadowMap.GetLightSpaceMatrices();
        glBindBuffer(GL_UNIFORM_BUFFER, shadowMap.GetMatricesUBO());
        
        for (size_t i = 0; i < lightMatrices.size(); ++i)
            glBufferSubData(GL_UNIFORM_BUFFER, i * sizeof(glm::mat4x4), sizeof(glm::mat4x4), &lightMatrices[i]);
        
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        
        simpleDepthShader.Use();
        
        glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.GetLightFBO());
            glViewport(0, 0, shadowMap.DEPTH_MAP_RES, shadowMap.DEPTH_MAP_RES);
            glClear(GL_DEPTH_BUFFER_BIT);
            glCullFace(GL_FRONT);                                                                   // peter panning
            RenderScene(simpleDepthShader, terrain, camera, view, proj, /*depthPassOnly=*/true);
            glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        // reset viewport
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        
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
        terrainShader.SetFloat("_FarPlane", farPlane);
        terrainShader.SetFloat("_Time",     frameTime);
        terrainShader.SetInt  ("_IrradianceMap", 1);
        terrainShader.SetInt  ("_PrefilterMap", 2);
        terrainShader.SetInt  ("_BrdfLUT", 3);
        
        // fog settings
        terrainShader.SetVec3("_BE", glm::vec3(be[0], be[1], be[2]));
        terrainShader.SetVec3("_BI", glm::vec3(bi[0], bi[1], bi[2]));
        terrainShader.SetVec3("_FogColorBase", glm::vec3(fogColorBase[0], fogColorBase[1], fogColorBase[2]));
        terrainShader.SetVec3("_FogColorSun",  glm::vec3(fogColorSun[0], fogColorSun[1], fogColorSun[2]));
        terrainShader.SetFloat("_FogDensity", fogDensity);
        terrainShader.SetFloat("_SunPower",   sunPower);
        terrainShader.SetFloat("_FogHeightMin", fogHeightMin);
        terrainShader.SetFloat("_FogHeightMax", fogHeightMax);
        terrainShader.SetFloat("_WispScale", wispScale);
        terrainShader.SetFloat("_WispSpeed", wispSpeed);
        terrainShader.SetFloat("_WispStrength",  wispStrength);
        terrainShader.SetFloat("_WispHeightAtt", wispHeightAtt);
        
        terrainShader.SetMat4("_View", view);
        terrainShader.SetInt("_ShadowMap", 4); 
        terrainShader.SetInt("_CascadeCount", shadowMap.shadowCascadeLevels.size());
        
        for (size_t i = 0; i < shadowMap.shadowCascadeLevels.size(); ++i)
            terrainShader.SetFloat("_CascadePlaneDistances[" + std::to_string(i) + "]", shadowMap.shadowCascadeLevels[i]);
        
        // ------ texture binds ------
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.GetIrradianceMap());
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.GetPrefilterMap());
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D,       skybox.GetBrdfTexture());
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMap.GetLightDepthMap());

        
        RenderScene(terrainShader, terrain, camera, view, proj, /*depthPassOnly=*/false);

        
        // Skybox
        glDepthFunc(GL_LEQUAL);
            skyboxShader.Use();
            skyboxShader.SetInt ("_Skybox", 0);
            skyboxShader.SetMat4("_Projection", proj);
            skyboxShader.SetMat4("_View", glm::mat4(glm::mat3(view)));
            skybox.Draw();
        glDepthFunc(GL_LESS);


        // ----- imgui frame ------
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        static TerrainParams uiParams = terrain.GetParams();

        ImVec2 winPos   = ImVec2(ImGui::GetIO().DisplaySize.x - padding, padding);
        ImVec2 winPivot = ImVec2(1.0f, 0.0f);
        ImGui::SetNextWindowPos (winPos,         ImGuiCond_Always, winPivot);
        ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_Always);

        ImGui::Begin("Controls");
        
        if(ImGui::CollapsingHeader("Terrain Noise Settings"))
        {
            bool changed = false;
            changed |= ImGui::SliderFloat("Frequency",       &uiParams.frequency,  0.001f, 0.1f);
            changed |= ImGui::SliderFloat("Amplitude",       &uiParams.amplitude,  1.0f,  50.0f);
            changed |= ImGui::SliderInt  ("Octaves",         &uiParams.octaves,    1,     12);
            changed |= ImGui::SliderFloat("Lacunarity",      &uiParams.lacunarity, 1.5f,  3.0f);
            changed |= ImGui::SliderFloat("Persistence",     &uiParams.persistence,0.0f,  1.0f);
            changed |= ImGui::SliderFloat("Hydro Erosion",   &uiParams.hydro,      0.0f,  1.0f);
            changed |= ImGui::SliderFloat("Thermal Erosion", &uiParams.thermal,    0.0f,  1.0f);
            changed |= ImGui::InputInt   ("Seed",            &uiParams.seed);
    
            if (changed) 
            {
                terrain.SetParams(uiParams);
                ComputeNoiseDispatch(terrain, noiseCalcComp);
            }
        }
        if(ImGui::CollapsingHeader("Lighting"))
        {
            ImGui::ColorEdit3  ("Albedo (Colour)",             albedo);
            ImGui::SliderFloat ("Metallic",                    &metallic,       0.0f, 1.0f);
            ImGui::SliderFloat ("Roughness",                   &roughness,      0.0f, 1.0f);
            ImGui::SliderFloat ("Ambient Occlusion",           &ambOcc,         0.0f, 1.0f);
            ImGui::SliderFloat2("Light Direction",             lightDir,      -40.0f, 40.0f);
            ImGui::ColorEdit3  ("Light Colour",                lightCol);
            ImGui::SliderFloat ("Light Intensity",             &lightIntensity, 0.01f, 100.0f);
            ImGui::SliderFloat ("Environment Light Intensity", &envLightStr,    0.0f, 10.0f);
        }
        if(ImGui::CollapsingHeader("Fog Settings")) 
        {
            ImGui::SliderFloat("Fog Density", &fogDensity, 0.000f, 0.005f);
            ImGui::ColorEdit3("Extinction Colour",   be);
            ImGui::ColorEdit3("Inscattering Colour", bi);
            ImGui::ColorEdit3("Base Fog Colour",     fogColorBase);
            ImGui::ColorEdit3("Sun Fog Colour",      fogColorSun);
            ImGui::SliderFloat("Sun Power",      &sunPower, 1.0f, 10.0f);
            ImGui::SliderFloat("Fog Min Height", &fogHeightMin, 0.0f, 20.0f);
            ImGui::SliderFloat("Fog Max Height", &fogHeightMax, 0.0f, 100.0f);
            ImGui::SliderFloat("Wisp Scale",     &wispScale, 0.0f, 1.0f);
            ImGui::SliderFloat("Wisp Speed",     &wispSpeed, 0.0f, 2.0f);
            ImGui::SliderFloat("Wisp Strength",  &wispStrength, 0.0f, 1.0f);
            ImGui::SliderFloat("Wisp Height Attenuation", &wispHeightAtt, 0.0f, 5.0f);
        }
        if(ImGui::CollapsingHeader("Camera Settings"))
        {
            ImGui::SliderFloat("Speed", &camera.speed, 1.0f, 100.0f);
            ImGui::SliderFloat("FOV",   &camera.fov,  30.0f, 120.0f);
        }

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