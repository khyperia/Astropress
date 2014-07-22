#include "AffineResampler.h"

double Index(Eigen::MatrixXd const& matrix, int row, int col)
{
	if (row < 0 || col < 0 || row >= matrix.rows() || col >= matrix.cols())
		return 0.0;
	return matrix(row, col);
}

double Index(Eigen::MatrixXd const& matrix, double row, double col)
{
	int introw = static_cast<int>(floor(row));
	int intcol = static_cast<int>(floor(col));
	row -= introw;
	col -= intcol;
	return
		Index(matrix, introw, intcol) * (1 - row) * (1 - col) +
		Index(matrix, introw + 1, intcol) * (row) * (1 - col) +
		Index(matrix, introw, intcol + 1) * (1 - row) * (col) +
		Index(matrix, introw + 1, intcol + 1) * (row) * (col);
}

Eigen::MatrixXd AffineResample(Eigen::MatrixXd const& source, Eigen::MatrixXd affine)
{
	Eigen::MatrixXd result(source.rows(), source.cols());
	for (int row = 0; row < result.rows(); row++)
	{
		for (int col = 0; col < result.cols(); col++)
		{
			Eigen::Vector2d index = affine * Eigen::Vector3d(row, col, 1);
			result(row, col) = Index(source, index[0], index[1]);
		}
	}
	return result;
}
