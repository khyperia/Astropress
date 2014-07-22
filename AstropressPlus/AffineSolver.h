#pragma once

#include "EigenAssert.h"
#include <eigen3/Eigen/StdVector>
#include <eigen3/Eigen/Eigenvalues>

Eigen::MatrixXd SolveTransform(
	std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d>> const& reference,
	std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d>> const& toSolve,
	Eigen::MatrixXd guess);
