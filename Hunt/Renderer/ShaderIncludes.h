#pragma once

// ---------------------------------------------------------------------------
// Minimal #include expansion for GLSL sources.
//
// GLSL 3.30 has no include mechanism, and the world shaders share one
// flashlight/night-lighting block. GLShader::LoadFromFile expands lines of the
// form
//
//     #include "flashlight.glsl"
//
// relative to the including file's directory before handing the source to
// OpenGL. Nesting is allowed up to kMaxDepth, which also breaks cycles. A
// missing or malformed include fails the load with a message naming the
// problem instead of feeding a partial source to the driver. Compile errors
// after expansion report post-expansion line numbers.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>

namespace shaderinclude {

constexpr int kMaxDepth = 8;

struct Result {
    bool ok = false;
    std::string text;
    std::string error; // reason when ok is false
};

// Whole file as text; false when it cannot be read. Trailing nulls and CRs are
// stripped the way the loader used to do it per shader file.
inline bool ReadFile(const std::string& path, std::string& text)
{
    std::FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;

    std::fseek(fp, 0, SEEK_END);
    const long size = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (size <= 0) {
        std::fclose(fp);
        return false;
    }

    text.resize(static_cast<size_t>(size));
    const size_t read = std::fread(&text[0], 1, static_cast<size_t>(size), fp);
    std::fclose(fp);
    if (read != static_cast<size_t>(size)) return false;

    while (!text.empty() && (text.back() == '\0' || text.back() == '\r')) text.pop_back();
    return true;
}

// Directory part of a path, including the trailing separator ("" when none).
inline std::string DirectoryOf(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

enum class LineKind { Text, Include, Malformed };

// Recognizes an include directive; only leading whitespace may precede it and
// only whitespace/CR may follow the quoted name.
inline LineKind ClassifyLine(const std::string& line, std::string& name)
{
    size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
    const char directive[] = "#include";
    for (size_t d = 0; directive[d] != '\0'; ++d, ++i) {
        if (i >= line.size() || line[i] != directive[d]) return LineKind::Text;
    }
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
    if (i >= line.size() || line[i] != '"') return LineKind::Malformed;
    const size_t end = line.find('"', i + 1);
    if (end == std::string::npos) return LineKind::Malformed;
    name = line.substr(i + 1, end - i - 1);
    for (size_t t = end + 1; t < line.size(); ++t) {
        if (line[t] != ' ' && line[t] != '\t' && line[t] != '\r' && line[t] != '\n') return LineKind::Malformed;
    }
    return name.empty() ? LineKind::Malformed : LineKind::Include;
}

// Expands includes in `source`, resolving names against `directory`.
// Forward declaration: ExpandText recurses through ExpandFile.
inline Result ExpandFile(const std::string& path, int depth = 0);

inline Result ExpandText(const std::string& directory, const std::string& source, int depth)
{
    Result expanded;
    expanded.ok = true;
    size_t pos = 0;
    while (pos <= source.size()) {
        const size_t newline = source.find('\n', pos);
        const std::string line = newline == std::string::npos
            ? source.substr(pos) : source.substr(pos, newline - pos);
        const std::string terminator = newline == std::string::npos ? "" : "\n";

        std::string name;
        switch (ClassifyLine(line, name)) {
        case LineKind::Malformed:
            expanded.ok = false;
            expanded.text.clear();
            expanded.error = "malformed #include line: '" + line + "'";
            return expanded;
        case LineKind::Include: {
            if (depth >= kMaxDepth) {
                expanded.ok = false;
                expanded.text.clear();
                expanded.error = "include depth exceeded (cycle?) while expanding '" + name + "'";
                return expanded;
            }
            const std::string path = directory + name;
            Result included = ExpandFile(path, depth + 1);
            if (!included.ok) {
                expanded.ok = false;
                expanded.text.clear();
                expanded.error = "cannot include '" + name + "': " + included.error;
                return expanded;
            }
            expanded.text += included.text;
            if (!included.text.empty() && included.text.back() != '\n') expanded.text += '\n';
            break;
        }
        case LineKind::Text:
            expanded.text += line;
            expanded.text += terminator;
            break;
        }
        if (newline == std::string::npos) break;
        pos = newline + 1;
    }
    return expanded;
}

// Reads one shader file and expands its includes.
inline Result ExpandFile(const std::string& path, int depth)
{
    Result expanded;
    std::string source;
    if (!ReadFile(path, source)) {
        expanded.error = "failed to read '" + path + "'";
        return expanded;
    }
    return ExpandText(DirectoryOf(path), source, depth);
}

} // namespace shaderinclude
