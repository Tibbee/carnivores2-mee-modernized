#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec3 uNightPresentation = vec3(1.0, 0.0, 0.0);

// BEGIN NIGHT PRESENTATION -- shared by model.frag and ui.frag.
void ApplyNightPresentation(inout vec3 color, float brightness)
{
   if (uNightPresentation.z < 0.5) return;
   float gray = dot(color, vec3(0.299, 0.587, 0.114));
   color = mix(color, vec3(gray), uNightPresentation.y) * brightness;
}
// END NIGHT PRESENTATION

uniform int uUIReadableCount = 0;
uniform vec4 uUIReadableRects[16]; // normalized top-down bounds

void main() {
   vec3 rgb555 = texture(uTexture, vTexCoord).rgb;
   // Test the original texel: dim opaque borders must not become transparent.
   if (dot(rgb555, vec3(1.0)) < 0.01) discard;
   float brightness = uNightPresentation.x;
   if (uNightPresentation.z > 0.5 && brightness < 0.55) {
      for (int i = 0; i < min(uUIReadableCount, 16); ++i) {
         vec4 rect = uUIReadableRects[i];
         if (all(greaterThanEqual(vTexCoord, rect.xy)) &&
             all(lessThan(vTexCoord, rect.zw))) {
            brightness = max(brightness, 0.55);
            break;
         }
      }
   }
   ApplyNightPresentation(rgb555, brightness);
   FragColor = vec4(rgb555, 1.0);
}
