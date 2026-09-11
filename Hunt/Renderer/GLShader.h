// ==========================================================================
// GLShader.h -- OpenGL shader management for Carnivores 2 ME
//
// Encapsulates vertex+fragment shader compilation, linking, binding, and
// uniform setting with location caching. Supports loading from external
// .vert/.frag files or from in-memory source strings.
// ==========================================================================

#ifndef GLSHADER_H
#define GLSHADER_H

#pragma once

#include <string>
#include <map>

#ifdef _gl
#include <glad/glad.h>
#endif

class GLShader {
public:
    GLShader();
    ~GLShader();

    // Non-copyable
    GLShader(const GLShader&) = delete;
    GLShader& operator=(const GLShader&) = delete;

    // Move support
    GLShader(GLShader&& other) noexcept;
    GLShader& operator=(GLShader&& other) noexcept;

    // ---- Loading ---------------------------------------------------------

    // Load vertex and fragment shader from external text files.
    // Returns true on success, false on failure (logs errors).
    bool LoadFromFile(const char* vertPath, const char* fragPath);

    // Compile vertex and fragment shader from in-memory source strings.
    // Returns true on success, false on failure (logs errors).
    bool LoadFromSource(const char* vertSrc, const char* fragSrc);

    // ---- Binding ---------------------------------------------------------

    // Bind this shader program for rendering.
    void Use() const;

    // Release (delete) the shader program. Idempotent.
    void Release();

    // ---- Uniform setters (with automatic location caching) ---------------

    void SetUniformVec2(const std::string& name, float x, float y);
    void SetUniformVec3(const std::string& name, float x, float y, float z);
    void SetUniformFloat(const std::string& name, float value);
    void SetUniformInt(const std::string& name, int value);
    void SetUniformMat4(const std::string& name, const float* matrix);

    // ---- Accessors -------------------------------------------------------

    GLuint GetProgramID() const { return m_Program; }
    bool IsValid() const { return m_Program != 0; }

private:
    GLuint m_Program;

    // Compile a single shader stage. Returns 0 on failure.
    GLuint CompileShader(GLenum type, const char* source);

    // Link a vertex+fragment shader pair into a program. Returns 0 on failure.
    GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader);

    // Check compilation/link errors. Returns true if OK.
    bool CheckCompileErrors(GLuint shader, const char* typeStr);
    bool CheckLinkErrors(GLuint program);

    // Get (or cache) a uniform location. Returns -1 if not found.
    GLint GetUniformLocation(const std::string& name);

    // Internal cache of uniform locations, populated on first access.
    std::map<std::string, GLint> m_uniformLocations;
};

#endif // GLSHADER_H
