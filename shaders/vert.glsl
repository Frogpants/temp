#version 300 es

precision highp float;

uniform vec3 cam;
uniform vec3 rot;

uniform float FOV;

in highp vec4 a_position;
in highp vec4 a_color;
in highp vec2 a_texCoord;
in mediump vec2 u_res;

out highp vec4 v_color;
out highp vec2 v_texCoord;
out highp float v_depth;

float f = 1280.0;

vec2 rotate(vec2 p, float angle) {
    float x = p.x * cos(angle) - p.y * sin(angle);
    float y = p.x * sin(angle) + p.y * cos(angle);
    return vec2(x, y);
}

vec2 project(vec3 vert) {
    vec3 p = vert - cam;

    vec2 r = rotate(vec2(p.x,p.z), rot.x);
    p = vec3(r.x, p.y, r.z);
    vec2 r = rotate(vec2(p.y,p.z), rot.y);
    p = vec3(p.x, r.y, r.z);
    
    p.z = max(p.z, 0.1);
    return f * (p.xy / p.z);
}

void vertex() {
    f = u_res.x / tan(FOV / 2.0);

    vec2 projected = project(a_position.xyz);
    gl_Position = vec4(projected, 0.0, 1.0);
    v_color = a_color;
    v_texCoord = a_texCoord;
    v_depth = a_position.z;
}