//
// Copyright (c) 2013, University of Erlangen-Nuremberg
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#ifndef __HIPACC_BASE_HPP__
#define __HIPACC_BASE_HPP__

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "hipacc_math_functions.hpp"

#define HIPACC_NUM_ITERATIONS 10

#ifdef _WIN32
#define setenv(a, b, c) _putenv_s(a, b)
#endif

class HipaccKernelTimingBase {
private:
  float last_gpu_timing; // milliseconds
public:
  HipaccKernelTimingBase();
  void init_gpu_timing();
  void set_gpu_timing(float t);
  float get_last_kernel_timing();
};

int64_t hipacc_time_micro();

enum class hipaccMemoryType { Global, Linear1D, Linear2D, Array2D, Surface };

class HipaccImageBase {
public:
  virtual ~HipaccImageBase() = 0;
};

class HipaccAccessorBase {
public: // TODO: PRIV
  size_t width, height;
  int32_t offset_x, offset_y;

public:
  HipaccAccessorBase(size_t width, size_t height, int32_t offset_x = 0,
                     int32_t offset_y = 0)
      : width(width), height(height), offset_x(offset_x), offset_y(offset_y) {}
};

// TODO: what is the purpose of this class?
// I have not found any usage beside "class HiappContext : public
// HipaccContextBase"
class HipaccContextBase {
protected:
  HipaccContextBase();
  HipaccContextBase(HipaccContextBase const &);
  void operator=(HipaccContextBase const &);
};

struct hipacc_launch_info {
  hipacc_launch_info(int size_x, int size_y, int is_width, int is_height,
                     int offset_x, int offset_y, int pixels_per_thread,
                     int simd_width);
  hipacc_launch_info(int size_x, int size_y, const HipaccAccessorBase &Acc,
                     int pixels_per_thread, int simd_width);
  int size_x, size_y;
  int is_width, is_height;
  int offset_x, offset_y;
  int pixels_per_thread, simd_width;
  // calculated later on
  int bh_start_left, bh_start_right;
  int bh_start_top, bh_start_bottom;
  int bh_fall_back;
};

struct hipacc_smem_info {
  hipacc_smem_info(int size_x, int size_y, int pixel_size);
  int size_x, size_y;
  int pixel_size;
};

// return function for information logging during runtime
enum class hipaccRuntimeLogLevel { INFO, WARNING, ERROR };

inline int hipaccRuntimeLogTrivial(hipaccRuntimeLogLevel level,
                                   const std::string &message) {
  switch (level) {
  case hipaccRuntimeLogLevel::INFO:
    std::cout << message << std::endl;
    break;
  case hipaccRuntimeLogLevel::WARNING:
    std::cerr << message << std::endl;
    break;
  case hipaccRuntimeLogLevel::ERROR:
    std::cerr << message << std::endl;
    exit(EXIT_FAILURE);
    break;
  default:
    assert(false && "not any hipaccRuntimeLogLevel");
  }
  return 0;
}

class HipaccPyramid {
protected:
  const int depth_;
  int level_;
  bool bound_;

public:
  HipaccPyramid(const int depth) : depth_(depth), level_(0), bound_(false) {}

  virtual int depth() const = 0;
  virtual int level() const = 0;
  virtual void levelInc() = 0;
  virtual void levelDec() = 0;
  virtual bool is_top_level() const = 0;
  virtual bool is_bottom_level() const = 0;
  virtual bool bind() = 0;
  virtual void unbind() = 0;
};

// Traversor for image pyramid applications
class HipaccPyramidTraversor {
private:
  std::vector<const std::function<void()> *> hipaccTraverseFunc;
  std::vector<std::vector<HipaccPyramid *>> hipaccPyramids;

public:
  void pushFunc(const std::function<void()> *f);
  void popFunc();
  const std::function<void()> getLastFunc();
  void pushPyramids(std::vector<HipaccPyramid *> &pyrs);
  void popPyramids();
  std::vector<HipaccPyramid *> getLastPyramids();
  bool hasPyramids();

public:
  // Interface for image pyramid applications: The traverse function is provided
  // to the user as a concise way to support recursive processing of image
  // pyramid.
  void hipaccTraverse(HipaccPyramid &p0, const std::function<void()> &func);
  void hipaccTraverse(HipaccPyramid &p0, HipaccPyramid &p1,
                      const std::function<void()> &func);
  void hipaccTraverse(HipaccPyramid &p0, HipaccPyramid &p1, HipaccPyramid &p2,
                      const std::function<void()> &func);
  void hipaccTraverse(HipaccPyramid &p0, HipaccPyramid &p1, HipaccPyramid &p2,
                      HipaccPyramid &p3, const std::function<void()> &func);
  void hipaccTraverse(HipaccPyramid &p0, HipaccPyramid &p1, HipaccPyramid &p2,
                      HipaccPyramid &p3, HipaccPyramid &p4,
                      const std::function<void()> &func);
  void hipaccTraverse(std::vector<HipaccPyramid *> pyrs,
                      const std::function<void()> &func);
  void hipaccTraverse(
      unsigned int loop = 1, const std::function<void()> &func = [] {});
};

#endif // __HIPACC_BASE_HPP__
