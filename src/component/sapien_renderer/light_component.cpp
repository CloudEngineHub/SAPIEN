#include "sapien/component/sapien_renderer/light_component.h"
#include "sapien/component/sapien_renderer/sapien_renderer_system.h"
#include "sapien/scene.h"
#include <svulkan2/scene/light.h>
#include <svulkan2/scene/scene.h>

namespace sapien {
namespace component {

void SapienRenderPointLightComponent::onAddToScene(Scene &scene) {
  auto system = scene.getSapienRendererSystem();
  auto s = system->getScene();
  mPointLight = &s->addPointLight();
  mPointLight->setColor({mColor.x, mColor.y, mColor.z});
  mPointLight->enableShadow(mShadowEnabled);
  mPointLight->setShadowParameters(mShadowNear, mShadowFar, mShadowMapSize);
  system->registerComponent(
      std::static_pointer_cast<SapienRenderLightComponent>(shared_from_this()));
}

void SapienRenderPointLightComponent::onRemoveFromScene(Scene &scene) {
  auto system = scene.getSapienRendererSystem();
  auto s = system->getScene();
  s->removeNode(*mPointLight);
  system->unregisterComponent(
      std::static_pointer_cast<SapienRenderLightComponent>(shared_from_this()));
}

void SapienRenderDirectionalLightComponent::onAddToScene(Scene &scene) {
  auto system = scene.getSapienRendererSystem();
  auto s = system->getScene();
  mDirectionalLight = &s->addDirectionalLight();
  mDirectionalLight->setColor({mColor.x, mColor.y, mColor.z});
  mDirectionalLight->enableShadow(mShadowEnabled);
  mDirectionalLight->setShadowParameters(mShadowNear, mShadowFar, mShadowHalfSize, mShadowMapSize);
  system->registerComponent(
      std::static_pointer_cast<SapienRenderLightComponent>(shared_from_this()));
}

void SapienRenderDirectionalLightComponent::onRemoveFromScene(Scene &scene) {
  auto system = scene.getSapienRendererSystem();
  auto s = system->getScene();
  s->removeNode(*mDirectionalLight);
  system->unregisterComponent(
      std::static_pointer_cast<SapienRenderLightComponent>(shared_from_this()));
}

void SapienRenderSpotLightComponent::onAddToScene(Scene &scene) {
  auto system = scene.getSapienRendererSystem();
  auto s = system->getScene();
  mSpotLight = &s->addSpotLight();
  mSpotLight->setColor({mColor.x, mColor.y, mColor.z});
  mSpotLight->enableShadow(mShadowEnabled);
  mSpotLight->setShadowParameters(mShadowNear, mShadowFar, mShadowMapSize);
  mSpotLight->setFovSmall(mFovInner);
  mSpotLight->setFov(mFovOuter);
  system->registerComponent(
      std::static_pointer_cast<SapienRenderLightComponent>(shared_from_this()));
}

void SapienRenderSpotLightComponent::onRemoveFromScene(Scene &scene) {
  auto system = scene.getSapienRendererSystem();
  auto s = system->getScene();
  s->removeNode(*mSpotLight);
  system->unregisterComponent(
      std::static_pointer_cast<SapienRenderLightComponent>(shared_from_this()));
}

void SapienRenderTexturedLightComponent::onAddToScene(Scene &scene) {
  auto system = scene.getSapienRendererSystem();
  auto s = system->getScene();
  mSpotLight = &s->addTexturedLight();
  mSpotLight->setColor({mColor.x, mColor.y, mColor.z});
  mSpotLight->enableShadow(mShadowEnabled);
  mSpotLight->setShadowParameters(mShadowNear, mShadowFar, mShadowMapSize);
  mSpotLight->setFovSmall(mFovInner);
  mSpotLight->setFov(mFovOuter);
  getLight()->setTexture(mTexture->getTexture());
  system->registerComponent(
      std::static_pointer_cast<SapienRenderLightComponent>(shared_from_this()));
}

void SapienRenderTexturedLightComponent::onRemoveFromScene(Scene &scene) {
  auto system = scene.getSapienRendererSystem();
  auto s = system->getScene();
  s->removeNode(*mSpotLight);
  system->unregisterComponent(
      std::static_pointer_cast<SapienRenderLightComponent>(shared_from_this()));
}

void SapienRenderParallelogramLightComponent::onAddToScene(Scene &scene) {
  auto system = scene.getSapienRendererSystem();
  auto s = system->getScene();
  mParallelogramLight = &s->addParallelogramLight();
  mParallelogramLight->setColor({mColor.x, mColor.y, mColor.z});
  mParallelogramLight->setShape({mHalfWidth, mHalfHeight}, mAngle);
  system->registerComponent(
      std::static_pointer_cast<SapienRenderLightComponent>(shared_from_this()));
}

void SapienRenderParallelogramLightComponent::onRemoveFromScene(Scene &scene) {
  auto system = scene.getSapienRendererSystem();
  auto s = system->getScene();
  s->removeNode(*mParallelogramLight);
  system->unregisterComponent(
      std::static_pointer_cast<SapienRenderLightComponent>(shared_from_this()));
}

void SapienRenderParallelogramLightComponent::setShape(float halfWidth, float halfHeight,
                                                       float angle) {
  mHalfWidth = halfWidth;
  mHalfHeight = halfHeight;
  mAngle = angle;
  if (mParallelogramLight) {
    mParallelogramLight->setShape({mHalfWidth, mHalfHeight}, mAngle);
  }
}

void SapienRenderPointLightComponent::internalUpdate() {
  auto pose = getGlobalPose() * POSE_GL_TO_ROS;
  mPointLight->setTransform({.position = {pose.p.x, pose.p.y, pose.p.z},
                             .rotation = {pose.q.w, pose.q.x, pose.q.y, pose.q.z}});
}
void SapienRenderDirectionalLightComponent::internalUpdate() {
  auto pose = getGlobalPose() * POSE_GL_TO_ROS;
  mDirectionalLight->setTransform({.position = {pose.p.x, pose.p.y, pose.p.z},
                                   .rotation = {pose.q.w, pose.q.x, pose.q.y, pose.q.z}});
}
void SapienRenderSpotLightComponent::internalUpdate() {
  auto pose = getGlobalPose() * POSE_GL_TO_ROS;
  mSpotLight->setTransform({.position = {pose.p.x, pose.p.y, pose.p.z},
                            .rotation = {pose.q.w, pose.q.x, pose.q.y, pose.q.z}});
}
void SapienRenderTexturedLightComponent::internalUpdate() {
  auto pose = getGlobalPose() * POSE_GL_TO_ROS;
  mSpotLight->setTransform({.position = {pose.p.x, pose.p.y, pose.p.z},
                            .rotation = {pose.q.w, pose.q.x, pose.q.y, pose.q.z}});
}
void SapienRenderParallelogramLightComponent::internalUpdate() {
  auto pose = getGlobalPose() * POSE_GL_TO_ROS;
  mParallelogramLight->setTransform({.position = {pose.p.x, pose.p.y, pose.p.z},
                                     .rotation = {pose.q.w, pose.q.x, pose.q.y, pose.q.z}});
}

void SapienRenderLightComponent::setLocalPose(Pose const &pose) { mLocalPose = pose; }
Pose SapienRenderLightComponent::getLocalPose() const { return mLocalPose; }
Pose SapienRenderLightComponent::getGlobalPose() const { return getPose() * mLocalPose; }

} // namespace component
} // namespace sapien
