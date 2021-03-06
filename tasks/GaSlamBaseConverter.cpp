#include "GaSlamBaseConverter.hpp"

namespace ga_slam {

void GaSlamBaseConverter::convertBaseCloudToPCL(
        const BaseCloud& baseCloud,
        Cloud::Ptr& pclCloud) {
    pclCloud->clear();
    pclCloud->reserve(baseCloud.points.size());
    pclCloud->is_dense = true;
    pclCloud->header.stamp = baseCloud.time.toMicroseconds();

    for (const auto& point : baseCloud.points)
        pclCloud->push_back(pcl::PointXYZ(point.x(), point.y(), point.z()));
}

void GaSlamBaseConverter::convertPCLToBaseCloud(
        BaseCloud& baseCloud,
        const Cloud::ConstPtr& pclCloud) {
    baseCloud.points.clear();
    baseCloud.points.reserve(pclCloud->size());
    baseCloud.time = BaseTime::fromMicroseconds(pclCloud->header.stamp);

    for (const auto& point : pclCloud->points)
        baseCloud.points.push_back(base::Point(point.x, point.y, point.z));
}

void GaSlamBaseConverter::convertMapToBaseImage(
        BaseImage& image,
        const Map& map) {
    const auto params = map.getParameters();
    const auto meanData = map.getMeanZ();

    image.width = params.size;
    image.height = params.size;
    image.data.clear();
    image.data.resize(image.width * image.height, NAN);
    image.time = BaseTime::fromMicroseconds(map.getTimestamp());

    for (auto&& it = map.begin(); !it.isPastEnd(); ++it) {
        const grid_map::Index index(*it);
        const grid_map::Index imageIndex(it.getUnwrappedIndex());
        const float value = meanData(index(0), index(1));
        image.data[imageIndex(0) * image.height + imageIndex(1)] = value;
    }
}

void GaSlamBaseConverter::convertMapToBaseCloud(
        BaseCloud& baseCloud,
        const Map& map) {
    const auto params = map.getParameters();

    baseCloud.points.clear();
    baseCloud.points.reserve(params.size * params.size);
    baseCloud.time = BaseTime::fromMicroseconds(map.getTimestamp());

    const auto& meanData = map.getMeanZ();
    Eigen::Vector3d point;

    for (auto&& it = map.begin(); !it.isPastEnd(); ++it) {
        map.getPointFromArrayIndex(*it, meanData, point);
        baseCloud.points.push_back(point);
    }
}

}  // namespace ga_slam

