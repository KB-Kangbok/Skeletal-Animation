#version 130

uniform vec3 uLight, uLight2, uColor;
uniform int uUseTexture;
uniform sampler2D uTexUnit;

in vec3 vNormal;
in vec3 vPosition;

out vec4 fragColor;

void main() {
	vec3 tolight = normalize(uLight - vPosition);
	vec3 tolight2 = normalize(uLight2 - vPosition);
	vec3 normal = normalize(vNormal);
	vec3 halfVector = normalize(tolight - vPosition);
	vec3 halfVector2 = normalize(tolight2 - vPosition);

	float diffuse = max(0.0, dot(normal, tolight));
	diffuse += max(0.0, dot(normal, tolight2));
	vec3 intensity = uColor * diffuse;
    fragColor = vec4(intensity, 1.0);

    float specular = pow(max(0.0,dot(normal,halfVector)),12);
    specular += pow(max(0.0,dot(normal,halfVector2)),12);
    fragColor += 0.3*vec4(1.0)*specular;
}