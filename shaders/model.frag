#version 330 core
out vec4 FragColor;
in vec2 vTexCoord;
in float vLight;
in float vFog;
in vec3 vFogColor;
in float vAlpha;
in float vCutout;
in float vViewZ;
in float vRadialDist;
uniform PerFrame {
   mat4 uProjection;
   vec2 uFogRange;
   vec3 uDistanceFogColor;
   float uForceFog;
   vec3 uFogColor;
};
uniform sampler2D uModelTexture;
uniform float uTintByFogColor;
uniform float uNightStrength;    // world-only night lighting (0=day, 1=night)
uniform vec3 uCamFogColor;       // camera-in-fog envelope colour
uniform float uCamFogAmount;     // camera-in-fog envelope strength (0 = off)

// Flashlight accessory. uFlashlight = (intensity, range, cosInner, cosOuter).
// This block must stay byte-identical across terrain.frag, model.frag and
// instanced_model.frag: GLSL has no include mechanism here, and the three
// copies are the only thing keeping the beam consistent between the world,
// its water and the objects standing on it.
uniform vec4 uFlashlight;      // x = 0 when the light is off
uniform vec3 uFlashlightColor; // warm white tint

void ApplyFlashlight(inout vec3 color, float distance, float cosOffAxis)
{
   // Early out: the toggle is off for most frames, and the whole block is
   // four multiplies once active.
   if (uFlashlight.x <= 0.0) return;

   // Cone term. Because the camera sits at the origin in view space, the
   // cosine of the fragment's off-axis angle is simply depth / radial
   // distance -- no normal and no trig needed. This is what the reference
   // Reshade shader could not do: it only had a radial screen gradient, so
   // it lit everything at the centre of the screen regardless of range.
   float cone = smoothstep(uFlashlight.w, uFlashlight.z, cosOffAxis);
   if (cone <= 0.0) return;

   // Squared linear falloff, reaching zero exactly at the beam range.
   float atten = clamp(1.0 - distance / max(uFlashlight.y, 1.0), 0.0, 1.0);
   atten *= atten;

   float amount = uFlashlight.x * cone * atten;
   if (amount <= 0.0) return;

   // Headroom roll-off, cubed. A plain multiplicative boost pins every lit
   // channel at 1.0 and the surface's own shading collapses into flat paint --
   // the washed-out look. Scaling by how much headroom the pixel has left
   // keeps texture detail intact: near-black night ground still takes most of
   // the beam, near-white highlights take almost none. See Core/Flashlight.h.
   float luma = dot(color, vec3(0.299, 0.587, 0.114));
   float headroom = 1.0 - clamp(luma, 0.0, 1.0);
   headroom *= headroom * headroom;
   amount *= headroom;
   if (amount <= 0.0) return;

   vec3 beam = uFlashlightColor * amount;
   // Multiplicative boost tinted by the beam, so texture detail is preserved
   // rather than replaced...
   color *= (1.0 + beam);
   // ...plus the reference shader's "blend fix" floor: a pixel at exactly
   // black does not survive a multiply, so without this the beam vanishes on
   // unlit night ground. Kept small on purpose; a larger value is what makes
   // shadowed detail read as a wash.
   color = max(color, beam * 0.03);
}

void main() {
   vec4 texColor = texture(uModelTexture, vTexCoord);
   if (vCutout > 0.5 && texColor.a <= 0.5) discard;
   vec3 litColor = texColor.rgb * vLight;
   // Phase 2.7: branch-less tint via mix (was if > 0.5).
   vec3 tinted = litColor * vFogColor;
   litColor = mix(litColor, tinted, uTintByFogColor);
   vec3 finalColor = mix(litColor, vFogColor, vFog);
   // Match the terrain/instanced-model horizon fade for legacy
   // model-path objects (BMP billboards and water-clipped meshes).
   // Radial distance (not forward-Z) so wide-FOV screen edges fog in
   // step with the radial CPU alpha fade -- see terrain.frag.
   float distanceFog = clamp((vRadialDist - uFogRange.x) / max(uFogRange.y - uFogRange.x, 1.0), 0.0, 1.0);
   finalColor = mix(finalColor, uDistanceFogColor, distanceFog);

   // camera-in-fog global envelope -- see terrain.frag.  Fogs the whole
   // scene by distance when the camera is submerged in a tall pocket-fog volume.
   if (uCamFogAmount > 0.001f) {
       // Near baseline so close objects are also hazed (sells "inside fog");
       // far objects still reach the full envelope amount.
       const float kNearFog = 0.25f;
       float camEnvDist = kNearFog + (1.0f - kNearFog) * (1.0f - exp(-2.5f * vViewZ / max(uFogRange.y, 1.0f)));
       float camFog = uCamFogAmount * camEnvDist;
       // Weapon phong/env-map overlays are additive passes drawn over a body
       // that already carries the envelope's fog-colour blend.  Adding the
       // fog colour a second time tinted the highlights twice as strongly as
       // the rest of the viewmodel (visible colour seam between specular and
       // diffuse weapon sections).  They attenuate toward black instead,
       // leaving the fog colour to the base pass -- mirroring why GLSky.cpp
       // zeroes the envelope for the sun, which is likewise added over an
       // already-fogged layer.  Base draws (uTintByFogColor=0) keep the
       // regular fog-colour blend.
       vec3 envelopeColor = mix(uCamFogColor, vec3(0.0f), uTintByFogColor);
       finalColor = finalColor * (1.0f - camFog) + envelopeColor * camFog;
   }

   // Apply night lighting to world models only. Sky and moon use their own
   // night textures/shading and never pass through this multiplier.
   if (uNightStrength > 0.001f) {
       float gray = dot(finalColor, vec3(0.299, 0.587, 0.114));
       finalColor = mix(finalColor, vec3(gray), 0.6 * uNightStrength);
       finalColor *= mix(1.0, 0.5, uNightStrength);
   }
   // Flashlight: after the night darkening above, so the beam restores what
   // night took away instead of being darkened with everything else. The sky
   // and HUD use separate programs and never reach this path.
   ApplyFlashlight(finalColor, vRadialDist,
                   vViewZ / max(vRadialDist, 1.0));

   FragColor = vec4(finalColor, texColor.a * vAlpha);
}
