#pragma once

#include "EigenHeaders.h"
#include <memory>

void SetDumpDir(std::string directory);
void DumpImage(const char* basename, Eigen::MatrixXd const& image);
//void SaveImage(const char* filename, Eigen::MatrixXd const& image);
//Eigen::MatrixXd LoadImage(const char* filename);

class FitsFile
{
public:
	virtual ~FitsFile() {}

	static std::shared_ptr<FitsFile> CreateRead(const char* filename);
	static std::shared_ptr<FitsFile> CreateWrite(std::shared_ptr<FitsFile> copyHeadersFrom, const char* filename);
	static std::shared_ptr<FitsFile> CreateWrite(const char* filename);
	virtual Eigen::MatrixXd ReadImage() = 0;
	virtual void WriteImage(const Eigen::MatrixXd& image) = 0;
};
