#define _SCL_SECURE_NO_WARNINGS 1
#include "StarFinder.h"

#include <unsupported/Eigen/FFT>
#undef _SCL_SECURE_NO_WARNINGS

#include "ImageIo.h"

#define USE_FFT 0

static bool starLocationDumpEnabled = false;
static bool flatImageDumpEnabled = false;
static double flatPercent = 1.0;
static int numLowFrequencies = 5;

void EnableStarLocationDump()
{
	starLocationDumpEnabled = true;
}

void EnableFlatImageDump()
{
	flatImageDumpEnabled = true;
}

void SetFlatPercent(char const* str)
{
	try
	{
		flatPercent = stod(std::string(str));
		if (flatPercent <= 0)
			throw std::runtime_error("--star_threshhold cannot be <= 0");
	}
	catch (std::invalid_argument)
	{
		throw std::runtime_error("Could not parse --star_threshhold argument");
	}
}

void SetNumLowFrequencies(char const* str)
{
	try
	{
		numLowFrequencies = stoi(std::string(str));
		if (numLowFrequencies < 0)
			throw std::runtime_error("--freq_removal cannot be < 0");
	}
	catch (std::invalid_argument)
	{
		throw std::runtime_error("Could not parse --freq_removal argument");
	}
}

#if USE_FFT
// http://stackoverflow.com/questions/17194451
Eigen::MatrixXcd FFT(Eigen::MatrixXd in)
{
	Eigen::FFT<double> fft;
	Eigen::MatrixXcd out(in.rows(), in.cols());

	for (int k = 0; k < in.rows(); k++) {
		Eigen::RowVectorXcd tmpOut;
		fft.fwd(tmpOut, in.row(k));
		out.row(k) = tmpOut;
	}

	for (int k = 0; k < in.cols(); k++) {
		Eigen::VectorXcd tmpOut;
		fft.fwd(tmpOut, out.col(k));
		out.col(k) = tmpOut;
	}
	return out;
}

Eigen::MatrixXd IFFT(Eigen::MatrixXcd in)
{
	Eigen::FFT<double> fft;
	Eigen::MatrixXd out(in.rows(), in.cols());

	for (int k = 0; k < in.rows(); k++) {
		Eigen::RowVectorXcd tmpOut;
		fft.inv(tmpOut, in.row(k));
		in.row(k) = tmpOut;
	}

	for (int k = 0; k < in.cols(); k++) {
		Eigen::VectorXd tmpOut;
		fft.inv(tmpOut, in.col(k));
		out.col(k) = tmpOut;
	}
	return out;
}
// end stackoverflow link
#else

/// Round up to next higher power of 2 (return x if it's already a power
/// of 2).
inline int pow2roundup(int x)
{
	if (x < 0)
		return 0;
	--x;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x + 1;
}

// Daubechies 4
// http://web.mit.edu/seven/src/AFNI/Daubechies.c
Eigen::VectorXd WaveletForward(Eigen::VectorXd input)
{
	Eigen::VectorXd tempA(input.size());
	Eigen::VectorXd tempC(input.size());
	const double h[4] = { 0.683013, 1.18301, 0.316987, -0.183013 };

	for (int n = input.size(); n > 1; n /= 2)
	{
		for (int i = 0; i < n / 2; i++)
		{
			tempA[i] = (h[0] * input[(2 * i) % n] + h[1] * input[(2 * i + 1) % n] + h[2] * input[(2 * i + 2) % n] + h[3] * input[(2 * i + 3) % n]) / 2.0;
			tempC[i] = (h[3] * input[(2 * i) % n] - h[2] * input[(2 * i + 1) % n] + h[1] * input[(2 * i + 2) % n] - h[0] * input[(2 * i + 3) % n]) / 2.0;
		}
		for (int i = 0; i < n / 2; i++)
		{
			input[i] = tempA[i];
			input[i + n / 2] = tempC[i];
		}
	}

	return input;
}

Eigen::VectorXd WaveletInverse(Eigen::VectorXd input)
{
	const double h[4] = { 0.683013, 1.18301, 0.316987, -0.183013 };

	for (int n = 2; n <= input.size(); n *= 2)
	{
		int nptsd2 = n / 2;
		Eigen::VectorXd a = input.head(nptsd2);
		Eigen::VectorXd c = input.head(n).tail(nptsd2);
		for (int i = 0; i < nptsd2; i++)
		{
			input[2 * i] = h[2] * a[(i - 1 + nptsd2) % nptsd2] + h[1] * c[(i - 1 + nptsd2) % nptsd2] + h[0] * a[i] + h[3] * c[i];
			input[2*i+1] = h[3] * a[(i - 1 + nptsd2) % nptsd2] - h[0] * c[(i - 1 + nptsd2) % nptsd2] + h[1] * a[i] - h[2] * c[i];
		}
	}
	return input;
}

Eigen::MatrixXd WaveletForward2d(Eigen::MatrixXd input, double fillValue)
{
	int rows = input.rows();
	int cols = input.cols();
	input.conservativeResize(pow2roundup(input.rows()), pow2roundup(input.cols()));
	input.rightCols(input.cols() - cols).fill(fillValue);
	input.bottomRows(input.rows() - rows).fill(fillValue);

	for (int row = 0; row < input.rows(); row++)
		input.row(row) = WaveletForward(input.row(row));
	for (int col = 0; col < input.cols(); col++)
		input.col(col) = WaveletForward(input.col(col));
	return input;
}

Eigen::MatrixXd WaveletInverse2d(Eigen::MatrixXd input, int rows, int cols)
{
	for (int col = 0; col < input.cols(); col++)
		input.col(col) = WaveletInverse(input.col(col));
	for (int row = 0; row < input.rows(); row++)
		input.row(row) = WaveletInverse(input.row(row));
	return input.topLeftCorner(rows, cols);
}
#endif

double Median(Eigen::MatrixXd block)
{
	auto data = block.data();
	auto nth = data + block.size() / 2;
	auto last = data + block.size();
	nth_element(data, nth, last, std::greater<int>());
	return *nth;
}

double ReversePercentile(Eigen::MatrixXd block, double percentile)
{
	auto data = block.data();
	auto nth = data + static_cast<int>(block.size() * percentile / 100);
	auto last = data + block.size();
	nth_element(data, nth, last, std::greater<int>());
	return *nth;
}

void ThreshHold(Eigen::MatrixXd& image)
{
	printf("Removing low frequencies... ");
#if USE_FFT
	auto fft = FFT(image);
	for (int row = -numLowFrequencies; row <= numLowFrequencies; row++)
	{
		for (int col = -numLowFrequencies; col <= numLowFrequencies; col++)
		{
			int realRow = row < 0 ? row + fft.rows() : row;
			int realCol = col < 0 ? col + fft.cols() : col;
			fft(realRow, realCol) = 0;
		}
	}
	image = IFFT(fft);
#else
	int rows = image.rows();
	int cols = image.cols();
	auto wav = WaveletForward2d(image, Median(image));

	for (int row = 0; row < (1 << numLowFrequencies); row++)
	{
		for (int col = 0; col < (1 << numLowFrequencies); col++)
		{
			wav(row, col) = 0;
		}
	}

	//DumpImage("wavelet", wav + Eigen::MatrixXd::Random(wav.rows(), wav.cols()) * 0.001);

	image = WaveletInverse2d(wav, rows, cols);
#endif
	puts("Done");

	//DumpImage("recon", image.array() - image.minCoeff());

	double thresh = ReversePercentile(image, flatPercent);
	image = image.array() - thresh;
	image = image.cwiseMax(0);

	if (flatImageDumpEnabled)
	{
		DumpImage("flattened", image);
	}
}

typedef Eigen::Array<bool, Eigen::Dynamic, Eigen::Dynamic> ArrayXb;

void FloodFill(Eigen::MatrixXd& image, int row, int col, double& centerRow, double& centerCol, double& totalSum, int& totalValues)
{
	if (totalValues > 2048 || row < 0 || row >= image.rows() || col < 0 || col >= image.cols())
		return;
	double value = image(row, col);
	if (value <= 0)
		return;
	totalValues++;
	centerRow += row * value;
	centerCol += col * value;
	totalSum += value;
	image(row, col) = 0;
	FloodFill(image, row + 1, col, centerRow, centerCol, totalSum, totalValues);
	FloodFill(image, row, col + 1, centerRow, centerCol, totalSum, totalValues);
	FloodFill(image, row - 1, col, centerRow, centerCol, totalSum, totalValues);
	FloodFill(image, row, col - 1, centerRow, centerCol, totalSum, totalValues);
}

Eigen::Vector3d FitAndRemoveGaussian(Eigen::MatrixXd& image, int row, int col)
{
	double sumMasked = 0;
	double rowf = 0;
	double colf = 0;
	int totalValues = 0;
	FloodFill(image, row, col, rowf, colf, sumMasked, totalValues);
	rowf /= sumMasked;
	colf /= sumMasked;
	if (totalValues < 25 || totalValues > 2048)
	{
		auto nan = std::numeric_limits<double>::quiet_NaN();
		return Eigen::Vector3d(nan, nan, nan);
	}
	return Eigen::Vector3d(sumMasked, rowf, colf);
}

void DumpStarLocations(std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d>> stars, Eigen::MatrixXd image)
{
	const int boxSize = 10;
	const double max = image.maxCoeff();
	for (unsigned int i = 0; i < stars.size(); i++)
	{
		for (int x = -boxSize; x <= boxSize; x++)
		{
			for (int y = -boxSize; y <= boxSize; y++)
			{
				if (x != -boxSize && x != boxSize && y != -boxSize && y != boxSize)
					continue;
				int row = static_cast<int>(stars[i][0]) + y;
				int col = static_cast<int>(stars[i][1]) + x;
				if (row < 0 || row >= image.rows() || col < 0 || col >= image.cols())
					continue;
				image(row, col) = max;
			}
		}
	}
	DumpImage("stars", image);
}

std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d>> FindStars(Eigen::MatrixXd image)
{
	Eigen::MatrixXd copyStarLocation;
	if (starLocationDumpEnabled)
	{
		copyStarLocation = image;
	}

	ThreshHold(image);
	std::vector<Eigen::Vector3d, Eigen::aligned_allocator<Eigen::Vector3d>> result;
	for (int row = 0; row < image.rows(); row++)
	{
		for (int col = 0; col < image.cols(); col++)
		{
			if (image(row, col) > 0)
			{
				auto index = FitAndRemoveGaussian(image, row, col);
				if (index.hasNaN() == false)
				{
					result.push_back(index);
				}
			}
		}
	}
	sort(result.begin(), result.end(), [](const Eigen::Vector3d& left, const Eigen::Vector3d& right)
	{
		return left[0] < right[0];
	});
	std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d>> result2d(result.size());
	for (unsigned int i = 0; i < result.size(); i++)
		result2d[i] = Eigen::Vector2d(result[i][1], result[i][2]);

	if (starLocationDumpEnabled)
	{
		DumpStarLocations(result2d, copyStarLocation);
	}

	return result2d;
}
