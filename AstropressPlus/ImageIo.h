#pragma once

#include "EigenHeaders.h"

void SetDumpDir(std::string directory);
void DumpImage(const char* basename, Eigen::MatrixXd const& image);
void SaveImage(const char* filename, Eigen::MatrixXd const& image);
Eigen::MatrixXd LoadImage(const char* filename);
