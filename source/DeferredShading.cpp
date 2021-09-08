#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "glsw.h"
#include "model.h"
#include "shader_s.h"
#include "arcball_camera.h"
#include "framebuffer.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>            // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>            // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>          // Initialize with gladLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/Binding.h>  // Initialize with glbinding::Binding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/glbinding.h>// Initialize with glbinding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

#include <iostream>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#define PATH fs::current_path().generic_string()

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path, bool gammaCorrection);
void renderQuad();


// settings
const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;
const float MAX_CAMERA_DISTANCE = 200.0f;
const unsigned int LIGHT_GRID_WIDTH = 10;  // point light grid size
const unsigned int LIGHT_GRID_HEIGHT = 3;  // point light vertical grid height
const float INITIAL_POINT_LIGHT_RADIUS = 0.663f;

#define M_PI       3.14159265358979323846   // pi

// camera
ArcballCamera arcballCamera(glm::vec3(0.0f, 1.5f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

float lastX = (float)SCR_WIDTH / 2.0;
float lastY = (float)SCR_HEIGHT / 2.0;
bool firstMouse = true;
bool leftMouseButtonPressed = false;
bool rightMouseButtonPressed = false;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// struct to hold information about scene light
struct SceneLight {
    SceneLight(const glm::vec3& _position, const glm::vec3& _color, float _radius)
        : position(_position), color(_color), radius(_radius)
    {}
    glm::vec3 position;      // world light position
    glm::vec3 color;         // light's color
    float     radius;        // light's radius

};

// buffer for light instance data
unsigned int matrixBuffer;
unsigned int colorSizeBuffer;

void configurePointLights(std::vector<glm::mat4>& modelMatrices, std::vector<glm::vec4>& modelColorSizes, float radius = 1.0f, float separation = 1.0f, float yOffset = 0.0f);
void updatePointLights(std::vector<glm::mat4>& modelMatrices, std::vector<glm::vec4>& modelColorSizes, float separation, float yOffset, float radiusScale);

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "CS 562 Project 1 (Deferred Shading)", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(true);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    glswInit();
    glswSetPath("OpenGL/shaders/", ".glsl");
    glswAddDirectiveToken("", "#version 330 core");

    // Shader for writing into a depth texture
    Shader shaderDepthWrite(glswGetShader("shadowMappingDepth.Vertex"), glswGetShader("shadowMappingDepth.Fragment"));
    // Shader for visualiazing the depth texture
    Shader shaderDebugDepthMap(glswGetShader("debugQuad.Vertex"), glswGetShader("debugQuad.Fragment"));
    // G-Buffer pass shader for models w/o textures and just Kd, Ks, etc colors 
    Shader shaderGeometryPass(glswGetShader("gBuffer.Vertex"), glswGetShader("gBuffer.Fragment"));
    // G-Buffer pass shader for the models with textures (diffuse, specular, etc)
    Shader shaderTexturedGeometryPass(glswGetShader("gBufferTextured.Vertex"), glswGetShader("gBufferTextured.Fragment"));
    // First pass of deferred shader that will render the scene with a global light and shadow mapping
    Shader shaderLightingPass(glswGetShader("deferredShading.Vertex"), glswGetShader("deferredShading.Fragment"));
    // Shader for debugging the G-Buffer contents
    Shader shaderGBufferDebug(glswGetShader("gBufferDebug.Vertex"), glswGetShader("gBufferDebug.Fragment"));
    // Shader to render the light geometry for visualization and debugging
    Shader shaderGlobalLightSphere(glswGetShader("deferredLight.Vertex"), glswGetShader("deferredLight.Fragment"));
    Shader shaderLightSphere(glswGetShader("deferredLightInstanced.Vertex"), glswGetShader("deferredLightInstanced.Fragment"));
    // Shader for a final composite rendering of point(area) lights with generated G-Buffer
    Shader shaderPointLightingPass(glswGetShader("deferredPointLightInstanced.Vertex"), glswGetShader("deferredPointLightInstanced.Fragment"));

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    float planeVertices[] = {
        // positions            // normals         // texcoords
         10.0f, -0.5f,  10.0f,  0.0f, 1.0f, 0.0f,  10.0f,  10.0f,
        -10.0f, -0.5f, -10.0f,  0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
        -10.0f, -0.5f,  10.0f,  0.0f, 1.0f, 0.0f,   0.0f,  10.0f,
        
         10.0f, -0.5f,  10.0f,  0.0f, 1.0f, 0.0f,  10.0f,  10.0f,
         10.0f, -0.5f, -10.0f,  0.0f, 1.0f, 0.0f,  10.0f, 0.0f,
        -10.0f, -0.5f, -10.0f,  0.0f, 1.0f, 0.0f,   0.0f, 0.0f,   
    };
    // create floor plane VAO
    unsigned int planeVAO, planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);

    // load textures
    // -------------
    std::string woodTexturePath = PATH + "/OpenGL/images/wood.png";
    unsigned int woodTexture = loadTexture(woodTexturePath.c_str(), false);

    // load models
    // -----------
    std::string bunnyPath = PATH + "/OpenGL/models/Bunny.obj";
    std::string dragonPath = PATH + "/OpenGL/models/Dragon.obj";
    //std::string ajaxPath = PATH + "/OpenGL/models/Ajax.obj";
    std::string lucyPath = PATH + "/OpenGL/models/Lucy.obj";
    //std::string modelPath = PATH + "/OpenGL/models/Aphrodite.obj";
    Model meshModelA(lucyPath);
    //Model meshModelB(dragonPath);
   // Model meshModelC(bunnyPath);
    std::string spherePath = PATH + "/OpenGL/models/Sphere.obj";
    Model lightModel(spherePath);
    std::vector<glm::vec3> objectPositions;
    objectPositions.push_back(glm::vec3(0.0, 1.0, 0.0));
   /* objectPositions.push_back(glm::vec3(2.5, 1.0, -0.5));
    objectPositions.push_back(glm::vec3(-2.5, 1.0, -0.5));
    objectPositions.push_back(glm::vec3(0.0, 1.0, 2.0));*/
    std::vector<Model*> meshModels;
    meshModels.push_back(&meshModelA);
   // meshModels.push_back(&meshModelB);
    //meshModels.push_back(&meshModelC);

    // configure depth map framebuffer for shadow generation
    // -----------------------
    const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    // create depth texture
    unsigned int depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    // attach depth texture as FBO's depth buffer
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // configure g-buffer framebuffer
    // ------------------------------
    FrameBuffer gBuffer(SCR_WIDTH, SCR_HEIGHT);
    gBuffer.attachTexture(GL_RGB16F, GL_NEAREST); // Position color buffer
    gBuffer.attachTexture(GL_RGB16F, GL_NEAREST); // Normal color buffer
    gBuffer.attachTexture(GL_RGB, GL_NEAREST);    // Diffuse (Kd)
    gBuffer.attachTexture(GL_RGBA, GL_NEAREST);   // Specular (Ks)
    gBuffer.bindOutput();                         // calls glDrawBuffers[i] for all attached textures
    gBuffer.attachRender(GL_DEPTH_COMPONENT);     // attach Depth render buffer
    gBuffer.check();
    FrameBuffer::unbind();                        // unbind framebuffer for now

    // lighting info
    // -------------
    // instance array data for our light volumes
    std::vector<glm::mat4> modelMatrices;
    std::vector<glm::vec4> modelColorSizes;

    // single global light
    SceneLight globalLight(glm::vec3(-2.5f, 5.0f, -1.25f), glm::vec3(1.0f, 1.0f, 1.0f), 0.125f);

    // option settings
    int gBufferMode = 0;
    bool enableShadows = true;
    bool drawPointLights = false;
    bool showDepthMap = false;
    bool drawPointLightsWireframe = true;
    glm::vec3 diffuseColor = glm::vec3(0.847f, 0.52f, 0.19f);
    glm::vec4 specularColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.8f);
    float glossiness = 16.0f;
    float gLinearAttenuation = 0.09f;
    float gQuadraticAttenuation = 0.032f;
    float pointLightIntensity = 0.736f;
    float pointLightRadius = INITIAL_POINT_LIGHT_RADIUS;
    float pointLightVerticalOffset = 0.636f;
    float pointLightSeparation = 0.670f;

    const int totalLights = LIGHT_GRID_WIDTH * LIGHT_GRID_WIDTH * LIGHT_GRID_HEIGHT;
    // initialize point lights
    configurePointLights(modelMatrices, modelColorSizes, pointLightRadius, pointLightSeparation, pointLightVerticalOffset);
    
    // configure instanced array of model transform matrices
    // -------------------------
    glGenBuffers(1, &matrixBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, matrixBuffer);
    glBufferData(GL_ARRAY_BUFFER, totalLights * sizeof(glm::mat4), &modelMatrices[0], GL_STATIC_DRAW);

    // light model has only one mesh
    unsigned int VAO = lightModel.meshes[0].VAO;
    glBindVertexArray(VAO);

    // set attribute pointers for matrix (4 times vec4)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(2 * sizeof(glm::vec4)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(3 * sizeof(glm::vec4)));

    glVertexAttribDivisor(3, 1);
    glVertexAttribDivisor(4, 1);
    glVertexAttribDivisor(5, 1);
    glVertexAttribDivisor(6, 1);

    // configure instanced array of light colors
    // -------------------------
    unsigned int colorSizeBuffer;
    glGenBuffers(1, &colorSizeBuffer);
   
    glBindVertexArray(VAO);
    // set attribute pointers for light color + radius (vec4)
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, colorSizeBuffer);
    glBufferData(GL_ARRAY_BUFFER, totalLights * sizeof(glm::vec4), &modelColorSizes[0], GL_STATIC_DRAW);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
    glVertexAttribDivisor(2, 1);

    glBindVertexArray(0);
    
    // shader configuration
    // --------------------
    shaderLightingPass.use();
    shaderLightingPass.setUniformInt("gPosition", 0);
    shaderLightingPass.setUniformInt("gNormal", 1);
    shaderLightingPass.setUniformInt("gDiffuse", 2);
    shaderLightingPass.setUniformInt("gSpecular", 3);
    shaderLightingPass.setUniformInt("shadowMap", 4);

    // deferred point lighting shader
    shaderPointLightingPass.use();
    shaderPointLightingPass.setUniformInt("gPosition", 0);
    shaderPointLightingPass.setUniformInt("gNormal", 1);
    shaderPointLightingPass.setUniformInt("gDiffuse", 2);
    shaderPointLightingPass.setUniformInt("gSpecular", 3);
    shaderPointLightingPass.setUniformVec2f("screenSize", SCR_WIDTH, SCR_HEIGHT);

    // G-Buffer debug shader
    shaderGBufferDebug.use();
    shaderGBufferDebug.setUniformInt("gPosition", 0);
    shaderGBufferDebug.setUniformInt("gNormal", 1);
    shaderGBufferDebug.setUniformInt("gDiffuse", 2);
    shaderGBufferDebug.setUniformInt("gSpecular", 3);
    shaderGBufferDebug.setUniformInt("gBufferMode", 1);

    // Shadow texture debug shader
    shaderDebugDepthMap.use();
    shaderDebugDepthMap.setUniformInt("depthMap", 0);


    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glEnable(GL_DEPTH_TEST);

        // 1. render depth of scene to texture (from light's perspective)
        // --------------------------------------------------------------
        glm::mat4 lightProjection, lightView;
        glm::mat4 lightSpaceMatrix;
        glm::mat4 model = glm::mat4(1.0f);
        float zNear = 1.0f, zFar = 10.0f;

        if (enableShadows) {
            lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, zNear, zFar);
            lightView = glm::lookAt(globalLight.position, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
            lightSpaceMatrix = lightProjection * lightView;
            // render scene from light's point of view
            shaderDepthWrite.use();
            shaderDepthWrite.setUniformMat4("lightSpaceMatrix", lightSpaceMatrix);
            shaderDepthWrite.setUniformMat4("model", model);

            glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
            glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
            glClear(GL_DEPTH_BUFFER_BIT);
            // render the textured floor
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, woodTexture);
            glBindVertexArray(planeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            for (unsigned int i = 0; i < objectPositions.size(); i++)
            {
                model = glm::mat4(1.0f);
                model = glm::translate(model, objectPositions[i]);
                model = glm::scale(model, glm::vec3(1.0f));
                shaderDepthWrite.setUniformMat4("model", model);
                meshModels[i]->draw(shaderDepthWrite);
            }
            FrameBuffer::unbind();
        }
        else {
            // just clear the depth texture if shadows aren't being generated
            glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
            glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
            glClear(GL_DEPTH_BUFFER_BIT);
        }
        
        // 2. geometry pass: render scene's geometry/color data into gbuffer
        // -----------------------------------------------------------------
        // reset viewport
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        gBuffer.bindOutput();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 150.0f);
        glm::mat4 view = arcballCamera.transform();
        model = glm::mat4(1.0f);
        shaderTexturedGeometryPass.use();
        shaderTexturedGeometryPass.setUniformMat4("projection", projection);
        shaderTexturedGeometryPass.setUniformMat4("view", view);
        shaderTexturedGeometryPass.setUniformMat4("model", model);
        glm::vec4 floorSpecular = glm::vec4(0.5f, 0.5f, 0.5f, 0.8f);
        shaderTexturedGeometryPass.setUniformVec4f("specularCol", floorSpecular);
        // render the textured floor
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, woodTexture);
        glBindVertexArray(planeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // render non-textured models
        shaderGeometryPass.use();
        shaderGeometryPass.setUniformMat4("projection", projection);
        shaderGeometryPass.setUniformMat4("view", view);
        shaderGeometryPass.setUniformMat4("model", model);
        shaderGeometryPass.setUniformVec3f("diffuseCol", diffuseColor);
        glm::vec4 specular = glm::vec4(1.0f, 1.0f, 1.0f, 0.1f);
        glm::vec4 spec = glm::vec4(1.0f, 1.0f, 1.0f, 0.1f);
        shaderGeometryPass.setUniformVec4f("specularCol", specularColor);
        for (unsigned int i = 0; i < objectPositions.size(); i++)
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, objectPositions[i]);
            model = glm::scale(model, glm::vec3(1.0f));
            shaderGeometryPass.setUniformMat4("model", model);
            meshModels[i]->draw(shaderGeometryPass);
        }
        FrameBuffer::unbind();

        // 3. lighting pass: calculate lighting by iterating over a screen filled quad pixel-by-pixel using the gbuffer's content and shadow map
        // -----------------------------------------------------------------------------------------------------------------------
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (gBufferMode == 0)
        {
            shaderLightingPass.use();
            // bind all of our input textures
            gBuffer.bindInput();

            // bind depth texture
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, depthMap);

            shaderLightingPass.setUniformVec3f("gLight.Position", globalLight.position);
            shaderLightingPass.setUniformVec3f("gLight.Color", globalLight.color);
            shaderLightingPass.setUniformFloat("gLight.Linear", gLinearAttenuation);
            shaderLightingPass.setUniformFloat("gLight.Quadratic", gQuadraticAttenuation);

            glm::vec3 camPosition = arcballCamera.eye();
            shaderLightingPass.setUniformVec3f("viewPos", camPosition);
            shaderLightingPass.setUniformMat4("lightSpaceMatrix", lightSpaceMatrix);
            shaderLightingPass.setUniformFloat("glossiness", glossiness);
        }
        else // for G-Buffer debuging 
        {
            shaderGBufferDebug.use();
            shaderGBufferDebug.setUniformInt("gBufferMode", gBufferMode);
            // bind all of our input textures
            gBuffer.bindInput();
        }
        
        // finally render quad
        renderQuad();

        static bool colorSizeBufferDirty = false;

        // 3.5 lighting pass: render point lights on top of main scene with additive blending and utilizing G-Buffer for lighting.
        // -----------------------------------------------------------------------------------------------------------------------
        if (gBufferMode == 0) {
            shaderPointLightingPass.use();
            gBuffer.bindInput();
            shaderPointLightingPass.setUniformMat4("projection", projection);
            shaderPointLightingPass.setUniformMat4("view", view);

            glEnable(GL_CULL_FACE);
            // only render the back faces of the light volume spheres
            glFrontFace(GL_CW);
            glDisable(GL_DEPTH_TEST);
            // enable additive blending
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glm::vec3 camPosition = arcballCamera.eye();
            shaderPointLightingPass.setUniformVec3f("viewPos", camPosition);
            shaderPointLightingPass.setUniformFloat("lightIntensity", pointLightIntensity);
            shaderPointLightingPass.setUniformFloat("glossiness", glossiness);
            glBindVertexArray(lightModel.meshes[0].VAO);
            // don't update the color and size buffer every frame
            if (colorSizeBufferDirty) {
                glBindBuffer(GL_ARRAY_BUFFER, colorSizeBuffer);
                glBufferData(GL_ARRAY_BUFFER, LIGHT_GRID_WIDTH * LIGHT_GRID_WIDTH * LIGHT_GRID_HEIGHT * sizeof(glm::vec4), &modelColorSizes[0], GL_STATIC_DRAW);
            }
            glDrawElementsInstanced(GL_TRIANGLES, lightModel.meshes[0].indices.size(), GL_UNSIGNED_INT, 0, totalLights);
            glBindVertexArray(0);

            glDisable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glFrontFace(GL_CCW);
            glDisable(GL_CULL_FACE);
        }

        // strictly used for debugging point light volumes (sizes, positions, etc)
        if (drawPointLights && gBufferMode == 0) {
            // re-enable the depth testing 
            glEnable(GL_DEPTH_TEST);
            // copy content of geometry's depth buffer to default framebuffer's depth buffer
            // ----------------------------------------------------------------------------------
            gBuffer.bindRead();
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
            // blit to default framebuffer. 
            glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            // unbind framebuffer for now
            FrameBuffer::unbind();

            // render lights on top of scene with Z-testing
            // --------------------------------
            shaderLightSphere.use();
            shaderLightSphere.setUniformMat4("projection", projection);
            shaderLightSphere.setUniformMat4("view", view);

            glPolygonMode(GL_FRONT_AND_BACK, drawPointLightsWireframe ? GL_LINE : GL_FILL);
            glBindVertexArray(lightModel.meshes[0].VAO);
            glDrawElementsInstanced(GL_TRIANGLES, lightModel.meshes[0].indices.size(), GL_UNSIGNED_INT, 0, totalLights);
            glBindVertexArray(0);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            shaderGlobalLightSphere.use();
            shaderGlobalLightSphere.setUniformMat4("projection", projection);
            shaderGlobalLightSphere.setUniformMat4("view", view);
            // render the global light model
            model = glm::mat4(1.0f);
            model = glm::translate(model, globalLight.position);
            shaderGlobalLightSphere.setUniformMat4("model", model);
            shaderGlobalLightSphere.setUniformVec3f("lightColor", globalLight.color);
            shaderGlobalLightSphere.setUniformFloat("lightRadius", globalLight.radius);
            lightModel.draw(shaderGlobalLightSphere);
        }

        if (showDepthMap) {
            // render Depth map to quad for visual debugging
            // ---------------------------------------------
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.7f, -0.7f, 0.0f));
            model = glm::scale(model, glm::vec3(0.3f, 0.3f, 1.0f)); // Make it 30% of total screen size
            shaderDebugDepthMap.use();
            shaderDebugDepthMap.setUniformMat4("transform", model);
            shaderDebugDepthMap.setUniformFloat("zNear", zNear);
            shaderDebugDepthMap.setUniformFloat("zFar", zFar);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, depthMap);
            renderQuad();
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Controls");                          // Create a window called "Controls" and append into it.

            if (ImGui::CollapsingHeader("Model Config")) {
                ImGui::ColorEdit3("Diffuse (Kd)", (float*)&diffuseColor);   // Edit 3 floats representing Kd color (r, g, b)
                ImGui::ColorEdit4("Specular (Ks)", (float*)&specularColor); // Edit 4 floats representing Ks color (r, g, b, alpha)
                ImGui::SliderFloat("Glossiness", &glossiness, 8.0, 128.0f);
            }
            if (ImGui::CollapsingHeader("Lighting Config")) {
                if (ImGui::CollapsingHeader("Global Light")) {
                    ImGui::Text("Attenuation");
                    ImGui::SliderFloat("Linear", &gLinearAttenuation, 0.022f, 0.7f);
                    ImGui::SliderFloat("Quadratic", &gQuadraticAttenuation, 0.0019f, 1.8f);
                    ImGui::Checkbox("Enabled shadows", &enableShadows);
                }

                if (ImGui::CollapsingHeader("Point Lights")) {
                    ImGui::SliderFloat("Intensity", &pointLightIntensity, 0.0f, 3.0f, "%.3f");
                    if (ImGui::SliderFloat("Radius", &pointLightRadius, 0.3f, 2.5f, "%.3f")) {
                        updatePointLights(modelMatrices, modelColorSizes, pointLightSeparation, pointLightVerticalOffset, pointLightRadius);
                        colorSizeBufferDirty = true;
                    }
                    else {
                        colorSizeBufferDirty = false;
                    }
                    if (ImGui::SliderFloat("Separation", &pointLightSeparation, 0.4f, 1.5f, "%.3f")) {
                        updatePointLights(modelMatrices, modelColorSizes, pointLightSeparation, pointLightVerticalOffset, pointLightRadius);
                    }
                    if (ImGui::SliderFloat("Vertical Offset", &pointLightVerticalOffset, -2.0f, 3.0f)) {
                        updatePointLights(modelMatrices, modelColorSizes, pointLightSeparation, pointLightVerticalOffset, pointLightRadius);
                    }
                } 
            }
            if (ImGui::CollapsingHeader("Debug")) {
                const char* gBuffers[] = { "Final render", "Position (world)", "Normal (world)", "Diffuse", "Specular"};
                ImGui::Combo("G-Buffer View", &gBufferMode, gBuffers, IM_ARRAYSIZE(gBuffers));
                shaderLightingPass.setUniformInt("gBufferMode", gBufferMode);
                ImGui::Checkbox("Point lights volumes", &drawPointLights);
                ImGui::SameLine(); ImGui::Checkbox("Wireframe", &drawPointLightsWireframe);
                ImGui::Checkbox("Show depth texture", &showDepthMap);
            }
                                                                    
            //ImGui::ShowDemoWindow();

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::Text("Point lights in scene: %i", LIGHT_GRID_WIDTH * LIGHT_GRID_WIDTH * LIGHT_GRID_HEIGHT);
            ImGui::End();

        }

        // Rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);

    glfwTerminate();
    return 0;

}

// Node: separation < 1.0 will cause lights to penetrate each other, and > 1.0 they will separate (1.0 is just touching)
void configurePointLights(std::vector<glm::mat4>& modelMatrices, std::vector<glm::vec4>& modelColorSizes, float radius, float separation, float yOffset)
{
    srand(glfwGetTime());
    // add some uniformly spaced point lights
    for (unsigned int lightIndexX = 0; lightIndexX < LIGHT_GRID_WIDTH; lightIndexX++)
    {
        for (unsigned int lightIndexZ = 0; lightIndexZ < LIGHT_GRID_WIDTH; lightIndexZ++)
        {
            for (unsigned int lightIndexY = 0; lightIndexY < LIGHT_GRID_HEIGHT; lightIndexY++)
            {
                float diameter = 2.0f * radius;
                float xPos = (lightIndexX - (LIGHT_GRID_WIDTH - 1.0f) / 2.0f) * (diameter * separation);
                float zPos = (lightIndexZ - (LIGHT_GRID_WIDTH - 1.0f) / 2.0f) * (diameter * separation);
                float yPos = (lightIndexY - (LIGHT_GRID_HEIGHT - 1.0f) / 2.0f) * (diameter * separation) + yOffset;
                double angle = double(rand()) * 2.0 * M_PI / (double(RAND_MAX));
                double length = double(rand()) * 0.5 / (double(RAND_MAX));
                float xOffset = cos(angle) * length;
                float zOffset = sin(angle) * length;
                xPos += xOffset;
                zPos += zOffset;
                // also calculate random color
                float rColor = ((rand() % 100) / 200.0f) + 0.5; // between 0.5 and 1.0
                float gColor = ((rand() % 100) / 200.0f) + 0.5; // between 0.5 and 1.0
                float bColor = ((rand() % 100) / 200.0f) + 0.5; // between 0.5 and 1.0

                int curLight = lightIndexX * LIGHT_GRID_WIDTH * LIGHT_GRID_HEIGHT + lightIndexZ * LIGHT_GRID_HEIGHT + lightIndexY;
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(xPos, yPos, zPos));
                // now add to list of matrices
                modelMatrices.emplace_back(model);
                modelColorSizes.emplace_back(glm::vec4(rColor, gColor, bColor, radius));
            }
        }
    }
}

void updatePointLights(std::vector<glm::mat4>& modelMatrices, std::vector<glm::vec4>& modelColorSizes, float separation, float yOffset, float radius)
{
    if (separation < 0.0f) {
        return;
    }
    // add some uniformly spaced point lights
    for (unsigned int lightIndexX = 0; lightIndexX < LIGHT_GRID_WIDTH; lightIndexX++)
    {
        for (unsigned int lightIndexZ = 0; lightIndexZ < LIGHT_GRID_WIDTH; lightIndexZ++)
        {
            for (unsigned int lightIndexY = 0; lightIndexY < LIGHT_GRID_HEIGHT; lightIndexY++)
            {
                int curLight = lightIndexX * LIGHT_GRID_WIDTH * LIGHT_GRID_HEIGHT + lightIndexZ * LIGHT_GRID_HEIGHT + lightIndexY;
                float diameter = 2.0f * INITIAL_POINT_LIGHT_RADIUS;
                float xPos = (lightIndexX - (LIGHT_GRID_WIDTH - 1.0f) / 2.0f) * (diameter * separation);
                float zPos = (lightIndexZ - (LIGHT_GRID_WIDTH - 1.0f) / 2.0f) * (diameter * separation);
                float yPos = (lightIndexY - (LIGHT_GRID_HEIGHT - 1.0f) / 2.0f) * (diameter * separation);
                
                // modify matrix translation
                modelMatrices[curLight][3] = glm::vec4(xPos, yPos + yOffset, zPos, 1.0);
                modelColorSizes[curLight].w = radius;
            }
        }
    }

    // update the instance matrix buffer
    glBindBuffer(GL_ARRAY_BUFFER, matrixBuffer);
    glBufferData(GL_ARRAY_BUFFER, LIGHT_GRID_WIDTH * LIGHT_GRID_WIDTH * LIGHT_GRID_HEIGHT * sizeof(glm::mat4), &modelMatrices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}


// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    ImGuiIO& io = ImGui::GetIO();

    // only rotate the camera if we aren't over imGui
    if (leftMouseButtonPressed && !io.WantCaptureMouse) {
        //std::cout << "Xpos = " << xpos << ", Ypos = " << ypos << std::endl;
        float prevMouseX = 2.0f * lastX / SCR_WIDTH - 1;
        float prevMouseY = -1.0f * (2.0f * lastY / SCR_HEIGHT - 1);
        float curMouseX = 2.0f * xpos / SCR_WIDTH - 1;
        float curMouseY = -1.0f * (2.0f * ypos / SCR_HEIGHT - 1);
        arcballCamera.rotate(glm::vec2(prevMouseX, prevMouseY), glm::vec2(curMouseX, curMouseY));
    }

    // pan the camera when the right mouse is pressed
    if (rightMouseButtonPressed && !io.WantCaptureMouse) {
        float prevMouseX = 2.0f * lastX / SCR_WIDTH - 1;
        float prevMouseY = -1.0f * (2.0f * lastY / SCR_HEIGHT - 1);
        float curMouseX = 2.0f * xpos / SCR_WIDTH - 1;
        float curMouseY = -1.0f * (2.0f * ypos / SCR_HEIGHT - 1);
        glm::vec2 mouseDelta = glm::vec2(curMouseX - prevMouseX, curMouseY - prevMouseY);
        arcballCamera.pan(mouseDelta);
    }

    lastX = xpos;
    lastY = ypos;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        leftMouseButtonPressed = true;
    }
    else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        leftMouseButtonPressed = false;
    }

    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        rightMouseButtonPressed = true;
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
        rightMouseButtonPressed = false;
    }
      
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    float distanceSq = glm::distance2(arcballCamera.center(), arcballCamera.eye());
    if (distanceSq < MAX_CAMERA_DISTANCE && yoffset < 0)
    {
        // zoom out
        arcballCamera.zoom(yoffset);
    }
    else if (yoffset > 0)
    {
        // zoom in
        arcballCamera.zoom(yoffset);
    }
}

// utility function for loading a 2D texture from file
// ---------------------------------------------------
unsigned int loadTexture(char const * path, bool gammaCorrection)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum internalFormat;
        GLenum dataFormat;
        if (nrComponents == 1)
        {
            internalFormat = dataFormat = GL_RED;
        }
        else if (nrComponents == 3)
        {
            internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
            dataFormat = GL_RGB;
        }
        else if (nrComponents == 4)
        {
            internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
            dataFormat = GL_RGBA;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, internalFormat == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT); // use GL_CLAMP_TO_EDGE to prevent semi-transparent borders. Due to interpolation it takes texels from next repeat 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, internalFormat == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}