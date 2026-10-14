// Flashlight beam and its night-lighting integration.
//
// Included by terrain.frag, model.frag and instanced_model.frag through the
// loader's #include expansion; see Hunt/Renderer/ShaderIncludes.h.
// uFlashlight = (intensity, range, cosInner, cosOuter) -- half-angle cosines
// measured off the view axis; see Core/Flashlight.h.

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

// Night defaults; the matching engine pushes configured values per draw.
uniform vec4 uNightSettings = vec4(0.22, 0.85, 0.60, 1.0); // gain, desaturation, sky gain, normal-night treatment

void ApplyNightLighting(inout vec3 color, float distance, float cosOffAxis)
{
   if (uNightStrength <= 0.001) return;
   vec3 originalColor = color;
   float gray = dot(originalColor, vec3(0.299, 0.587, 0.114));
   color = mix(originalColor, vec3(gray), uNightSettings.y * uNightStrength);
   color *= mix(1.0, uNightSettings.x, uNightStrength);

   // Restore brightness AND color, fading with the existing beam geometry.
   // Night vision bypasses this world grade and its color restoration.
   if (uNightSettings.w > 0.5 && uFlashlight.x > 0.0) {
      float cone = smoothstep(uFlashlight.w, uFlashlight.z, cosOffAxis);
      float atten = clamp(1.0 - distance / max(uFlashlight.y, 1.0), 0.0, 1.0);
      atten *= atten;
      float restoration = clamp(uFlashlight.x * cone * atten, 0.0, 1.0);
      color = mix(color, originalColor, restoration);
   }
}
