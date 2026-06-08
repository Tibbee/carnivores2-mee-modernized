// ==========================================================================
// GLShader.cpp — OpenGL shader management for Carnivores 2 ME
//
// Minimal stub for compilation. Will be implemented when GL renderer
// is developed further.
// ==========================================================================

#include "Hunt.h"
#include "GLShader.h"

#ifdef _gl

#include <cstdio>

bool GLShader::LoadFromFile(const char* vertPath, const char* fragPath)
{
    // TODO: Read shader files and compile
    PrintLog("GLShader: LoadFromFile() — stub\n");
    (void)vertPath; (void)fragPath;
    return true;
}

bool GLShader::LoadFromSource(const char* vertSrc, const char* fragSrc)
{
    // TODO: Compile shaders from source
    PrintLog("GLShader: LoadFromSource() — stub\n");
    (void)vertSrc; (void)fragSrc;
    return true;
}

void GLShader::Use() const
{
    // TODO: glUseProgram(m_Program)
}

void GLShader::Release()
{
    // TODO: glDeleteProgram(m_Program)
    m_Program = 0;
}

#endif // _gl
