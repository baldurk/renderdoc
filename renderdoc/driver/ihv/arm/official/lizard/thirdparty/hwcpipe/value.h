/*
 * Copyright (c) 2019 ARM Limited.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

namespace hwcpipe
{
class Value
{
  public:
	Value() :
	    is_int_(true),
	    int_(0),
	    double_(0.0f)
	{}
	Value(long long value) :
	    is_int_(true),
	    int_(value)
	{}
	Value(double value) :
	    is_int_(false),
	    double_(value)
	{}

	template <typename T>
	T get() const
	{
		return is_int_ ? static_cast<T>(int_) : static_cast<T>(double_);
	}

	void set(long long value)
	{
		int_    = value;
		is_int_ = true;
	}

	void set(double value)
	{
		double_ = value;
		is_int_ = false;
	}

  private:
	bool      is_int_;
	long long int_{0};
	double    double_{0.0};
};
}        // namespace hwcpipe
