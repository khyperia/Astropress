#pragma once

#include "EigenAssert.h"
#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/StdVector>
#include <vector>

std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d>> FindStars(Eigen::MatrixXd image);
