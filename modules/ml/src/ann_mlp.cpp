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

// c++
#include <algorithm>
#include <vector>
#include <string>
// cv
#include <opencv2/core.hpp>
// ssig

// local
#include "ssiglib/ml/ann_mlp.hpp"

namespace ssig {
MultilayerPerceptron::MultilayerPerceptron() {
  // Constructor
}

cv::Ptr<MultilayerPerceptron> MultilayerPerceptron::create() {
  auto ans = cv::Ptr<MultilayerPerceptron>(new MultilayerPerceptron());
  return ans;
}

cv::Ptr<MultilayerPerceptron> MultilayerPerceptron::create(
  const std::vector<std::string>& activationTypes,
  const std::vector<int>& layersLength,
  const std::vector<float>& dropouts) {
  auto ans = cv::Ptr<MultilayerPerceptron>(new MultilayerPerceptron());
  ans->setDropoutWeights(dropouts);
  ans->setActivationsTypes(activationTypes);
  ans->setLayersLength(layersLength);

  return ans;
}

MultilayerPerceptron::~MultilayerPerceptron() {
  // Destructor
}

MultilayerPerceptron::MultilayerPerceptron(const MultilayerPerceptron& rhs) {
  // Constructor Copy
}

template <class MatType>
void MultilayerPerceptron::computeLossDerivative(
  const std::string& lossType,
  const MatType& activation,
  const MatType& target,
  MatType& error) const {
  if (lossType == "quadratic") {
    cv::subtract(activation, target, error);
  } else if (lossType == "log") {
    MatType diff, aux1, aux2;

    cv::subtract(activation, target, aux1);
    cv::subtract(1, activation, aux2);
    cv::multiply(activation, aux2, aux2);
    cv::add(aux2, FLT_EPSILON, aux2);

    cv::divide(aux1, aux2, error);
  }
}

template <class MatType>
float MultilayerPerceptron::computeLoss(
  const std::string& loss,
  const MatType& out,
  const MatType& target) const {
  float avg_loss = 0.f;
  if (loss == "log") {
    MatType m1, m2;
    cv::add(out, FLT_EPSILON, m1);
    cv::log(m1, m1);
    cv::multiply(target, m1, m1);
    avg_loss = static_cast<float>(
      (cv::sum(m1) / (target.rows * target.cols))[0]);
    cv::subtract(1, out, m2);

    cv::add(m2, FLT_EPSILON, m2);
    cv::log(m2, m2);
    MatType aux;
    cv::subtract(1, target, aux);
    cv::multiply(aux, m2, m2);
    avg_loss += static_cast<float>(
      (cv::sum(m2) / (target.rows * target.cols))[0]);
    avg_loss = -avg_loss;
  } else if (loss == "quadratic") {
    MatType aux;
    cv::subtract(out, target, aux);
    cv::pow(aux, 2, aux);
    avg_loss = static_cast<float>(cv::sum(aux)[0]);
    avg_loss = avg_loss / (target.rows * target.cols);
  }
  return avg_loss;
}

template <class MatType>
void MultilayerPerceptron::doForwardPass(
  const MatType& input,
  const std::vector<MatType>& weights,
  const std::vector<std::string>& activationTypes,
  const std::vector<float>& dropout,
  std::vector<MatType>& outputs,
  std::vector<MatType>& activations) const {
  const int numLayers = mNumLayers;
  outputs.resize(numLayers + 1);
  activations.resize(numLayers + 1);

  outputs[0] = input;
  activations[0] = input;

  for (int l = 0; l < numLayers; ++l) {
    MatType layerResponse;
    cv::gemm(weights[l], activations[l], 1, cv::noArray(), 0, layerResponse);

    outputs[l] = layerResponse;
    applyActivation(activationTypes[l], layerResponse, layerResponse);
    if (dropout[l] > 0.05f && dropout[l] < 0.8f) {
      cv::multiply(mDropouts[l], layerResponse, layerResponse);
    }
    activations[l + 1] = layerResponse;
  }
}

void MultilayerPerceptron::setWeights(const int startNodes) {
  mWeights.resize(mActivationsTypes.size());
  const int numWeights = static_cast<int>(mWeights.size());
  const int weightEnd = numWeights - 1;
  mWeights[0] = cv::Mat::zeros(
    mNumNodesConfiguration[0] + 1,
    startNodes, CV_32F);
  cv::randn(mWeights[0],
            cv::Mat(1, 1, CV_32F, 0.f),
            cv::Mat(1, 1, CV_32F, 1.f));
  for (int i = 1; i < weightEnd - 1; ++i) {
    const int ncols = mNumNodesConfiguration[i - 1] + 1,
      nrows = mNumNodesConfiguration[i] + 1;
    // plus one due to the bias factor
    mWeights[i] = cv::Mat::zeros(nrows, ncols, CV_32F);
    cv::randn(
      mWeights[i], cv::Mat(1, 1, CV_32F, 0.f),
      cv::Mat(1, 1, CV_32F, 1.f));
  }
  mWeights[weightEnd] = cv::Mat::zeros(
    mNumNodesConfiguration[weightEnd],
    mWeights[weightEnd - 1].rows, CV_32F);
  cv::randn(
    mWeights[weightEnd],
    cv::Mat(1, 1, CV_32F, 0.f),
    cv::Mat(1, 1, CV_32F, 1.f));
}

void MultilayerPerceptron::learn(
  const cv::Mat_<float>& _input,
  const cv::Mat& labels) {
  mIsTrained = false;

  if (mOpenClEnabled) {
    cv::UMat inp, lab;
    _input.copyTo(inp);
    labels.copyTo(lab);
    _learn(inp, lab);
  } else {
    cv::Mat inp = _input;
    _learn(inp, labels);
  }
  mIsTrained = true;
}

template <class MatType>
void MultilayerPerceptron::_learn(
  const MatType& _input,
  const MatType& labels) {
  MatType input;
  MatType target;
  cv::transpose(labels, target);

  MatType ones = MatType(_input.rows, 1, CV_32F, 1);
  cv::hconcat(_input, ones, input);
  input = input.t();
  // create weight matrices
  setWeights(input.rows);
  // END Weights
  char* loss_msg =
    "The average loss in the output layer of this epoch is [%g]\n";
  std::vector<MatType> activations,
    weights(mWeights.size());

  for (int i = 0; i < static_cast<int>(mWeights.size()); ++i) {
    mWeights[i].copyTo(weights[i]);
  }
  for (int it = 0; it < mMaxIterations; ++it) {
    learnWeights(
      input,
      target,
      mActivationsTypes,
      mDropoutWeights,
      activations,
      weights);

    MatType out;
    activations.back().copyTo(out);

    auto avg_loss = computeLoss(mLoss, out, target);
    if (it % 100 == 0) {
      verboseLog(loss_msg, avg_loss);
    }
    if (avg_loss < mEpsilon) {
      break;
    }
  }

  for (int i = 0; i < weights.size(); ++i) {
    weights[i].copyTo(mWeights[i]);
  }
}

int MultilayerPerceptron::predict(
  const cv::Mat_<float>& _inp,
  cv::Mat_<float>& resp,
  cv::Mat_<int>& labels) const {
  std::vector<cv::Mat> layerOut;
  std::vector<cv::Mat> activations;
  cv::Mat inp;
  cv::Mat ones = cv::Mat(_inp.rows, 1, CV_32F, 1);
  cv::hconcat(_inp, ones, inp);
  inp = inp.t();

  predict(
    inp,
    resp,
    labels,
    activations,
    layerOut);

  return static_cast<int>(NAN);
}

void MultilayerPerceptron::predict(
  const int layerIdx,
  const cv::Mat& _inp,
  cv::Mat& resp,
  cv::Mat& labels) const {
  std::vector<cv::Mat> activations,
                       outputs;
  predict(_inp, resp, labels, activations, outputs);
  resp = activations[layerIdx];
}

void MultilayerPerceptron::predict(
  const cv::Mat& inp,
  cv::Mat& resp,
  cv::Mat& labels,
  std::vector<cv::Mat>& activations,
  std::vector<cv::Mat>& outputs) const {
  doForwardPass<cv::Mat>(
    inp,
    mWeights,
    mActivationsTypes,
    mDropoutWeights,
    outputs,
    activations);
  resp = activations.back().t();
  labels = cv::Mat(resp.rows, 1, CV_32S, -1);
  for (int r = 0; r < resp.rows; ++r) {
    int maxIdx[2];
    cv::minMaxIdx(resp.row(r), nullptr, nullptr, nullptr, maxIdx);
    labels.at<int>(r) = maxIdx[1];
  }
}

void MultilayerPerceptron::addLayer(
  const int numNodes,
  const int poolingSize,
  const float dropout,
  const std::string& activation) {
  CV_Assert(numNodes > 0);
  if (poolingSize > 0) {
    // TODO(Ricardo): pooling
  }

  mDropoutWeights.push_back(dropout);
  mNumNodesConfiguration.push_back(numNodes);
  mActivationsTypes.push_back(activation);
}

void MultilayerPerceptron::setLossType(const std::string& loss) {
  mLoss = loss;
}

bool MultilayerPerceptron::empty() const {
  return mWeights.empty();
}

bool MultilayerPerceptron::isTrained() const {
  return mIsTrained;
}

Classifier* MultilayerPerceptron::clone() const {
  return new MultilayerPerceptron(*this);
}

template <class MatType>
void MultilayerPerceptron::learnWeights(
  const MatType& inputs,
  const MatType& labels,
  const std::vector<std::string>& activationTypes,
  const std::vector<float>& dropout,
  std::vector<MatType>& activations,
  std::vector<MatType>& weights) const {
  const int numLayers = static_cast<int>(weights.size()) + 1;
  CV_Assert(weights.size() == numLayers - 1);
  CV_Assert(activationTypes.size() == numLayers - 1);
  CV_Assert(dropout.size() == numLayers - 1);

  std::vector<MatType> net_out;
  doForwardPass(inputs, weights,
                activationTypes,
                dropout,
                net_out,
                activations);
  std::vector<MatType> errors;
  computeErrors(
    labels,
    activationTypes,
    weights,
    net_out,
    activations,
    errors);
  gradientUpdate<MatType>(
    mLearningRate,
    activations,
    errors,
    weights);
}

template <class MatType>
void MultilayerPerceptron::computeErrors(
  const MatType& target,
  const std::vector<std::string>& activationTypes,
  const std::vector<MatType>& weights,
  const std::vector<MatType>& outputs,
  const std::vector<MatType>& activations,
  std::vector<MatType>& errors) const {
  //////////////////////////////////////////////////////
  const int numLayers = mNumLayers;
  errors.resize(numLayers + 1);
  // this is the derivative of the loss function
  computeLossDerivative(mLoss,
                        activations.back(),
                        target,
                        errors.back());
  for (int L = numLayers - 1; L > 0; --L) {
    MatType aux;
    cv::gemm(weights[L], errors[L + 1], 1,
             cv::noArray(), 0, aux, cv::GEMM_1_T);
    MatType derivative;
    applyDerivative(activationTypes[L - 1], outputs[L - 1], derivative);
    cv::multiply(aux, derivative, errors[L]);
  }
}

template <class MatType>
void MultilayerPerceptron::gradientUpdate(
  const float learningRate,
  const std::vector<MatType>& activations,
  const std::vector<MatType>& errors,
  std::vector<MatType>& weights) const {
  const int len = static_cast<int>(weights.size());
  std::vector<MatType> newWeights(weights.size());
  for (int i = 1; i <= len; ++i) {
    MatType G_l;
    cv::gemm(
      errors[i], activations[i - 1],
      1, cv::noArray(), 1, G_l, cv::GEMM_2_T);
    cv::addWeighted(
      weights[i - 1], 1, G_l, -learningRate, 0, newWeights[i - 1]);
  }
  weights = std::move(newWeights);
}

void MultilayerPerceptron::applyActivation(
  const std::string& type,
  cv::InputArray _inp,
  cv::OutputArray _out) {
  if (type == "relu") {
    relu(_inp, _out);
  } else if (type == "logistic") {
    logistic(_inp, _out);
  } else if (type == "softmax") {
    softmax(_inp, _out);
  } else if (type == "softplus") {
    softplus(_inp, _out);
  } else {
    _inp.copyTo(_out);
  }
}

void MultilayerPerceptron::applyDerivative(
  const std::string& type,
  cv::InputArray _inp,
  cv::OutputArray _out) {
  if (type == "relu") {
    dRelu(_inp, _out);
  } else if (type == "logistic") {
    dLogistic(_inp, _out);
  } else if (type == "softmax") {
    dSoftmax(_inp, _out);
  } else if (type == "softplus") {
    dSoftplus(_inp, _out);
  } else {
    _inp.copyTo(_out);
  }
}

void MultilayerPerceptron::relu(
  const cv::InputArray& _inp,
  cv::OutputArray& _out) {
  if (_inp.isUMat()) {
    cv::UMat inp = _inp.getUMat();
    cv::UMat out;
    cv::max(0, inp, out);
    out.copyTo(_out);
  } else {
    cv::Mat inp = _inp.getMat();
    cv::Mat out = cv::max(0, inp);
    out.copyTo(_out);
  }
}

void MultilayerPerceptron::logistic(
  const cv::InputArray& _inp,
  cv::OutputArray& _out) {
  if (_inp.isUMat()) {
    cv::UMat inp = _inp.getUMat();
    cv::UMat out;
    cv::multiply(-1, inp, out);
    cv::exp(out, out);
    cv::add(1, out, out);
    cv::divide(1, out, out);
    out.copyTo(_out);
  } else {
    cv::Mat inp = _inp.getMat();
    cv::Mat out;
    cv::multiply(-1, inp, out);
    cv::exp(out, out);
    cv::add(1, out, out);
    cv::divide(1, out, out);
    out.copyTo(_out);
  }
}

void MultilayerPerceptron::softmax(
  cv::InputArray _inp,
  cv::OutputArray _out) {
  if (_inp.isUMat()) {
    cv::UMat inp = _inp.getUMat();
    cv::UMat out;
    cv::exp(inp, out);
    cv::add(out, FLT_EPSILON, out);
    for (int c = 0; c < out.cols; ++c) {
      cv::normalize(out.col(c), out.col(c), 1, 0, cv::NORM_L1);
    }
    out.copyTo(_out);
  } else {
    cv::Mat inp = _inp.getMat();
    cv::Mat out;
    cv::exp(inp, out);
    cv::min(out, 1e10, out);
    cv::add(out, FLT_EPSILON, out);
    for (int c = 0; c < out.cols; ++c) {
      cv::normalize(out.col(c), out.col(c), 1, 0, cv::NORM_L1);
      if (!cv::checkRange(out.col(c))) {
        perror("NAN found\n");
      }
    }
    out.copyTo(_out);
  }
}

void MultilayerPerceptron::softplus(
  cv::InputArray _inp,
  cv::OutputArray _out) {
  static const float EULER = 2.7182818284590452353602874713527f;
  if (_inp.isUMat()) {
    cv::UMat out;
    cv::exp(_inp, out);
    cv::add(1, out, out);
    cv::log(out, out);
    // cv::UMat aux(out.size(), CV_32F, EULER);
    // cv::log(aux, aux);
    // cv::divide(out, aux, out);
    out.copyTo(_out);
  } else {
    cv::Mat out;
    cv::Mat aux(out.size(), CV_32F, EULER);
    // for finding the natural log
    cv::exp(_inp, out);
    cv::add(1, out, out);
    // cv::log(out, out);
    // cv::log(aux, aux);
    // cv::divide(out, aux, out);
    // ln(x) = log(x) / log(e)
    out.copyTo(_out);
  }
}

void MultilayerPerceptron::dRelu(
  cv::InputArray _inp,
  cv::OutputArray _out) {
  if (_inp.isUMat()) {
    cv::UMat comp_mask;
    cv::UMat local_zeros = cv::UMat::zeros(_inp.size(), CV_32F);
    cv::compare(_inp, local_zeros, comp_mask, CV_CMP_EQ);
    cv::UMat::ones(_inp.size(), CV_32F).copyTo(_out);
    local_zeros.copyTo(_out, comp_mask);
  } else {
    cv::Mat comp_mask;
    cv::Mat local_zeros = cv::Mat::zeros(_inp.size(), CV_32F);
    cv::compare(_inp, local_zeros, comp_mask, CV_CMP_EQ);
    cv::Mat out = cv::Mat::ones(_inp.size(), CV_32F);
    local_zeros.copyTo(out, comp_mask);
    out.copyTo(_out);
  }
}

void MultilayerPerceptron::dSoftplus(
  cv::InputArray _inp,
  cv::OutputArray _out) {
  logistic(_inp, _out);
}

void MultilayerPerceptron::dLogistic(
  cv::InputArray _inp,
  cv::OutputArray _out) {
  if (_inp.isUMat()) {
    cv::UMat out;
    logistic(_inp, out);
    cv::UMat aux;
    cv::subtract(1, out, aux);
    cv::multiply(out, aux, _out);
  } else {
    cv::Mat out;
    logistic(_inp, out);
    cv::Mat aux;
    cv::subtract(1, out, aux);
    cv::multiply(out, aux, _out);
  }
}

void MultilayerPerceptron::dSoftmax(
  cv::InputArray _inp,
  cv::OutputArray _out) {
  // TODO(Ricardo): implement this
}

void MultilayerPerceptron::read(const cv::FileNode& fn) {
  int numWeights;
  fn["numWeights"] >> numWeights;
  cv::FileNode node;
  node = fn["weights"];

  mWeights.resize(numWeights);
  for (int i = 0; i < numWeights; ++i) {
    node["weight_" + std::to_string(i)] >> mWeights[i];
  }

  fn["numLayers"] >> mNumLayers;

  node = fn["activations"];
  int len = static_cast<int>(node.size());
  mActivationsTypes.resize(len);
  for (int i = 0; i < len; ++i) {
    node[i] >> mActivationsTypes[i];
  }

  fn["learningRate"] >> mLearningRate;
  fn["numNodesConfig"] >> mNumNodesConfiguration;
  fn["dropoutWeights"] >> mDropoutWeights;
  fn["dropouts"] >> mDropouts;
  fn["loss"] >> mLoss;
}

void MultilayerPerceptron::write(cv::FileStorage& fs) const {
  const int numWeights = static_cast<int>(mWeights.size());
  fs << "numWeights" << numWeights;
  fs << "weights" << "{";

  for (int i = 0; i < numWeights; ++i) {
    fs << "weight_" + std::to_string(i) << mWeights[i];
  }

  fs << "numLayers" << mNumLayers;
  fs << "activations" << mActivationsTypes;
  fs << "learningRate" << mLearningRate;
  fs << "numNodesConfig" << mNumNodesConfiguration;
  fs << "dropoutWeights" << mDropoutWeights;
  fs << "dropouts" << mDropouts;
  fs << "loss" << mLoss;
}

cv::Mat MultilayerPerceptron::getLabels() const {
  return cv::Mat();
}

cv::Mat MultilayerPerceptron::getWeights(const int layerIndex) {
  return mWeights[layerIndex];
}

std::unordered_map<int, int> MultilayerPerceptron::getLabelsOrdering() const {
  std::unordered_map<int, int> ans;
  return {{0, 1}};
}

float MultilayerPerceptron::getLearningRate() const {
  return mLearningRate;
}

void MultilayerPerceptron::setLearningRate(const float learningRate) {
  mLearningRate = learningRate;
}

int MultilayerPerceptron::getNumLayers() const {
  return mNumLayers;
}

void MultilayerPerceptron::setNumLayers(const int numLayers) {
  mNumLayers = numLayers;
}

std::vector<cv::Mat> MultilayerPerceptron::getWeightMatrices() const {
  return mWeights;
}

void MultilayerPerceptron::setWeights(const std::vector<cv::Mat>& weights) {
  mWeights = weights;
}

std::vector<cv::Mat> MultilayerPerceptron::getDropouts() const {
  return mDropouts;
}

void MultilayerPerceptron::setDropouts(const std::vector<cv::Mat>& dropouts) {
  mDropouts = dropouts;
}

std::vector<cv::Mat> MultilayerPerceptron::getLayerActivations() const {
  return mLayerActivations;
}

void MultilayerPerceptron::setLayerActivations
(const std::vector<cv::Mat>& layerActivations) {
  mLayerActivations = layerActivations;
}

std::vector<cv::Mat> MultilayerPerceptron::getLayerOut() const {
  return mLayerOut;
}

void MultilayerPerceptron::setLayerOut(const std::vector<cv::Mat>& layerOut) {
  mLayerOut = layerOut;
}

std::vector<int> MultilayerPerceptron::getNumNodesConfiguration() const {
  return mNumNodesConfiguration;
}

void MultilayerPerceptron::setLayersLength(
  const std::vector<int>& numNodesConfiguration) {
  mNumNodesConfiguration = numNodesConfiguration;
}

std::vector<std::string> MultilayerPerceptron::getActivationsTypes() const {
  return mActivationsTypes;
}

void MultilayerPerceptron::setActivationsTypes(
  const std::vector<std::string>& activationsTypes) {
  mActivationsTypes = activationsTypes;
}

std::vector<float> MultilayerPerceptron::getDropoutWeights() const {
  return mDropoutWeights;
}

void MultilayerPerceptron::setDropoutWeights(
  const std::vector<float>& dropoutWeights) {
  mDropoutWeights = dropoutWeights;
}

std::string MultilayerPerceptron::getLossType() const {
  return mLoss;
}
}  // namespace ssig
