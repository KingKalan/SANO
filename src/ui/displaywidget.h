#ifndef DISPLAYWIDGET_H
#define DISPLAYWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>

// Forward declaration
class Emulator;

/**
 * Displaywidget - Custom QOpenGLWidget for rendering emulator output
 * 
 * Displays the 320x240 framebuffer from the emulator using OpenGL 3.3 Core.
 * Handles scaling and maintains aspect ratio.
 */
class Displaywidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT

public:
    explicit Displaywidget(QWidget *parent = nullptr);
    ~Displaywidget();
    
    // Set the emulator instance to render from
    void setEmulator(Emulator *emu);

protected:
    // QOpenGLWidget overrides
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    Emulator *emulator;
    
    // Screen dimensions
    static constexpr int SCREEN_WIDTH = 320;
    static constexpr int SCREEN_HEIGHT = 240;
    
    // Modern OpenGL objects
    QOpenGLShaderProgram *shaderProgram;
    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer vbo;
    GLuint textureId;
    
    // Helper methods
    void initShaders();
    void initGeometry();
    void initTexture();
    void updateTexture();
};

#endif // DISPLAYWIDGET_H
