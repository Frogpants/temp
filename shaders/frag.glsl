#version 300 es

precision highp float;

attribute highp vec4 a_position;
attribute highp vec4 a_color;
attribute highp vec2 a_texCoord;
attribute mediump vec2 u_res;

varying highp vec4 v_color;
varying highp vec2 v_texCoord;
varying highp float v_depth;

out highp vec4 fragColor;

void fragment() {
    fragColor = v_color;
}