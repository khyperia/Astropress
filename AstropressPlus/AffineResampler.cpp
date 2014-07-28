#include "AffineResampler.h"
#include <iostream>

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
		Index(matrix, introw + 1, intcol) * (row)* (1 - col) +
		Index(matrix, introw, intcol + 1) * (1 - row) * (col)+
		Index(matrix, introw + 1, intcol + 1) * (row)* (col);
}

double MedianNoMiddle(Eigen::MatrixXd const& mat)
{
	std::vector<double> toSort(mat.size() - 1);
	int i = 0;
	for (int row = 0; row < mat.rows(); row++)
	{
		for (int col = 0; col < mat.cols(); col++)
		{
			if (row != mat.rows() / 2 || col != mat.cols() / 2)
			{
				toSort[i++] = mat(row, col);
			}
		}
	}
	if (i != toSort.size())
		throw std::runtime_error("Internal error: MedianNoMiddle: i != toSort.size");
	const auto nth = toSort.begin() + toSort.size() / 2;
	std::nth_element(toSort.begin(), nth, toSort.end());
	return *nth;
}

int RemoveBadPixels(Eigen::MatrixXd& image)
{
	double toSort[8];
	auto max = image.maxCoeff();
	auto min = image.minCoeff();
	int hotPixels = 0;
	for (int row = 1; row < image.rows() - 1; row++)
	{
		for (int col = 1; col < image.cols() - 1; col++)
		{
			int i = 0;
			for (int drow = -1; drow <= 1; drow++)
				for (int dcol = -1; dcol <= 1; dcol++)
					if (drow != 0 || dcol != 0)
						toSort[i++] = image(row + drow, col + dcol);
			std::nth_element(toSort, toSort + 4, toSort + 8);
			auto median = toSort[4];
			if (abs(image(row, col) - median) > (max - min) / 10)
			{
				image(row, col) = median;
				hotPixels++;
			}
		}
	}
	return hotPixels;
}

Eigen::MatrixXd AffineResample(Eigen::MatrixXd const& source, Eigen::MatrixXd affine)
{
	const int subsample = 2;
	Eigen::MatrixXd result(source.rows() * subsample, source.cols() * subsample);
	for (int row = 0; row < result.rows(); row++)
	{
		for (int col = 0; col < result.cols(); col++)
		{
			Eigen::Vector2d index = affine * Eigen::Vector3d(static_cast<double>(row) / subsample, static_cast<double>(col) / subsample, 1);
			result(row, col) = Index(source, index[0], index[1]);
		}
	}
	return result;
}
