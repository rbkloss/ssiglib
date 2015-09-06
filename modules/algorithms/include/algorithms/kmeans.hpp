/*L*************************************************************************************************
*
*  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
* 
*  By downloading, copying, installing or using the software you agree to this license. If you do 
*  not agree to this license, do not download, install, copy or use the software.
*
*                            Software License Agreement (BSD License)
*                               For Smart Surveillance Framework
*                                 http://ssig.dcc.ufmg.br/ssf/
*
*  Copyright (c) 2013, Smart Surveillance Interest Group, all rights reserved.
*  
*  Redistribution and use in source and binary forms, with or without modification, are permitted 
*  provided that the following conditions are met:
*
*    1. Redistributions of source code must retain the above copyright notice, this list of 
*       conditions and the following disclaimer.
*
*    2. Redistributions in binary form must reproduce the above copyright notice, this list of 
*       conditions and the following disclaimer in the documentation and/or other materials 
*       provided with the distribution.
*
*    3. Neither the name of the copyright holder nor the names of its contributors may be used to 
*       endorse or promote products derived from this software without specific prior written 
*       permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR 
*  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY 
*  AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
*  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
*  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
*  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
*  POSSIBILITY OF SUCH DAMAGE.
*************************************************************************************************L*/

#ifndef _SSF_ALGORITHMS_KMEANS_HPP_
#define _SSF_ALGORITHMS_KMEANS_HPP_

#include "clusteringMethod.hpp"

namespace ssf{
struct KmeansParams : ClusteringParams{
  int flags = cv::KMEANS_RANDOM_CENTERS;
  int nAttempts = 1;
  int predicitonDistanceType = cv::NORM_L2;
};

class Kmeans : ClusteringMethod{
public:
  Kmeans(void) = default;
  virtual ~Kmeans(void) = default;
  Kmeans(const Kmeans& rhs);
  Kmeans& operator=(const Kmeans& rhs);
  void setup(cv::Mat_<float>& input, const std::vector<Cluster> & initialClustering, ClusteringParams* parameters) override;
  std::vector<Cluster> learn(cv::Mat_<float>& input, ClusteringParams* parameters) override;
  cv::Mat_<float> predict(cv::Mat_<float>& sample)const override;
  std::vector<Cluster> getResults()const override;
  cv::Mat_<float> getCentroids() const override;

  cv::Mat_<float> getState()const override;
  void load(const std::string& filename, const std::string& nodename = "") override;
  void save(const std::string& filename, const std::string& nodename = "")const override;
  void clear() override;

private:
  //private members
  cv::Mat_<float> centroids_;
  int flags_;
  int nAttempts_;
  int predicitonDistanceType_;

};

}

#endif // !_SSF_ALGORITHMS_KMEANS_HPP_

