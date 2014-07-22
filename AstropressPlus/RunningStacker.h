#pragma once

#include "EigenAssert.h"
#include <eigen3/Eigen/Core>

class RunningStacker
{
	int n = 0;
	Eigen::MatrixXd mean;
	Eigen::MatrixXd M2;
public:
	RunningStacker();
	~RunningStacker();
	void Stack(Eigen::MatrixXd input);
	Eigen::MatrixXd Mean();
	Eigen::MatrixXd Stdev();
};

