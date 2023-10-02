#version 420 core
layout(early_fragment_tests) in;



in vec3 Color;
out vec4 FragColor;

void main() {


	FragColor = vec4(Color * 0.4, 1);	
}  