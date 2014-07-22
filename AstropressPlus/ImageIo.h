#pragma once

#include "EigenAssert.h"
#include <eigen3/Eigen/Dense>

Eigen::MatrixXd LoadImage(const char* filename);
void SaveImage(const char* filename, Eigen::MatrixXd const& image);
