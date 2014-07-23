#include "AffineSolver.h"

#include <set>

std::pair<int, int> ClosestPoint(
	std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d>> const& reference,
	std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d>> const& toSolve,
	Eigen::MatrixXd guess, std::set<int> skipRef, std::set<int> skipSolve)
{
	double min = std::numeric_limits<double>::max();
	std::pair<int, int> result(-1, -1);
	for (unsigned int ref = 0; ref < reference.size(); ref++)
	{
		if (skipRef.find(ref) != skipRef.end())
			continue;
		for (unsigned int solve = 0; solve < toSolve.size(); solve++)
		{
			if (skipSolve.find(solve) != skipSolve.end())
				continue;
			auto rv = reference[ref];
			auto tsv = toSolve[solve];
			// hotspot optimization, explicitly spell out multiplication instead of using Eigen's objects
			auto xLen = rv[0] - guess(0, 0) * tsv[0] - guess(0, 1) * tsv[1] - guess(0, 2);
			auto yLen = rv[1] - guess(1, 0) * tsv[0] - guess(1, 1) * tsv[1] - guess(1, 2);
			auto len = xLen * xLen + yLen * yLen;
			// auto len = (reference[ref] - guess * Eigen::Vector3d(tsv[0], tsv[1], 1)).squaredNorm();
			if (len < min)
			{
				min = len;
				result.first = ref;
				result.second = solve;
			}
		}
	}
	return result;
}

std::pair<Eigen::MatrixXd, std::set<std::pair<int, int>>> UpdateGuessICP(
	std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d>> const& reference,
	std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d>> const& toSolve,
	Eigen::MatrixXd guess)
{
	int count = std::min(reference.size(), toSolve.size()) / 2;
	std::set<int> skipRef;
	std::set<int> skipSolve;

	Eigen::MatrixXd refMat(2, count);
	Eigen::MatrixXd solveMat(3, count);
	std::set<std::pair<int, int>> matches;

	for (int i = 0; i < count; i++)
	{
		auto closestPair = ClosestPoint(reference, toSolve, guess, skipRef, skipSolve);
		if (closestPair.first == -1 || closestPair.second == -1)
		{
			throw std::runtime_error("Could not find enough matching star pairs");
		}
		skipRef.insert(closestPair.first);
		skipSolve.insert(closestPair.second);
		matches.insert(closestPair);
		refMat.col(i) = reference[closestPair.first];
		auto sVec = toSolve[closestPair.second];
		solveMat.col(i) = Eigen::Vector3d(sVec[0], sVec[1], 1);
	}

	Eigen::MatrixXd mul = refMat * solveMat.transpose() * (solveMat * solveMat.transpose()).inverse();
	if (mul.hasNaN())
		throw std::runtime_error("Solved transformation had NaN");
	return make_pair(mul, matches);
}

Eigen::MatrixXd SolveTransform(
	std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d>> const& reference,
	std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d>> const& toSolve,
	Eigen::MatrixXd guess)
{
	std::pair<Eigen::MatrixXd, std::set<std::pair<int, int>>> old;
	old.first = guess;
	for (int i = 0; i < 64; i++)
	{
		auto newPair = UpdateGuessICP(reference, toSolve, old.first);
		if (newPair.second == old.second)
		{
			printf("ICP converged after %i iterations\n", i);
			return newPair.first;
		}
		old = newPair;
	}
	printf("ICP hit max 10 iterations, using last result");
	return old.first;
}
