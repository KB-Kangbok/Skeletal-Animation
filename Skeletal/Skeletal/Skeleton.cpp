#include "Skeleton.h"
#include "cvec.h"
#include "matrix4.h"

Bone::Bone(Bone* parent,const RigTForm& transform)
	: parent_(parent), transform_(transform)
{
	offset_ = inv(getModelMatrix());
}

void Bone::rotate(const Quat& rotation)
{
	transform_.setRotation(transform_.getRotation()*rotation);
}

void Bone::setRotate(const Quat& rotation) {
	transform_.setRotation(rotation);
}

Quat Bone::getRotation() const{
	return transform_.getRotation();
}

Matrix4 Bone::getModelMatrix() const
{
	if(parent_ != NULL)
		return parent_->getModelMatrix()* rigTFormToMatrix(transform_);
	return  rigTFormToMatrix(transform_);
}

Matrix4 Bone::getBoneMatrix() const
{
	return this->getModelMatrix()*offset_;
}

Skeleton::Skeleton()
{
}

Skeleton::~Skeleton()
{
	std::map<int, Bone*>::iterator iter;
    for (iter = bones_.begin(); iter != bones_.end(); iter++)
		delete iter->second;
	bones_.clear();
}

Bone* Skeleton::addBone(int name,Bone* parent,const RigTForm& transform)
{
	Bone* newBone = new Bone(parent,transform);
	bones_[name] = newBone;
	return newBone;
}

Bone* Skeleton::getNamedBone(int index)
{
	return bones_[index];
}