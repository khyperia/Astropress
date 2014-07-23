#pragma once

#include "EigenHeaders.h"

Eigen::MatrixXd AffineResample(Eigen::MatrixXd const& source, Eigen::MatrixXd affine);
