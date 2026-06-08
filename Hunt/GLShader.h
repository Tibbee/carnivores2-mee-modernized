// ==========================================================================
// GLShader.h — OpenGL shader management for Carnivores 2 ME
//
// Minimal stub for compilation. Will be implemented when GL renderer
// is developed further.
// ==========================================================================

#ifndef GLSHADER_H
#define GLSHADER_H

#pragma once

class GLShader {
public:
    GLShader() = default;
    ~GLShader() = default;

    // Non-copyable
    GLShader(const GLShader&) = delete;
    GLShader& operator=(const GLShader&) = delete;

    bool LoadFromFile(const char* vertPath, const char* fragPath);
    bool LoadFromSource(const char* vertSrc, const char* fragSrc);
    void Use() const;
    void Release();

    unsigned int GetProgramID() const { return m_Program; }

private:
    unsigned int m_Program = 0;
};

#endif // GLSHADER_H
