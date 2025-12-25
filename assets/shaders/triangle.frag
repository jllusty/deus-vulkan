#version 450

layout(location = 0) out vec4 color;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(800.0, 600.0); // replace with your size
    color = vec4(0., 0., 1., 1.);
}
