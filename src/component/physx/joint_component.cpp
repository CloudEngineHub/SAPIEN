#include "sapien/component/physx/joint_component.h"
#include "sapien/component/physx/articulation_link_component.h"
#include "sapien/component/physx/physx_system.h"
#include "sapien/entity.h"
#include "sapien/math/conversion.h"
#include "sapien/scene.h"

using namespace physx;

namespace sapien {
namespace component {

PhysxJointComponent::PhysxJointComponent(std::shared_ptr<PhysxRigidBodyComponent> body)
    : mChild(body) {}

PhysxJointComponent::~PhysxJointComponent() {}

void PhysxJointComponent::setParent(std::shared_ptr<PhysxRigidBaseComponent> body) {
  if (mParent) {
    mParent->internalUnregisterJoint(
        std::static_pointer_cast<PhysxJointComponent>(shared_from_this()));
  }
  mParent = body;
  internalRefresh();
  if (mParent) {
    mParent->internalRegisterJoint(
        std::static_pointer_cast<PhysxJointComponent>(shared_from_this()));
  }
}

std::shared_ptr<PhysxRigidBaseComponent> PhysxJointComponent::getParent() const { return mParent; }

void PhysxJointComponent::internalRefresh() {
  getPxJoint()->setActors(mParent ? mParent->getPxActor() : nullptr, mChild->getPxActor());
}

void PhysxJointComponent::setParentAnchorPose(Pose const &pose) {
  getPxJoint()->setLocalPose(PxJointActorIndex::eACTOR0, PoseToPxTransform(pose));
}

Pose PhysxJointComponent::getParentAnchorPose() const {
  return PxTransformToPose(getPxJoint()->getLocalPose(PxJointActorIndex::eACTOR0));
}

void PhysxJointComponent::setChildAnchorPose(Pose const &pose) {
  getPxJoint()->setLocalPose(PxJointActorIndex::eACTOR1, PoseToPxTransform(pose));
}

Pose PhysxJointComponent::getChildAnchorPose() const {
  return PxTransformToPose(getPxJoint()->getLocalPose(PxJointActorIndex::eACTOR1));
}

Pose PhysxJointComponent::getRelativePose() const {
  return PxTransformToPose(getPxJoint()->getRelativeTransform());
}

void PhysxJointComponent::onAttach() {
  if (mChild->getEntity().get() != getEntity().get()) {
    throw std::runtime_error(
        "physx drive component and its attach body must be attached to the same entity.");
  }
}

void PhysxJointComponent::onAddToScene(Scene &scene) {
  internalRefresh();
  getPxJoint()->setConstraintFlag(PxConstraintFlag::eDISABLE_CONSTRAINT, false);
}

void PhysxJointComponent::onRemoveFromScene(Scene &scene) {
  getPxJoint()->setConstraintFlag(PxConstraintFlag::eDISABLE_CONSTRAINT, true);
}

std::shared_ptr<PhysxDriveComponent>
PhysxDriveComponent::Create(std::shared_ptr<PhysxRigidBodyComponent> body) {
  auto drive = std::make_shared<PhysxDriveComponent>(body);
  body->internalRegisterJoint(drive);
  return drive;
}

PhysxDriveComponent::PhysxDriveComponent(std::shared_ptr<PhysxRigidBodyComponent> body)
    : PhysxJointComponent(body) {
  if (!body) {
    throw std::runtime_error("physx drive must be initialized with a valid rigid body");
  }
  mJoint = PxD6JointCreate(*mEngine->getPxPhysics(), nullptr, PxTransform(PxIdentity),
                           body->getPxActor(), PxTransform(PxIdentity));

  for (auto axis : {PxD6Axis::eX, PxD6Axis::eY, PxD6Axis::eZ, PxD6Axis::eTWIST, PxD6Axis::eSWING1,
                    PxD6Axis::eSWING2}) {
    mJoint->setMotion(axis, PxD6Motion::eFREE);
  }
  mJoint->setConstraintFlag(PxConstraintFlag::eDISABLE_CONSTRAINT, true);
}

void PhysxDriveComponent::setLinearLimit(PxD6Axis::Enum axis, float low, float high,
                                         float stiffness, float damping) {
  if (low < 0 && std::isinf(low) && high > 0 && std::isinf(high)) {
    mJoint->setMotion(axis, PxD6Motion::eFREE);
    return;
  }

  if (low == 0.f && high == 0.f && stiffness == 0.f && damping == 0.f) {
    mJoint->setMotion(axis, PxD6Motion::eLOCKED);
    return;
  }

  mJoint->setMotion(axis, PxD6Motion::eLIMITED);
  mJoint->setLinearLimit(axis, {low, high, {stiffness, damping}});
}

void PhysxDriveComponent::setXLimit(float low, float high, float stiffness, float damping) {
  setLinearLimit(PxD6Axis::eX, low, high, stiffness, damping);
}
void PhysxDriveComponent::setYLimit(float low, float high, float stiffness, float damping) {
  setLinearLimit(PxD6Axis::eY, low, high, stiffness, damping);
}
void PhysxDriveComponent::setZLimit(float low, float high, float stiffness, float damping) {
  setLinearLimit(PxD6Axis::eZ, low, high, stiffness, damping);
}
void PhysxDriveComponent::setYZConeLimit(float yAngle, float zAngle, float stiffness,
                                         float damping) {
  mJoint->setSwingLimit({yAngle, zAngle, {stiffness, damping}});
}
void PhysxDriveComponent::setYZPyramidLimit(float yLow, float yHigh, float zLow, float zHigh,
                                            float stiffness, float damping) {
  mJoint->setPyramidSwingLimit({yLow, yHigh, zLow, zHigh, {stiffness, damping}});
}
void PhysxDriveComponent::setXTwistLimit(float low, float high, float stiffness, float damping) {
  mJoint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);
  mJoint->setTwistLimit({low, high, {stiffness, damping}});
}

void PhysxDriveComponent::setDrive(PxD6Drive::Enum drive, float stiffness, float damping,
                                   float forceLimit, DriveMode mode) {
  mJoint->setDrive(drive, {stiffness, damping, forceLimit, mode == DriveMode::eACCELERATION});
}
void PhysxDriveComponent::setXDriveProperties(float stiffness, float damping, float forceLimit,
                                              DriveMode mode) {
  setDrive(PxD6Drive::eX, stiffness, damping, forceLimit, mode);
}
void PhysxDriveComponent::setYDriveProperties(float stiffness, float damping, float forceLimit,
                                              DriveMode mode) {
  setDrive(PxD6Drive::eY, stiffness, damping, forceLimit, mode);
}
void PhysxDriveComponent::setZDriveProperties(float stiffness, float damping, float forceLimit,
                                              DriveMode mode) {
  setDrive(PxD6Drive::eZ, stiffness, damping, forceLimit, mode);
}
void PhysxDriveComponent::setXTwistDriveProperties(float stiffness, float damping,
                                                   float forceLimit, DriveMode mode) {
  setDrive(PxD6Drive::eTWIST, stiffness, damping, forceLimit, mode);
}
void PhysxDriveComponent::setYZSwingDriveProperties(float stiffness, float damping,
                                                    float forceLimit, DriveMode mode) {
  setDrive(PxD6Drive::eSWING, stiffness, damping, forceLimit, mode);
}
void PhysxDriveComponent::setSlerpDriveProperties(float stiffness, float damping, float forceLimit,
                                                  DriveMode mode) {
  setDrive(PxD6Drive::eSLERP, stiffness, damping, forceLimit, mode);
}

void PhysxDriveComponent::setDriveTarget(Pose const &pose) {
  mJoint->setDrivePosition(PoseToPxTransform(pose));
}
Pose PhysxDriveComponent::getDriveTarget() const {
  return PxTransformToPose(mJoint->getDrivePosition());
}

void PhysxDriveComponent::setDriveTargetVelocity(Vec3 const &linear, Vec3 const &angular) {
  mJoint->setDriveVelocity(Vec3ToPxVec3(linear), Vec3ToPxVec3(angular));
}
std::tuple<Vec3, Vec3> PhysxDriveComponent::getDriveTargetVelocity() const {
  PxVec3 linear, angular;
  mJoint->getDriveVelocity(linear, angular);
  return {PxVec3ToVec3(linear), PxVec3ToVec3(angular)};
}

PhysxDriveComponent::~PhysxDriveComponent() {
  assert(weak_from_this().expired());
  if (mParent) {
    mParent->internalClearExpiredJoints();
  }
  mChild->internalClearExpiredJoints();
  mJoint->release();
}

std::shared_ptr<PhysxGearComponent>
PhysxGearComponent::Create(std::shared_ptr<PhysxRigidBodyComponent> body) {
  auto gear = std::make_shared<PhysxGearComponent>(body);
  body->internalRegisterJoint(gear);
  return gear;
}

PhysxGearComponent::PhysxGearComponent(std::shared_ptr<PhysxRigidBodyComponent> body)
    : PhysxJointComponent(body) {
  if (!body) {
    throw std::runtime_error("physx gear must be initialized with a valid rigid body");
  }
  mJoint = PxGearJointCreate(*mEngine->getPxPhysics(), nullptr, PxTransform(PxIdentity),
                             body->getPxActor(), PxTransform(PxIdentity));
  mJoint->setGearRatio(1.f);
  mJoint->setConstraintFlag(PxConstraintFlag::eDISABLE_CONSTRAINT, true);
}

float PhysxGearComponent::getGearRatio() const { return mJoint->getGearRatio(); }
void PhysxGearComponent::setGearRatio(float ratio) { mJoint->setGearRatio(ratio); }

void PhysxGearComponent::internalRefresh() {
  PhysxJointComponent::internalRefresh();

  auto parentLink = std::dynamic_pointer_cast<PhysxArticulationLinkComponent>(mParent);
  auto childLink = std::dynamic_pointer_cast<PhysxArticulationLinkComponent>(mChild);
  if (!parentLink || !childLink) {
    return;
  }
  auto parentJoint = parentLink->getPxActor()->getInboundJoint();
  auto childJoint = childLink->getPxActor()->getInboundJoint();
  if (!parentJoint || !childJoint) {
    return;
  }

  if (mHingesEnabled) {
    if (parentJoint->getJointType() != PxArticulationJointType::eREVOLUTE &&
        parentJoint->getJointType() != PxArticulationJointType::eREVOLUTE_UNWRAPPED) {
      return;
    }
    if (childJoint->getJointType() != PxArticulationJointType::eREVOLUTE &&
        childJoint->getJointType() != PxArticulationJointType::eREVOLUTE_UNWRAPPED) {
      return;
    }
    mJoint->setHinges(parentJoint, childJoint);
  }
}

void PhysxGearComponent::enableHinges() {
  mHingesEnabled = true;
  internalRefresh();
}

PhysxGearComponent::~PhysxGearComponent() {
  assert(weak_from_this().expired());
  if (mParent) {
    mParent->internalClearExpiredJoints();
  }
  mChild->internalClearExpiredJoints();
  mJoint->release();
}

std::shared_ptr<PhysxDistanceJointComponent>
PhysxDistanceJointComponent::Create(std::shared_ptr<PhysxRigidBodyComponent> body) {
  auto distanceJoint = std::make_shared<PhysxDistanceJointComponent>(body);
  body->internalRegisterJoint(distanceJoint);
  return distanceJoint;
}

PhysxDistanceJointComponent::PhysxDistanceJointComponent(
    std::shared_ptr<PhysxRigidBodyComponent> body)
    : PhysxJointComponent(body) {
  if (!body) {
    throw std::runtime_error("physx distanceJoint must be initialized with a valid rigid body");
  }
  mJoint = PxDistanceJointCreate(*mEngine->getPxPhysics(), nullptr, PxTransform(PxIdentity),
                                 body->getPxActor(), PxTransform(PxIdentity));
  mJoint->setConstraintFlag(PxConstraintFlag::eDISABLE_CONSTRAINT, true);
  mJoint->setTolerance(0.f);

  setLimit(0.f, 0.f, 0.f, 0.f);
}

void PhysxDistanceJointComponent::setLimit(float low, float high, float stiffness, float damping) {
  if (high < PX_MAX_F32) {
    mJoint->setDistanceJointFlag(PxDistanceJointFlag::eMAX_DISTANCE_ENABLED, true);
    mJoint->setMaxDistance(high);
  } else {
    mJoint->setDistanceJointFlag(PxDistanceJointFlag::eMAX_DISTANCE_ENABLED, false);
    mJoint->setMaxDistance(high);
  }

  if (low > 0) {
    mJoint->setDistanceJointFlag(PxDistanceJointFlag::eMIN_DISTANCE_ENABLED, true);
    mJoint->setMinDistance(low);
  } else {
    mJoint->setDistanceJointFlag(PxDistanceJointFlag::eMIN_DISTANCE_ENABLED, false);
    mJoint->setMinDistance(low);
  }

  // TODO: check correctness here
  if (stiffness == 0.f && damping == 0.f) {
    mJoint->setDistanceJointFlag(PxDistanceJointFlag::eSPRING_ENABLED, false);
    mJoint->setStiffness(stiffness);
    mJoint->setDamping(damping);
  } else {
    mJoint->setDistanceJointFlag(PxDistanceJointFlag::eSPRING_ENABLED, true);
    mJoint->setStiffness(stiffness);
    mJoint->setDamping(damping);
  }
}

float PhysxDistanceJointComponent::getStiffness() const { return mJoint->getStiffness(); }
float PhysxDistanceJointComponent::getDamping() const { return mJoint->getDamping(); }
Eigen::Vector2f PhysxDistanceJointComponent::getLimit() const {
  return {mJoint->getMinDistance(), mJoint->getMaxDistance()};
}
void PhysxDistanceJointComponent::setContactDistance(float distance) {
  mJoint->setContactDistance(distance);
}
float PhysxDistanceJointComponent::getContactDistance() const {
  return mJoint->getContactDistance();
}

float PhysxDistanceJointComponent::getDistance() const { return mJoint->getDistance(); }

PhysxDistanceJointComponent::~PhysxDistanceJointComponent() {
  assert(weak_from_this().expired());
  if (mParent) {
    mParent->internalClearExpiredJoints();
  }
  mChild->internalClearExpiredJoints();
  mJoint->release();
}

} // namespace component
} // namespace sapien
