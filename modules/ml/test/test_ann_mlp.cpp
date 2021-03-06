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

#include <gtest/gtest.h>
// c++
#include <opencv2/core/ocl.hpp>
// ssig
#include "ssiglib/ml/results.hpp"
#include "ssiglib/ml/pca_embedding.hpp"
#include "ssiglib/ml/ann_mlp.hpp"
class ANN_IrisTest : public ::testing::Test {
 protected:
  cv::Mat labels;
  cv::Mat X, Y, data;
  cv::Mat testX, testY;
  cv::Ptr<ssig::MultilayerPerceptron> ann_mlp;


  void SetUp() override {
    srand(1234);
    cv::theRNG().state = 1234;
    cv::FileStorage inp_file("iris.yml", cv::FileStorage::READ);
    inp_file["samples"] >> data;

    labels = cv::Mat(150, 3, CV_32F, 0.f);
    labels.rowRange(0, 50).col(0) = 1;
    labels.rowRange(50, 100).col(1) = 1;
    labels.rowRange(100, 150).col(2) = 1;

    X = data.rowRange(10, 50);
    X.push_back(data.rowRange(60, 100));
    X.push_back(data.rowRange(110, 150));
    Y = labels.rowRange(10, 50);
    Y.push_back(labels.rowRange(60, 100));
    Y.push_back(labels.rowRange(110, 150));

    testX = data.rowRange(0, 10);
    testX.push_back(data.rowRange(50, 60));
    testX.push_back(data.rowRange(100, 110));
    testY = cv::Mat::zeros(30, 1, CV_32S);
    testY.rowRange(10, 20) = 1;
    testY.rowRange(20, 30) = 2;

    ann_mlp = ssig::MultilayerPerceptron::create();
  }
};
TEST_F(ANN_IrisTest, SampleMultilayerPerceptron) {
  ann_mlp->addLayer(5, 0);
  ann_mlp->addLayer(3, 0);

  ann_mlp->setMaxIterations(1000);
  ann_mlp->setLearningRate(static_cast<float>(1e-2));
  ann_mlp->setEpsilon(0.01f);

  ann_mlp->learn(X, Y);
  cv::Mat_<int> actual;
  cv::Mat_<float> resp;
  ann_mlp->predict(testX, resp, actual);

  ssig::Results res(actual, testY);
  float acc = res.getAccuracy();

  EXPECT_GT(acc, 0.9f);
}

TEST_F(ANN_IrisTest, Relu) {
  // Adding the input
  ann_mlp->addLayer(5, 0);
  ann_mlp->addLayer(3, 0, 0.0f, "softmax");

  ann_mlp->setMaxIterations(1500);
  // ann_mlp->setVerbose(true);
  ann_mlp->setLearningRate(static_cast<float>(1e-3));
  ann_mlp->setEpsilon(0.01f);

  ann_mlp->learn(X, Y);
  cv::Mat_<int> actual;
  cv::Mat_<float> resp;
  ann_mlp->predict(testX, resp, actual);

  ssig::Results res(actual, testY);
  float acc = res.getAccuracy();

  EXPECT_GT(acc, 0.9f);
}

TEST_F(ANN_IrisTest, SoftMax) {
  // Adding the input
  ann_mlp->addLayer(5, 0);
  ann_mlp->addLayer(3, 0, 0.0f, "relu");

  ann_mlp->setMaxIterations(1500);
  // ann_mlp->setVerbose(true);
  ann_mlp->setLearningRate(static_cast<float>(1e-3));
  ann_mlp->setEpsilon(0.01f);

  ann_mlp->learn(X, Y);
  cv::Mat_<int> actual;
  cv::Mat_<float> resp;
  ann_mlp->predict(testX, resp, actual);

  ssig::Results res(actual, testY);
  float acc = res.getAccuracy();

  EXPECT_GT(acc, 0.9f);
}

TEST_F(ANN_IrisTest, LogLoss) {
  ann_mlp->addLayer(5, 0);
  ann_mlp->addLayer(3, 0, 0.f, "softmax");

  ann_mlp->setMaxIterations(5000);
  ann_mlp->setLearningRate(static_cast<float>(1e-5));
  ann_mlp->setEpsilon(0.3f);
  ann_mlp->setLossType("log");

  ann_mlp->learn(X, Y);
  cv::Mat_<int> actual;
  cv::Mat_<float> resp;
  ann_mlp->predict(testX, resp, actual);

  ssig::Results res(actual, testY);
  float acc = res.getAccuracy();

  EXPECT_GT(acc, 0.6f);
}

TEST_F(ANN_IrisTest, OCL) {
  ann_mlp->setUseOpenCl(true);
  // Adding the input
  ann_mlp->addLayer(5, 0);
  ann_mlp->addLayer(3, 0);

  ann_mlp->setMaxIterations(1000);
  ann_mlp->setLearningRate(static_cast<float>(1e-2));
  ann_mlp->setEpsilon(0.01f);

  ann_mlp->learn(X, Y);
  cv::Mat_<int> actual;
  cv::Mat_<float> resp;
  ann_mlp->predict(testX, resp, actual);

  ssig::Results res(actual, testY);
  float acc = res.getAccuracy();

  EXPECT_GT(acc, 0.9f);
}
