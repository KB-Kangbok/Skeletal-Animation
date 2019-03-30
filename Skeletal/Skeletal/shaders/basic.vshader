#version 130

uniform mat4 uProjMatrix;
uniform mat4 uModelViewMatrix;
uniform mat4 uNormalMatrix;
uniform mat4 uBone[32];
uniform mat4 uBoneNormal[32];
uniform int uUseBones;

in vec3 aPosition;
in vec3 aNormal;
in ivec3 aBoneNames;
in vec3 aBoneWeights;

out vec3 vNormal;
out vec3 vPosition;

void main() {
  if(uUseBones == 1)
    vNormal = vec3((aBoneWeights.x*uBoneNormal[aBoneNames.x]+
                    aBoneWeights.y*uBoneNormal[aBoneNames.y]+
                    aBoneWeights.z*uBoneNormal[aBoneNames.z]) * vec4(aNormal, 0.0));
  else
    vNormal = vec3(uNormalMatrix * vec4(aNormal, 0.0));

  // send position (eye coordinates) to fragment shader
  vec4 tPosition;
  if(uUseBones == 1)
    tPosition = (aBoneWeights.x*uBone[aBoneNames.x]+
                 aBoneWeights.y*uBone[aBoneNames.y]+
                 aBoneWeights.z*uBone[aBoneNames.z]) * vec4(aPosition, 1.0);
  else
    tPosition = uModelViewMatrix * vec4(aPosition, 1.0);

  vPosition = vec3(tPosition);
  gl_Position = uProjMatrix * tPosition;
}
