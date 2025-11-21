#include "displaywidget.h"
#include "emulator.h"

#include <iostream>
#include <QMatrix4x4>

// Vertex shader - transforms quad vertices and passes texture coords
static const char *vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec2 aTexCoord;
    
    out vec2 TexCoord;
    
    uniform mat4 projection;
    
    void main() {
        gl_Position = projection * vec4(aPos, 0.0, 1.0);
        TexCoord = aTexCoord;
    }
)";

// Fragment shader - samples texture
static const char *fragmentShaderSource = R"(
    #version 330 core
    in vec2 TexCoord;
    out vec4 FragColor;
    
    uniform sampler2D screenTexture;
    
    void main() {
        FragColor = texture(screenTexture, TexCoord);
    }
)";

Displaywidget::Displaywidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , emulator(nullptr)
    , shaderProgram(nullptr)
    , textureId(0)
{
    // Request OpenGL 3.3 Core Profile
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    setFormat(format);
    
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
}

Displaywidget::~Displaywidget() {
    makeCurrent();
    
    if (textureId) {
        glDeleteTextures(1, &textureId);
    }
    
    vbo.destroy();
    vao.destroy();
    
    delete shaderProgram;
    
    doneCurrent();
}

void Displaywidget::setEmulator(Emulator *emu) {
    emulator = emu;
}

void Displaywidget::initializeGL() {
    if (!initializeOpenGLFunctions()) {
        std::cerr << "[DISPLAY] Failed to initialize OpenGL 3.3 Core functions!" << std::endl;
        return;
    }
    
    std::cout << "[DISPLAY] OpenGL initialized: " << glGetString(GL_VERSION) << std::endl;
    
    // Set clear color to black
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    // Initialize components
    initShaders();
    initGeometry();
    initTexture();
}

void Displaywidget::initShaders() {
    shaderProgram = new QOpenGLShaderProgram(this);
    
    if (!shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        std::cerr << "[DISPLAY] Vertex shader compilation failed: " 
                  << shaderProgram->log().toStdString() << std::endl;
        return;
    }
    
    if (!shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
        std::cerr << "[DISPLAY] Fragment shader compilation failed: " 
                  << shaderProgram->log().toStdString() << std::endl;
        return;
    }
    
    if (!shaderProgram->link()) {
        std::cerr << "[DISPLAY] Shader program linking failed: " 
                  << shaderProgram->log().toStdString() << std::endl;
        return;
    }
    
    std::cout << "[DISPLAY] Shaders compiled and linked successfully" << std::endl;
}

void Displaywidget::initGeometry() {
    // Quad vertices: position (x, y) and texture coords (u, v)
    // Full screen quad from (0,0) to (SCREEN_WIDTH, SCREEN_HEIGHT)
    float vertices[] = {
        // Position                         // TexCoord
        0.0f,                0.0f,           0.0f, 0.0f,  // Bottom-left
        (float)SCREEN_WIDTH, 0.0f,           1.0f, 0.0f,  // Bottom-right
        (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 1.0f, 1.0f,  // Top-right
        
        0.0f,                0.0f,           0.0f, 0.0f,  // Bottom-left
        (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 1.0f, 1.0f,  // Top-right
        0.0f,                (float)SCREEN_HEIGHT, 0.0f, 1.0f   // Top-left
    };
    
    // Create VAO
    vao.create();
    vao.bind();
    
    // Create VBO
    vbo.create();
    vbo.bind();
    vbo.allocate(vertices, sizeof(vertices));
    
    // Set up vertex attributes
    // Position attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    
    // Texture coord attribute (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    vao.release();
    vbo.release();
    
    std::cout << "[DISPLAY] Geometry initialized" << std::endl;
}

void Displaywidget::initTexture() {
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Allocate texture storage (will be filled each frame)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SCREEN_WIDTH, SCREEN_HEIGHT, 
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    std::cout << "[DISPLAY] Texture created, ID=" << textureId << std::endl;
}

void Displaywidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void Displaywidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (!emulator || !shaderProgram || !textureId) {
        return;
    }
    
    // Update texture with framebuffer data
    updateTexture();
    
    // Calculate aspect-ratio-correct rendering area
    float screenAspect = static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT;
    float widgetAspect = static_cast<float>(width()) / height();
    
    float scaleX, scaleY;
    float offsetX = 0, offsetY = 0;
    
    if (widgetAspect > screenAspect) {
        // Widget is wider than screen - fit to height
        scaleY = static_cast<float>(height()) / SCREEN_HEIGHT;
        scaleX = scaleY;
        offsetX = (width() - SCREEN_WIDTH * scaleX) / 2.0f;
    } else {
        // Widget is taller than screen - fit to width
        scaleX = static_cast<float>(width()) / SCREEN_WIDTH;
        scaleY = scaleX;
        offsetY = (height() - SCREEN_HEIGHT * scaleY) / 2.0f;
    }
    
    // Create orthographic projection matrix
    QMatrix4x4 projection;
    projection.ortho(0, width(), height(), 0, -1, 1);
    
    // Apply scaling and offset via model transform
    QMatrix4x4 model;
    model.translate(offsetX, offsetY);
    model.scale(scaleX, scaleY);
    
    QMatrix4x4 mvp = projection * model;
    
    // Render
    shaderProgram->bind();
    shaderProgram->setUniformValue("projection", mvp);
    shaderProgram->setUniformValue("screenTexture", 0);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    
    vao.bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);
    vao.release();
    
    glBindTexture(GL_TEXTURE_2D, 0);
    shaderProgram->release();
}

void Displaywidget::updateTexture() {
    const uint32_t *framebuffer = emulator->getFramebuffer();
    if (!framebuffer) {
        return;
    }
    
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                    GL_RGBA, GL_UNSIGNED_BYTE, framebuffer);
    glBindTexture(GL_TEXTURE_2D, 0);
}
