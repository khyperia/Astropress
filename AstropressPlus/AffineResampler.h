#pragma once

#include "EigenHeaders.h"

int RemoveBadPixels(Eigen::MatrixXd& image);
Eigen::MatrixXd AffineResample(Eigen::MatrixXd const& source, Eigen::MatrixXd affine, double subsample);
