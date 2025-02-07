//#include <GL/glew.h>   // or <glad/glad.h> if you prefer GLAD
//#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>

class Colour
{
private:
    double map_r[256];
    double map_g[256];
    double map_b[256];

public:
    Colour()
    {
        double stops[] = {0.0, 0.16, 0.42, 0.6425, 0.8575, 1.0};
        double reds[] = {0, 32, 237, 255, 0, 0};
        double greens[] = {7, 107, 255, 170, 2, 0};
        double blues[] = {100, 203, 255, 0, 0, 0};

        for (int i = 0; i < 256; i++)
        {
            double pos = static_cast<double>(i) / 255.0;

            int stop = std::upper_bound(stops, stops + 6, pos) - stops - 1;

            if (stop < 0)
                stop = 0;
            if (stop >= 5)
                stop = 4;

            double start = stops[stop];
            double end = stops[stop + 1];
            double range = end - start;
            double factor = (pos - start) / range;

            // Linear interpolation
            map_r[i] = (1 - factor) * reds[stop] + factor * reds[stop + 1];
            map_g[i] = (1 - factor) * greens[stop] + factor * greens[stop + 1];
            map_b[i] = (1 - factor) * blues[stop] + factor * blues[stop + 1];
        }
    }

    void get_colour(int i, double *r, double *g, double *b) const
    {
        if (i < 0) i = 0;
        if (i > 255) i = 255;

        *r = map_r[i];
        *g = map_g[i];
        *b = map_b[i];
    }
};



static double complex_centre = 0.00000000000000000278793706563379402178294753790944364927085054500163081379043930650189386849765202169477470552201325772332454726999999995;
static double real_centre =  -1.74995768370609350360221450607069970727110579726252077930242837820286008082972804887218672784431700831100544507655659531379747541999999995;
static int zoom_level = 1;
static int max_iters = 50;

// A simple vertex shader: it just takes a set of positions in clip-space (-1..1)
// and passes them through to the rasterizer.
const char* vertexShaderSource = R"(
#version 430
layout (location = 0) in vec3 aPos;

void main()
{
    // 'aPos' is already in clip-space coordinates for a fullscreen quad.
    gl_Position = vec4(aPos, 1.0);
}
)";

// A simple fragment shader: colors each pixel based on its screen coordinate.
// gl_FragCoord.xy is the pixel position in the window, from bottom-left.
const char* FLOATfragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

uniform vec2 uCentre;
uniform float uZoom;
uniform vec2 iResolution;

void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution;

    vec2 image_range = normalize(iResolution) * 4 * pow(0.9, uZoom);

    vec2 c = (uv * image_range) - (image_range/2) + uCentre;
    vec2 z = vec2(0.0, 0.0);

    float max_iters = 100 * sqrt(3. / image_range[1]);
    //float max_iters = 10000;
    int iters = 0;
    while (iters < max_iters && z[0]*z[0] < 4 && z[1]*z[1] < 4) {
        float tmp_z_real = z[0];
        z[0] = z[0] * z[0] - z[1]*z[1] + c[0];
        z[1] = 2*tmp_z_real*z[1] + c[1];
        iters++;
    }

    float colour = iters / max_iters;
    FragColor = vec4(colour, colour, colour, 1.0);

}
)";

const char* fragmentShaderSource = R"(
#version 430
out vec4 FragColor;

uniform int max_iters;
uniform dvec2 uCentre;
uniform float uZoom;
uniform dvec2 iResolution;

layout(std140) uniform ColoursBlock
{
    vec4 Colours[256];  // up to 64 floats, for example
};

void main()
{
    dvec2 uv = dvec2(gl_FragCoord.xy) / iResolution;

    dvec2 image_range = normalize(iResolution) * 4 * pow(0.9, uZoom);

    dvec2 c = (uv * image_range) - (image_range/2) + uCentre;
    dvec2 z = vec2(0.0, 0.0);

    int iters = 0;
    double max_iters = 100 * sqrt(3. / image_range[1]);
    while (iters < max_iters && z[0]*z[0] < 4 && z[1]*z[1] < 4) {
        double tmp_z_real = z[0];
        z[0] = z[0] * z[0] - z[1]*z[1] + c[0];
        z[1] = 2*tmp_z_real*z[1] + c[1];
        iters++;
    }

    int c_index = iters % 255;
    FragColor = Colours[c_index];
    //FragColor = vec4(vec2(uv), 0.0, 1.0);
}
)";

// A simple fullscreen quad (two triangles) in normalized device coordinates:
//  (-1,-1) -> bottom-left, (1,1) -> top-right.
GLfloat quadVertices[] = {
    //   X      Y     Z
    -1.0f,  1.0f, 0.0f,   // top-left
    -1.0f, -1.0f, 0.0f,   // bottom-left
     1.0f, -1.0f, 0.0f,   // bottom-right

    -1.0f,  1.0f, 0.0f,   // top-left
     1.0f, -1.0f, 0.0f,   // bottom-right
     1.0f,  1.0f, 0.0f    // top-right
};

static void checkCompileErrors(GLuint shader, std::string type)
{
    GLint success;
    GLchar infoLog[1024];
    if (type != "PROGRAM")
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "[ERROR] Shader Compilation Error (" << type << ")\n"
                      << infoLog << "\n";
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "[ERROR] Program Linking Error:\n" << infoLog << "\n";
        }
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    // Typically, yoffset is positive for scrolling "up" (zoom in) and
    // negative for scrolling "down" (zoom out).
    if (yoffset > 0)
        zoom_level += 1;   // scroll up => zoom in
    else
        zoom_level -= 1;   // scroll down => zoom out
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // If a key was pressed or is being repeated (held down)
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        float moveStep = 0.05f * std::pow(0.9, zoom_level); // move less when zoomed in

        switch (key)
        {
            case GLFW_KEY_W:
                // Move the center up
                    complex_centre += moveStep;
            break;
            case GLFW_KEY_S:
                // Move the center down
                    complex_centre -= moveStep;
            break;
            case GLFW_KEY_A:
                // Move left
                    real_centre -= moveStep;
            break;
            case GLFW_KEY_D:
                // Move right
                    real_centre += moveStep;
            break;
            case GLFW_KEY_UP:
                max_iters += 10;
            std::cout << max_iters << std::endl;
            break;
            case GLFW_KEY_DOWN:
                max_iters -= 10;
            std::cout << max_iters << std::endl;
            break;
            default:
                break;
        }
    }
}

int main()
{
    // 1. Set an error callback (optional, but recommended)
    //glfwSetErrorCallback(glfwErrorCallback);

    // 2. Initialize GLFW
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // 3. Configure GLFW to request at least OpenGL 4.0 Core profile
    //    This is crucial for double-precision support (via GPU shader fp64).
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // NOTE: On macOS, you often need:
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // 4. Create a windowed mode window and its OpenGL context
    GLFWwindow* window = glfwCreateWindow(800, 600, "OpenGL 4.0 Example", NULL, NULL);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Make the window's context current
    glfwMakeContextCurrent(window);

    // 5. Initialize GLEW (or GLAD). Must be done *after* making the context current.
    glewExperimental = GL_TRUE; // recommended for core profiles
    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(err) << std::endl;
        glfwTerminate();
        return -1;
    }

    // Print out some info about our GPU and OpenGL version
    std::cout << "OpenGL Vendor:   " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "OpenGL Version:  " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version:    " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;


    // 3. Initialize GLEW (or GLAD)
    // glewExperimental = GL_TRUE;
    // if (glewInit() != GLEW_OK)
    // {
    //     std::cerr << "Failed to initialize GLEW.\n";
    //     return -1;
    // }

    // 4. Build and compile shaders
    // ------------------------------------
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    checkCompileErrors(vertexShader, "VERTEX");

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    checkCompileErrors(fragmentShader, "FRAGMENT");

    // Link shaders into a program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkCompileErrors(shaderProgram, "PROGRAM");

    // We can now delete the individual shaders.
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // 5. Set up vertex data and buffers and configure vertex attributes
    // -----------------------------------------------------------------
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    // Copy our vertices array in a buffer for OpenGL to use
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // Vertex attribute: 3 floats per vertex, starting at offset 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    GLuint ubo;
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);

    // data to upload
    float colours[256*4] = { };

    Colour c;

    double r, g, b;
    for (int i = 0; i < 255; i++) {
        c.get_colour(i, &r, &g, &b);
        colours[i*4+0] = r;
        colours[i*4+1] = g;
        colours[i*4+2] = b;
        colours[i*4+3] = 1.0f;
    }

    // Allocate and initialize
    glBufferData(GL_UNIFORM_BUFFER, sizeof(colours), colours, GL_STATIC_DRAW);

    GLuint blockIndex = glGetUniformBlockIndex(shaderProgram, "ColoursBlock");
    glUniformBlockBinding(shaderProgram, blockIndex, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, 256*4 * sizeof(float),
             colours, GL_STATIC_DRAW);
    // 6. Main Render Loop
    // -------------------
    while (!glfwWindowShouldClose(window))
    {
        // Handle events
        glfwPollEvents();

        // Resize viewport if the user resizes the window
        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);

        // Clear the screen to a dark gray
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Use our shader
        glUseProgram(shaderProgram);

        // Pass the current window resolution to the shader
        GLint iResolutionLoc = glGetUniformLocation(shaderProgram, "iResolution");
        glUniform2d(iResolutionLoc, (double)displayW, (double)displayH);

        GLint uZoomLoc = glGetUniformLocation(shaderProgram, "uZoom");
        glUniform1f(uZoomLoc, (GLfloat)zoom_level);

        GLint uCentreLoc = glGetUniformLocation(shaderProgram, "uCentre");
        glUniform2d(uCentreLoc, real_centre, complex_centre);

        GLint umax_iters = glGetUniformLocation(shaderProgram, "max_iters");
        glUniform1i(umax_iters, max_iters);


        // GLint iResolutionLoc = glGetUniformLocation(shaderProgram, "iResolution");
        // glUniform2f(iResolutionLoc, (float)displayW, (float)displayH);
        //
        // GLint uZoomLoc = glGetUniformLocation(shaderProgram, "uZoom");
        // glUniform1f(uZoomLoc, (GLfloat)zoom_level);
        //
        // GLint uCentreLoc = glGetUniformLocation(shaderProgram, "uCentre");
        // glUniform2f(uCentreLoc, (float)real_centre, (float)complex_centre);


        // Draw our fullscreen quad
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        // Swap buffers to display on screen
        glfwSwapBuffers(window);
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}
