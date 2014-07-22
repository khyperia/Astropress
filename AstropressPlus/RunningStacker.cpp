#include "RunningStacker.h"


RunningStacker::RunningStacker()
{
}


RunningStacker::~RunningStacker()
{
}

void RunningStacker::Stack(Eigen::MatrixXd input)
{
	n++;
	if (n == 1)
	{
		mean = input;
		M2 = Eigen::MatrixXd::Zero(input.rows(), input.cols());
		return;
	}
	const Eigen::MatrixXd delta = input - mean;
	mean = mean + delta / n;
	M2 = M2 + delta.cwiseProduct(input - mean);
}

Eigen::MatrixXd RunningStacker::Mean()
{
	return mean;
}

Eigen::MatrixXd RunningStacker::Stdev()
{
	return (M2 / n).cwiseSqrt();
}