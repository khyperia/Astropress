#pragma once

#define eigen_assert(x) if (!(x)) throw std::runtime_error("Eigen assert failed: " #x)

#include <Eigen/Dense>
#include <Eigen/StdVector>
