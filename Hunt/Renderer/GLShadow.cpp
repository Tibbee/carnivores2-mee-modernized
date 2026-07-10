// ==========================================================================
// GLShadow.cpp � Projected shadow rendering
// ==========================================================================

#include "Hunt.h"
#include "GLRenderer.h"
#include "Renderer/GLUtils.h"

#ifdef _gl

#include "glad/glad.h"
#include <cmath>

void GLRenderer::BuildCharacterShadowVertices(const TCharacter& character, float alpha,
                                              std::vector<ModelVertex>& outVerts)
{
    if (!character.pinfo || !character.pinfo->mptr || alpha <= 0.0f) {
        return;
    }

    TModel* mptr = character.pinfo->mptr.get();
    if (!mptr->gVertex || !mptr->gFace || mptr->VCount <= 0 || mptr->FCount <= 0) {
        return;
    }

    const float cal = pi / 2.0f - character.alpha;
    const float cla = std::cos(cal);
    const float sla = std::sin(cal);
    const float caCam = std::cos(CameraAlpha);
    const float saCam = std::sin(CameraAlpha);
    const float cbCam = std::cos(CameraBeta);
    const float sbCam = std::sin(CameraBeta);

    std::vector<Vector3d> projected(mptr->VCount);
    bool anyVisible = false;

    for (int s = 0; s < mptr->VCount; ++s) {
        const TPoint3d& source = mptr->gVertex[s];
        const float mrx = source.x * cla + source.z * sla;
        const float mrz = source.z * cla - source.x * sla;

        const float shx = mrx + source.y * SunShadowK;
        const float shz = mrz + source.y * SunShadowK;
        const float shy = GetLandH(shx + character.pos.x, shz + character.pos.z) - character.pos.y;

        Vector3d out;
        out.x = (shx * caCam + shz * saCam) + character.rpos.x;
        const float vz = shz * caCam - shx * saCam;
        out.y = (shy * cbCam - vz * sbCam) + character.rpos.y;
        out.z = (vz * cbCam + shy * sbCam) + character.rpos.z + 8.0f;
        projected[s] = out;

        if (out.z < kModelNearClip) {
            anyVisible = true;
        }
    }

    if (!anyVisible) {
        return;
    }

    outVerts.reserve(static_cast<size_t>(mptr->FCount) * 3);

    for (int f = 0; f < mptr->FCount; ++f) {
        const TFace& face = mptr->gFace[f];
        const Vector3d& p0 = projected[face.v1];
        const Vector3d& p1 = projected[face.v2];
        const Vector3d& p2 = projected[face.v3];

        if (ShouldCullModelFace(face.Flags, p0, p1, p2)) {
            continue;
        }

        // Phase 1.4: pack shadow vertices with zero light, zero fog,
        // per-character alpha, no cutout, zero fog color.
        const uint8_t lightByte  = 0;
        const uint8_t fogByte    = 0;
        const uint8_t alphaByte  = Float01ToByte(alpha);
        const uint8_t cutoutByte = 0;
        outVerts.push_back({p0.x, p0.y, p0.z, 0.0f, 0.0f, lightByte, fogByte, alphaByte, cutoutByte, 0, 0, 0, {0,0,0,0,0}});
        outVerts.push_back({p1.x, p1.y, p1.z, 0.0f, 0.0f, lightByte, fogByte, alphaByte, cutoutByte, 0, 0, 0, {0,0,0,0,0}});
        outVerts.push_back({p2.x, p2.y, p2.z, 0.0f, 0.0f, lightByte, fogByte, alphaByte, cutoutByte, 0, 0, 0, {0,0,0,0,0}});
    }
}

void GLRenderer::RenderProjectedCharacterShadow(const TCharacter& character, float alpha)
{
    std::vector<ModelVertex> shadowVertices;
    BuildCharacterShadowVertices(character, alpha, shadowVertices);
    if (shadowVertices.empty()) {
        return;
    }

    DrawModelVertices(m_whiteTexture, shadowVertices, BuildLegacyProjection(), true, true, false);
}

void GLRenderer::RenderProjectedShadows()
{
#ifdef GL_PERF_HOOKS
    GL_PERF_SCOPE("RenderProjectedShadows");
#endif
    if (!SHADOWS3D || IsUnderwater()) {
        return;
    }

    std::vector<std::pair<float, const TCharacter*>> sortedCharacters;
    sortedCharacters.reserve(ChCount);

    for (int i = 0; i < ChCount; ++i) {
        const TCharacter& character = Characters[i];
        if (!character.pinfo) {
            continue;
        }

    const float distanceSq = VectorLengthSq(character.rpos);
    const float visibilityRadius = static_cast<float>(ctViewR * 256);
    const float shadowCullRadius = static_cast<float>(256 * (ctViewR - 8));
    const float visibilityRadiusSq = visibilityRadius * visibilityRadius;
    const float shadowCullRadiusSq = shadowCullRadius * shadowCullRadius;

        float r = static_cast<float>((std::max)(fabs(character.rpos.x), fabs(character.rpos.z)));
        int ri = -1 + static_cast<int>(r / 256.0f + 0.5f);
        if (ri < 0) ri = 0;
        if (ri > ctViewR) continue;

        float br = BackViewR + DinoInfo[character.CType].Radius;
        if (character.rpos.z > br) continue;
        if (fabs(character.rpos.x) > -character.rpos.z + br) continue;
        if (fabs(character.rpos.y) > -character.rpos.z + br) continue;
        if (distanceSq > visibilityRadiusSq || distanceSq > shadowCullRadiusSq) continue;

        float alpha = 0x60 / 255.0f;
        if (character.Health == 0) {
            if (Tranq || character.CType == 11) {
                continue;
            }

            const int aniTime = character.pinfo->Animation[character.Phase].AniTime;
            if (aniTime <= 0 || character.FTime >= aniTime - 1) {
                continue;
            }

            alpha *= static_cast<float>(aniTime - character.FTime) / static_cast<float>(aniTime);
        }

        if (alpha <= 0.0f) {
            continue;
        }

        sortedCharacters.emplace_back(distanceSq, &character);
    }

    std::sort(sortedCharacters.begin(), sortedCharacters.end(),
              [](const auto& a, const auto& b) {
                  return a.first > b.first;
              });

    // Step 6: Shadow fade with distance
    const float shadowFadeStart = static_cast<float>((ctViewR - 8) << 8);
    const float shadowFadeEnd = 256.0f * static_cast<float>(ctViewR - 4);

    if (GpuFeatureEnabled(GPUF_SHADOWS_INSTANCING)) {
        std::vector<ModelVertex> shadowBatch;
        shadowBatch.reserve(sortedCharacters.size() * static_cast<size_t>(384));
        for (const auto& entry : sortedCharacters) {
            const TCharacter& character = *entry.second;
            float alpha = 0x60 / 255.0f;
            if (character.Health == 0) {
                const int aniTime = character.pinfo->Animation[character.Phase].AniTime;
                if (aniTime > 0) {
                    alpha *= static_cast<float>(aniTime - character.FTime) / static_cast<float>(aniTime);
                }
            }

            // Apply distance-based fade to shadow alpha
            const float distanceSq = VectorLengthSq(character.rpos);
            if (distanceSq > shadowFadeStart * shadowFadeStart) {
                float shadowFade = CalcTerrainAlpha(distanceSq, shadowFadeStart,
                                                    shadowFadeStart * shadowFadeStart,
                                                    shadowFadeEnd, IsUnderwater());
                alpha *= shadowFade;
            }

            if (alpha <= 0.005f) continue;

            BuildCharacterShadowVertices(character, alpha, shadowBatch);
        }
        if (!shadowBatch.empty()) {
            DrawModelVertices(m_whiteTexture, shadowBatch, BuildLegacyProjection(), true, true, false);
        }
    } else {
        for (const auto& entry : sortedCharacters) {
            const TCharacter& character = *entry.second;
            float alpha = 0x60 / 255.0f;
            if (character.Health == 0) {
                const int aniTime = character.pinfo->Animation[character.Phase].AniTime;
                if (aniTime > 0) {
                    alpha *= static_cast<float>(aniTime - character.FTime) / static_cast<float>(aniTime);
                }
            }

            // Apply distance-based fade to shadow alpha
            const float distanceSq = VectorLengthSq(character.rpos);
            if (distanceSq > shadowFadeStart * shadowFadeStart) {
                float shadowFade = CalcTerrainAlpha(distanceSq, shadowFadeStart,
                                                    shadowFadeStart * shadowFadeStart,
                                                    shadowFadeEnd, IsUnderwater());
                alpha *= shadowFade;
            }

            if (alpha <= 0.005f) continue;

            RenderProjectedCharacterShadow(character, alpha);
        }
    }
}

#endif // _gl
