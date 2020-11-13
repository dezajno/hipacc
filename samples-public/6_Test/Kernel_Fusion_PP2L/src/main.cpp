//
// Copyright (c) 2020, University of Erlangen-Nuremberg
// Copyright (c) 2012, University of Erlangen-Nuremberg
// Copyright (c) 2012, Siemens AG
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "hipacc.hpp"

#include <iostream>
#include <hipacc_helper.hpp>

#define WIDTH 512
#define HEIGHT 512
#define SIZE_X 5
#define SIZE_Y 5
#define TYPE uchar

using namespace hipacc;
using namespace hipacc::math;

// Kernel description in Hipacc
class PointOperatorExample : public Kernel<TYPE> {
    private:
        Accessor<TYPE> &in;

    public:
        PointOperatorExample(IterationSpace<TYPE> &iter, Accessor<TYPE> &acc)
              : Kernel(iter), in(acc) {
            add_accessor(&in);
        }

        void kernel() {
            TYPE interm_pixel = in();
            interm_pixel += 3;
            output() = interm_pixel;
        }
};

class LocalOutOperatorExample : public Kernel<TYPE> {
  private:
    Accessor<TYPE> &in1;
    Accessor<TYPE> &in2;
    Mask<float> &mask;

  public:
    LocalOutOperatorExample(
        IterationSpace<TYPE> &iter,
        Accessor<TYPE> &acc1,
        Accessor<TYPE> &acc2,
        Mask<float> &mask)
          : Kernel(iter), in1(acc1), in2(acc2), mask(mask) {
        add_accessor(&in1);
        add_accessor(&in2);
    }

    void kernel() {
        output() = (TYPE)(convolve(mask, Reduce::SUM, [&] () -> float {
                return (mask() * in1(mask)) + (mask() * in2(mask));
            }) + 0.5f);
    }
};

// forward declaration of reference implementation
void kernel_fusion(TYPE *in, TYPE *out, float *filter,
                   int size_x, int size_y, int width, int height);

/*************************************************************************
 * Main function                                                         *
 *************************************************************************/
HIPACC_CODEGEN int main(int argc, const char **argv) {
    const int width = WIDTH;
    const int height = HEIGHT;
    const int size_x = SIZE_X;
    const int size_y = SIZE_Y;
    const int offset_x = size_x >> 1;
    const int offset_y = size_y >> 1;

    // convolution filter mask
    const float coef[SIZE_Y][SIZE_X] = {
        { 0.005008f, 0.017300f, 0.026151f, 0.017300f, 0.005008f },
        { 0.017300f, 0.059761f, 0.090339f, 0.059761f, 0.017300f },
        { 0.026151f, 0.090339f, 0.136565f, 0.090339f, 0.026151f },
        { 0.017300f, 0.059761f, 0.090339f, 0.059761f, 0.017300f },
        { 0.005008f, 0.017300f, 0.026151f, 0.017300f, 0.005008f }
    };

    // host memory for image of width x height pixels, random
    TYPE *input = (TYPE*)load_data<TYPE>(width, height);
    TYPE *ref_out = new TYPE[width*height];

    std::cout << "Testing Hipacc kernel fusion ..." << std::endl;

    //************************************************************************//

    // input and output image of width x height pixels
    Image<TYPE> in(width, height, input);
    Image<TYPE> out(width, height);

    // test point-point to local kernel fusion

    Accessor<TYPE> acc0(in);
    Image<TYPE> buf0(width, height);
    IterationSpace<TYPE> iter0(buf0);
    PointOperatorExample pointOp0(iter0, acc0);

    Accessor<TYPE> acc1(in);
    Image<TYPE> buf1(width, height);
    IterationSpace<TYPE> iter1(buf1);
    PointOperatorExample pointOp1(iter1, acc1);

    Mask<float> mask(coef);
    BoundaryCondition<TYPE> bound0(buf0, mask, Boundary::CLAMP);
    Accessor<TYPE> acc2(bound0);
    BoundaryCondition<TYPE> bound1(buf1, mask, Boundary::CLAMP);
    Accessor<TYPE> acc3(bound1);
    IterationSpace<TYPE> iter2(out);
    LocalOutOperatorExample localOutOp(iter2, acc2, acc3, mask);

    // execution after all decls
    pointOp0.execute();
    pointOp1.execute();
    localOutOp.execute();

    // get pointer to result data
    TYPE *output = out.data();

    //************************************************************************//
    std::cout << "Calculating reference ..." << std::endl;
    kernel_fusion(input, ref_out, (float*)coef, size_x, size_y, width, height);
    compare_results(output, ref_out, width, height, offset_x, offset_y);

    // free memory
    delete[] input;
    delete[] ref_out;

    return EXIT_SUCCESS;
}

// kernel fusion reference
void point_kernel(TYPE *in, TYPE *out, int width, int height) {
    for (int p = 0; p < width*height; ++p) {
        TYPE interm_pixel = in[p];
        interm_pixel += 3;
        out[p] = interm_pixel;
    }
}

void local_out_kernel(TYPE *in1, TYPE *in2, TYPE *out, float *filter,
                     int size_x, int size_y, int width, int height) {
    int anchor_x = size_x >> 1;
    int anchor_y = size_y >> 1;
    int upper_x = width  - anchor_x;
    int upper_y = height - anchor_y;

    for (int y=anchor_y; y<upper_y; ++y) {
        for (int x=anchor_x; x<upper_x; ++x) {
            float sum = 0.5f;
            for (int yf = -anchor_y; yf<=anchor_y; ++yf) {
                for (int xf = -anchor_x; xf<=anchor_x; ++xf) {
                    sum += 
                        filter[(yf+anchor_y)*size_x + xf+anchor_x]
                            * in1[(y+yf)*width + x + xf]
                        + filter[(yf+anchor_y)*size_x + xf+anchor_x]
                            * in2[(y+yf)*width + x + xf];
                }
            }
            out[y*width + x] = (TYPE) (sum);
        }
    }
}

void kernel_fusion(TYPE *in, TYPE *out, float *filter,
                   int size_x, int size_y, int width, int height) {
  TYPE *ref_buf0 = new TYPE[width*height];
  TYPE *ref_buf1 = new TYPE[width*height];

  point_kernel(in, ref_buf0, width, height);
  point_kernel(in, ref_buf1, width, height);
  local_out_kernel(ref_buf0, ref_buf1, out, filter, size_x, size_y, width, height);

  delete[] ref_buf0;
  delete[] ref_buf1;
}
