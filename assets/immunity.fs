#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

// Time
uniform float time;

void basicImmunity() {
    vec4 texelColor = texture(texture0, fragTexCoord);

    ivec2 textureSz = textureSize(texture0, 0);

    float s = abs(sin(time * 40));

    if (s > 0.5) {
        finalColor = texelColor.aaaa;
    } else {
        finalColor = texelColor;
    }
}

void advancedImmunity() {
    vec4 texelColor = texture(texture0, fragTexCoord);

    ivec2 textureSz = textureSize(texture0, 0);

    float xsin = (sin(time * 20) * textureSz.x + textureSz.x) / 2;
    float ysin = (sin(time * 20) * textureSz.y + textureSz.y) / 2;

    float width = 0;

    bool falls_on_x = xsin + width > fragTexCoord.x && fragTexCoord.x < xsin - width;
    bool falls_on_y = ysin + width > fragTexCoord.y && fragTexCoord.y < ysin - width;

    if (falls_on_x && falls_on_y) {
        finalColor = texelColor.aaaa;
    } else {
        finalColor = texelColor;
    }
}

void main() {
    basicImmunity();
}
