#pragma once

#include "EigenHeaders.h"
#include <vector>

void EnableStarLocationDump();
void EnableFlatImageDump();
void SetFlatPercent(char const* str);
void SetNumHighFrequencies(char const* str);
std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d>> FindStars(Eigen::MatrixXd image);
