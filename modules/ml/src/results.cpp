/*L*****************************************************************************
*
*  Copyright (c) 2015, Smart Surveillance Interest Group, all rights reserved.
*
*  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
*
*  By downloading, copying, installing or using the software you agree to this
*  license. If you do not agree to this license, do not download, install, copy
*  or use the software.
*
*                Software License Agreement (BSD License)
*             For Smart Surveillance Interest Group Library
*                         http://ssig.dcc.ufmg.br
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*
*    1. Redistributions of source code must retain the above copyright notice,
*       this list of conditions and the following disclaimer.
*
*    2. Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*
*    3. Neither the name of the copyright holder nor the names of its
*       contributors may be used to endorse or promote products derived from
*       this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
*  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
*  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
*  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
*  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
*  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
*  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
*  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
*  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
*  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************L*/

#include "ssiglib/ml/results.hpp"

#include <limits.h>
#include <sstream>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <ctime>
#include <vector>

#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core.hpp>

#include <ssiglib/ml/classification.hpp>

namespace ssig {
Results::Results(
  const cv::Mat_<int>& actualLabels,
  const cv::Mat_<int>& expectedLabels) :
  mGroundTruth(expectedLabels),
  mLabels(actualLabels) {}

void Results::computeLabelsVec(
  const cv::Mat_<int>& labelMat,
  const cv::Mat_<int>& gtLabelMat,
  std::unordered_map<int, int>& labelsMap) const {
  std::vector<int> labelsVec(labelMat.rows + gtLabelMat.rows);
  for (int i = 0; i < labelMat.rows; ++i) {
    auto label = labelMat.at<int>(i);
    labelsVec[i] = label;
  }

  for (int i = 0; i < gtLabelMat.rows; ++i) {
    auto label = gtLabelMat.at<int>(i);
    labelsVec[i + labelMat.rows] = label;
  }
  std::sort(labelsVec.begin(), labelsVec.end(), [](const int a, const int b) {
              return a < b;
            });
  auto pos = std::unique(labelsVec.begin(), labelsVec.end());
  labelsVec.erase(pos, labelsVec.end());
  for (int i = 0; i < static_cast<int>(labelsVec.size()); ++i) {
    labelsMap[labelsVec[i]] = i;
  }
}

void Results::compute(
  const cv::Mat_<int>& groundTruth,
  const cv::Mat_<int>& labels,
  std::unordered_map<int, int>& labelsVec,
  cv::Mat_<int>& confusionMatrix) const {
  computeLabelsVec(labels, groundTruth, labelsVec);

  int len = static_cast<int>(labelsVec.size());
  confusionMatrix = cv::Mat_<int>::zeros(len, len);
  for (int i = 0; i < groundTruth.rows; ++i) {
    auto value = labels.at<int>(i);
    auto gt = groundTruth.at<int>(i);
    confusionMatrix[labelsVec[gt]][labelsVec[value]] += 1;
  }
}

int Results::getClassesLen() const {
  return mConfusionMatrix.rows;
}

float Results::getAccuracy() {
  if (mConfusionMatrix.empty())
    compute(mGroundTruth, mLabels,
            mLabelMap,
            mConfusionMatrix);

  auto accrcy = cv::trace(mConfusionMatrix);
  accrcy = accrcy / cv::sum(mConfusionMatrix);
  return static_cast<float>(accrcy[0]);
}

float Results::getMeanAccuracy() {
  float ans = 0.f;
  int ncols = mConfusionMatrix.cols;
  for (int r = 0; r < mConfusionMatrix.rows; ++r) {
    float accuracy = 0;
    if (r < ncols) {
      float sum = static_cast<float>(cv::sum(mConfusionMatrix.row(r))[0]);
      if (sum > 0.f) {
        accuracy = mConfusionMatrix.at<int>(r, r) / (sum);
      }
    }
    ans += accuracy;
  }
  ans = ans / mConfusionMatrix.rows;
  return ans;
}

cv::Mat Results::getConfusionMatrix() {
  if (mConfusionMatrix.empty())
    compute(mGroundTruth,
            mLabels,
            mLabelMap,
            mConfusionMatrix);
  return mConfusionMatrix;
}

void Results::getLabelMap(std::unordered_map<int, int>& rowLabels,
                          std::unordered_map<int, int>& colLabels) const {
  for (const auto& it : mLabelMap)
    colLabels[it.second] = it.first;

  rowLabels = colLabels;
}

void Results::setStringLabels(std::unordered_map<int,
                                                 std::string>& stringLabels) {
  mStringLabels = stringLabels;
}

std::vector<int> Results::getLabelsCount() {
  if (mConfusionMatrix.empty())
    compute(mGroundTruth, mLabels,
            mLabelMap,
            mConfusionMatrix);
  int nrows = mConfusionMatrix.rows;
  std::vector<int> ans(mConfusionMatrix.rows);

  for (int r = 0; r < nrows; ++r) {
    ans[r] = static_cast<int>(cv::sum(mConfusionMatrix.row(r))[0]);
  }
  return ans;
}

std::pair<float, float> Results::crossValidation(
  const cv::Mat_<float>& features,
  const cv::Mat_<int>& labels,
  const int nfolds,
  const size_t seed,
  ssig::Classifier& classifier,
  std::vector<Results>& out) {
  cv::Mat_<float> accuracies(nfolds, 1, 0.0f);
  cv::RNG rng(static_cast<uint64>(seed));
  const int len = features.rows;
  cv::Mat_<int> ordering(len, 1);
  for (int i = 0; i < len; ++i)
    ordering.at<int>(i) = i;
  cv::randShuffle(ordering, 5, &rng);
  int foldLen = static_cast<int>(len / static_cast<float>(nfolds));
  cv::Mat_<float> test, train;

  for (int fold = 0; fold < nfolds; ++fold) {
    const int offset = foldLen * fold;
    cv::Mat_<int> foldOrdering = ordering(cv::Range(offset, foldLen + offset),
                                          cv::Range(0, ordering.cols)).clone();
    cv::sort(foldOrdering, foldOrdering, CV_SORT_EVERY_COLUMN);
    int testIndex = 0;
    cv::Mat_<int> testLabels, trainLabels;
    for (int i = 0; i < len; ++i) {
      auto feature = features.row(i);
      const auto label = labels.at<int>(i);
      if (testIndex < foldOrdering.rows &&
        i == foldOrdering.at<int>(testIndex)) {
        test.push_back(feature);
        testLabels.push_back(label);
        ++testIndex;
      } else {
        train.push_back(feature);
        trainLabels.push_back(label);
      }
    }
    classifier.learn(train, trainLabels);

    cv::Mat_<float> resp;
    cv::Mat_<int> actual;
    for (int r = 0; r < test.rows; ++r) {
      auto query = test.row(r);
      classifier.predict(query, resp);
      auto labelOrdering = classifier.getLabelsOrdering();
      float maxResp = -FLT_MAX;
      int bestLabel = 0;
      for (auto& p : labelOrdering) {
        float curResp = resp.at<float>(p.second);
        if (curResp > maxResp) {
          maxResp = curResp;
          bestLabel = p.first;
        }
      }
      actual.push_back(bestLabel);
    }

    Results result(actual, testLabels);
    out.push_back(result);
    auto acrcy = result.getAccuracy();
    accuracies.at<float>(fold) = acrcy;
    train.release();
    test.release();
  }
  cv::Scalar mean, stdev;
  cv::meanStdDev(accuracies, mean, stdev);

  return std::make_pair(static_cast<float>(mean[0]),
                        static_cast<float>(stdev[0]));
}

void Results::makeConfusionMatrixVisualization(
  const bool color,
  const int blockWidth,
  const cv::Mat_<float>& confusionMatrix,
  cv::Mat& visualization) {
  cv::Mat aux = confusionMatrix.clone();
  aux.convertTo(aux, CV_64F);

  const int nclasses = confusionMatrix.rows;
  const int nrows = aux.rows,
      ncols = aux.cols;

  cv::Mat_<float> visFloat = cv::Mat_<float>::zeros
      (blockWidth * nrows, blockWidth * ncols);
  for (int r = 0; r < confusionMatrix.rows; ++r) {
    cv::normalize(aux.row(r), aux.row(r), 1, 0, cv::NORM_L1);
  }

  for (int i = 0; i < nrows; ++i) {
    for (int j = 0; j < ncols; ++j) {
      cv::Mat roi = visFloat(cv::Rect(j * blockWidth, i * blockWidth,
                                      blockWidth, blockWidth));
      roi = aux.at<double>(i, j);
    }
  }
  cv::Mat temp = aux.clone();
  visFloat.convertTo(temp, CV_8UC1, 255);

  if (color)
    cv::applyColorMap(temp, visualization, cv::COLORMAP_JET);
  else
    cv::applyColorMap(temp, visualization, cv::COLORMAP_BONE);

  temp = visualization.clone();
  cv::cvtColor(temp, visualization, CV_BGR2BGRA);
  for (int i = 0; i < nrows; ++i) {
    char msg[10];
    sprintf(msg, "%3.2f", aux.at<double>(i, i));
    cv::Mat textRoi = visualization(cv::Rect(i * blockWidth, i * blockWidth,
                                       blockWidth, blockWidth));

    /**
    This part place the accuracy as a text in the center of the block
    */
    int baseline;
    const double fontScale = 3;
    int thickness = 4;
    const auto font = cv::HersheyFonts::FONT_HERSHEY_COMPLEX_SMALL;
    cv::Size sz = cv::getTextSize(
                                  msg,
                                  font,
                                  fontScale,
                                  thickness,
                                  &baseline);

    cv::Mat auxText = cv::Mat(sz, CV_8UC4);
    auxText = cv::Scalar(0, 0, 0, 0);
    auto textOrigin = cv::Point((auxText.cols - sz.width) / 2,
                                (auxText.rows + sz.height) / 2);

    cv::putText(auxText, msg, textOrigin,
                cv::HersheyFonts::FONT_HERSHEY_COMPLEX_SMALL,
                fontScale, cv::Scalar(10, 10, 255, 255),
                thickness, cv::LineTypes::LINE_8);
    cv::resize(auxText, auxText, cv::Size(blockWidth, blockWidth));
    cv::Mat mask;
    cv::cvtColor(auxText, mask, CV_BGRA2GRAY, 1);
    auxText.copyTo(textRoi, mask);
  }
  cv::cvtColor(visualization, visualization, CV_BGRA2BGR, 1);
  cv::medianBlur(visualization, visualization, 3);
  for (int i = 0; i < nclasses; ++i) {
    cv::Mat temp = visualization(cv::Rect(i * blockWidth, i * blockWidth,
                                          blockWidth, blockWidth));

    const int x = i * blockWidth;
    const int y = i * blockWidth;
    auto lineColor = CV_RGB(255, 255, 255);
    const int len = blockWidth - 1;
    cv::line(visualization,
             cv::Point(x, y),
             cv::Point(x + len, y),
             lineColor, 1);

    cv::line(visualization,
             cv::Point(x, y),
             cv::Point(x, y + len),
             lineColor, 1);

    cv::line(visualization,
             cv::Point(x + len, y),
             cv::Point(x + len, y + len),
             lineColor, 1);

    cv::line(visualization,
             cv::Point(x, y + len),
             cv::Point(x + len, y + len),
             lineColor, 1);
  }
}

void Results::makeConfusionMatrixVisualization(const bool color,
                                               const int blockWidth,
                                               cv::Mat& visualization) const {
  makeConfusionMatrixVisualization(color,
                                   blockWidth,
                                   mConfusionMatrix,
                                   visualization);
  const int ncols = mConfusionMatrix.cols;
  cv::Mat textLabels;
  makeTextImage(blockWidth, false,
                mLabelMap,
                mStringLabels,
                textLabels);
  float scale = blockWidth * ncols / static_cast<float>(textLabels.cols);
  cv::resize(textLabels, textLabels,
             cv::Size(blockWidth * ncols,
                      static_cast<int>(scale * textLabels.rows)));

  textLabels.push_back(visualization);
  visualization = textLabels.clone();

  makeTextImage(blockWidth, true,
                mLabelMap,
                mStringLabels,
                textLabels);
  cv::Mat block = cv::Mat::zeros(blockWidth, blockWidth, CV_8UC3);
  cv::vconcat(block, textLabels, textLabels);

  cv::hconcat(textLabels, visualization, visualization);
}

void Results::makeTextImage(
  const int blockWidth,
  const bool row,
  const std::unordered_map<int, int>& labelMap,
  const std::unordered_map<int, std::string>& stringLabelsMap,
  cv::Mat& img) const {
  img.release();
  int textLen = 0;
  std::vector<std::string> strVec(labelMap.size());
  if (stringLabelsMap.size() >= labelMap.size()) {
    for (const auto& it : stringLabelsMap) {
      const auto k_v = labelMap.find(it.first);
      if (k_v != labelMap.end()) {
        const auto value = k_v->second;
        strVec[value] = it.second;
      }
      int len = static_cast<int>(it.second.size());
      if (len > textLen)
        textLen = len;
    }
  }

  cv::Size blockSize = {blockWidth, blockWidth};
  int lineWidth = cv::max(1, blockWidth / 2);
  for (int i = 0; i < static_cast<int>(strVec.size()); ++i) {
    cv::Mat auxMat;
    int baseline;
    const std::string& labelText = strVec[i];
    double fontScale = blockWidth * 0.1f;
    int thickness = 3;
    cv::Size sz = cv::getTextSize(labelText,
                                  cv::HersheyFonts::FONT_HERSHEY_COMPLEX_SMALL,
                                  fontScale, thickness, &baseline);
    sz.width += lineWidth;
    auxMat = cv::Mat::zeros(sz, CV_8UC3);
    auxMat = 0;
    auto textOrigin = cv::Point((auxMat.cols - sz.width) / 2,
                                (auxMat.rows + sz.height) / 2);
    cv::putText(auxMat, labelText, textOrigin,
                cv::HersheyFonts::FONT_HERSHEY_COMPLEX_SMALL,
                fontScale, cv::Scalar(255, 255, 255),
                thickness, cv::LineTypes::LINE_AA);
    if (!row) {
      cv::line(auxMat, {auxMat.cols - 1, 0},
               {auxMat.cols - 1, auxMat.rows - 1},
               cv::Scalar(255, 0, 0), lineWidth);
    }

    cv::resize(auxMat, auxMat, blockSize);


    if (img.empty()) {
      img = auxMat;
    } else {
      if (row) {
        cv::vconcat(img, auxMat, img);
      } else {
        cv::hconcat(img, auxMat, img);
      }
    }
  }
}

} // namespace ssig
