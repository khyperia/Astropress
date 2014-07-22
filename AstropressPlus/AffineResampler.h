#pragma once

#include "EigenAssert.h"
#include <eigen3/Eigen/Eigenvalues>

Eigen::MatrixXd AffineResample(Eigen::MatrixXd const& source, Eigen::MatrixXd affine);
