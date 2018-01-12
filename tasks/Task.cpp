#include "Task.hpp"
#include "GaSlamBaseConverter.hpp"

#include "grid_map_cereal/GridMapCereal.hpp"

#include <mutex>

namespace ga_slam {

bool Task::configureHook(void) {
    if (!TaskBase::configureHook()) return false;

    if (!_body2ground.get(BaseTime::now(), bodyToGroundTF_, false)) {
        RTT::log(RTT::Error) << "Body to ground TF not found" << RTT::endlog();
        error(TRANSFORM_NOT_FOUND);
        return false;
    }

    if (!_hazcam2body.get(BaseTime::now(), hazcamToBodyTF_, false)) {
        RTT::log(RTT::Error) << "HazCam to body TF not found" << RTT::endlog();
        error(TRANSFORM_NOT_FOUND);
        return false;
    }

    if (!_loccam2body.get(BaseTime::now(), loccamToBodyTF_, false)) {
        RTT::log(RTT::Error) << "LocCam to body TF not found" << RTT::endlog();
        error(TRANSFORM_NOT_FOUND);
        return false;
    }

    gaSlam_.setParameters(_mapLengthX.rvalue(), _mapLengthY.rvalue(),
            _mapResolution.rvalue(), _minElevation.rvalue(),
            _maxElevation.rvalue(), _voxelSize.rvalue(),
            _numParticles.rvalue(), _resampleFrequency.rvalue(),
            _initialSigmaX.rvalue(), _initialSigmaY.rvalue(),
            _initialSigmaYaw.rvalue(), _predictSigmaX.rvalue(),
            _predictSigmaY.rvalue(), _predictSigmaYaw.rvalue(),
            _traversedDistanceThreshold.rvalue(), _slopeSumThreshold.rvalue());

    return true;
}

void Task::updateHook(void) {
    TaskBase::updateHook();

    if (isFutureReady(poseGuessFuture_) &&
            _poseGuess.read(poseGuess_) == RTT::NewData) {
        poseGuessFuture_ = std::async(std::launch::async, &GaSlam::poseCallback,
                &gaSlam_, poseGuess_.getTransform(), bodyToGroundTF_);
    }

    if (isFutureReady(hazcamCloudFuture_) &&
            _hazcamCloud.read(hazcamCloud_) == RTT::NewData) {
        std::cout << "[GA SLAM] HazCam cloud received!" << std::endl;

        hazcamCloudFuture_ = std::async(std::launch::async,
                &Task::cloudCallback, this, hazcamCloud_, hazcamToBodyTF_);
    }

    if (isFutureReady(loccamCloudFuture_) &&
            _loccamCloud.read(loccamCloud_) == RTT::NewData) {
        std::cout << "[GA SLAM] LocCam cloud received!" << std::endl;

        loccamCloudFuture_ = std::async(std::launch::async,
                &Task::cloudCallback, this, loccamCloud_, loccamToBodyTF_);
    }

    if (isFutureReady(pancamCloudFuture_) &&
            _pancamCloud.read(pancamCloud_) == RTT::NewData &&
            _pancamTransformation.read(basePancamToBodyTF_) == RTT::NewData) {
        std::cout << "[GA SLAM] PanCam cloud received!" << std::endl;

        pancamCloudFuture_ = std::async(std::launch::async,
                &Task::cloudCallback, this, pancamCloud_,
                basePancamToBodyTF_.getTransform());
    }

    if (isFutureReady(orbiterCloudFuture_) &&
            _orbiterCloud.read(orbiterCloud_) == RTT::NewData) {
        std::cout << "[GA SLAM] Orbiter cloud received!" << std::endl;

        orbiterCloudFuture_ = std::async(std::launch::async, [&] {
            Cloud::Ptr cloud(new Cloud);
            GaSlamBaseConverter::convertBaseCloudToPCL(orbiterCloud_, cloud);
            gaSlam_.registerOrbiterCloud(cloud);
        });
    }

    if (_debugInfoEnabled.rvalue()) outputDebugInfo();
}

void Task::cloudCallback(
        const BaseCloud& baseCloud,
        const Pose& sensorToBodyTF) {
    Cloud::Ptr cloud(new Cloud);
    GaSlamBaseConverter::convertBaseCloudToPCL(baseCloud, cloud);

    gaSlam_.cloudCallback(cloud, sensorToBodyTF);
}

void Task::outputDebugInfo(void) {
    if (_rawMapDebugEnabled.rvalue()) {
        BaseImage rawMapBaseImage;

        std::unique_lock<std::mutex> guard(gaSlam_.getRawMapMutex());
        GaSlamBaseConverter::convertMapToBaseImage(rawMapBaseImage,
                gaSlam_.getRawMap());
        guard.unlock();

        _rawElevationMap.write(rawMapBaseImage);
    }

    if (_cloudDebugEnabled.rvalue()) {
        BaseCloud mapBaseCloud;

        std::unique_lock<std::mutex> guard(gaSlam_.getRawMapMutex());
        GaSlamBaseConverter::convertMapToBaseCloud(mapBaseCloud,
                gaSlam_.getRawMap());
        guard.unlock();

        _mapCloud.write(mapBaseCloud);
    }

    if (_serializationDebugEnabled.rvalue()) {
        savePose(gaSlam_.getPose(), _posePath.rvalue());

        std::unique_lock<std::mutex> rawMapGuard(gaSlam_.getRawMapMutex());
        saveGridMap(gaSlam_.getRawMap().getGridMap(), _localMapPath.rvalue());
        rawMapGuard.unlock();

        std::lock_guard<std::mutex> globalMapGuard(gaSlam_.getGlobalMapMutex());
        saveGridMap(gaSlam_.getGlobalMap().getGridMap(),
                _globalMapPath.rvalue());
    }
}

}  // namespace ga_slam

