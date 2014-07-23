#pragma once

#include "EigenHeaders.h"
#include <vector>

Eigen::MatrixXd SolveTransform(
	std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d>> const& reference,
	std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d>> const& toSolve,
	Eigen::MatrixXd guess);
