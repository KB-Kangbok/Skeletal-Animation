#ifndef SKELETON_H
#define SKELETON_H

#include "rigtform.h"

#include <map>

class Skeleton;

class Bone
{
private:
	RigTForm transform_;
	Matrix4 offset_;
	Bone* parent_;
public:
	Bone(Bone* parent,const RigTForm& transform);

	void rotate(const Quat& rotation);
	void setRotate(const Quat& rotation);

	Quat getRotation() const;
	Matrix4 getModelMatrix() const;
	Matrix4 getBoneMatrix() const;
};

class Skeleton
{
private:
	std::map<int,Bone*> bones_;
public:
	Skeleton();
	~Skeleton();

	Bone* addBone(int name,Bone* parent,const RigTForm& transform);
	Bone* getNamedBone(int name);
};

#endif