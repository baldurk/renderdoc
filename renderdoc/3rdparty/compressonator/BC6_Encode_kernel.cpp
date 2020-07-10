//=====================================================================
// Copyright (c) 2018    Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//=====================================================================
#include "BC6_Encode_kernel.h"

#if 0
void memset(CGU_UINT8 *srcdata, CGU_UINT8 value, CGU_INT size)
{
    for (CGU_INT i = 0; i < size; i++)
        *srcdata++ = value;
}

void memcpy(CGU_UINT8 *srcdata, CGU_UINT8 *dstdata, CGU_INT size)
{
    for (CGU_INT i = 0; i < size; i++)
    {
        *srcdata = *dstdata;
        srcdata++;
        dstdata++;
    }
}

void swap(CGU_INT A, CGU_INT B)
{
    CGU_INT hold = A;
    A = B;
    B = hold;
}

#define abs      fabs
#define floorf   floor
#define sqrtf    sqrt
#define logf     log
#define ceilf    ceil

#endif

__constant CGU_UINT8   BC6_PARTITIONS[MAX_BC6H_PARTITIONS][MAX_SUBSET_SIZE] = {
   { // 0
       0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1
   },

   { // 1
       0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1
   },

   { // 2
       0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1
   },

   { // 3
       0,0,0,1,0,0,1,1,0,0,1,1,0,1,1,1
   },

   { // 4
       0,0,0,0,0,0,0,1,0,0,0,1,0,0,1,1
   },

   { // 5
       0,0,1,1,0,1,1,1, 0,1,1,1,1,1,1,1
   },

   { // 6
       0,0,0,1,0,0,1,1,0,1,1,1,1,1,1,1
   },

   { // 7
       0,0,0,0,0,0,0,1,0,0,1,1,0,1,1,1
   },

   { // 8
       0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,1
   },

   { // 9
       0,0,1,1,0,1,1,1,1,1,1,1,1,1,1,1
   },

   { // 10
       0,0,0,0,0,0,0,1,0,1,1,1,1,1,1,1
   },

   { // 11
       0,0,0,0,0,0,0,0,0,0,0,1,0,1,1,1
   },

   { // 12
       0,0,0,1,0,1,1,1,1,1,1,1,1,1,1,1
   },

   { // 13
       0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1
   },

   { // 14
       0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1
   },

   { // 15
       0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1
   },

   { // 16
       0,0,0,0,1,0,0,0,1,1,1,0,1,1,1,1
   },

   { // 17
       0,1,1,1,0,0,0,1,0,0,0,0,0,0,0,0
   },

   { // 18
       0,0,0,0,0,0,0,0,1,0,0,0,1,1,1,0
   },

   { // 19
       0,1,1,1,0,0,1,1,0,0,0,1,0,0,0,0
   },

   { // 20
       0,0,1,1,0,0,0,1,0,0,0,0,0,0,0,0
   },

   { // 21
       0,0,0,0,1,0,0,0,1,1,0,0,1,1,1,0
   },

   { // 22
       0,0,0,0,0,0,0,0,1,0,0,0,1,1,0,0
   },

   { // 23
       0,1,1,1,0,0,1,1,0,0,1,1,0,0,0,1
   },

   { // 24
       0,0,1,1,0,0,0,1,0,0,0,1,0,0,0,0
   },

   { // 25
       0,0,0,0,1,0,0,0,1,0,0,0,1,1,0,0
   },

   { // 26
       0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0
   },

   { // 27
       0,0,1,1,0,1,1,0,0,1,1,0,1,1,0,0
   },

   { // 28
       0,0,0,1,0,1,1,1,1,1,1,0,1,0,0,0
   },

   { // 29
       0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0
   },

   { // 30
       0,1,1,1,0,0,0,1,1,0,0,0,1,1,1,0
   },

   { // 31
       0,0,1,1,1,0,0,1,1,0,0,1,1,1,0,0
   },
};

CGU_DWORD get_partition_subset(CGU_INT subset, CGU_INT partI, CGU_INT index)
{
    if (subset)
        return BC6_PARTITIONS[partI][index];
    else
        return 0;
}

void    Partition(CGU_INT      shape,
                  CGU_FLOAT    in[][MAX_DIMENSION_BIG],
                  CGU_FLOAT    subsets[MAX_SUBSETS][MAX_SUBSET_SIZE][MAX_DIMENSION_BIG], //[3][16][4]
                  CGU_INT      count[MAX_SUBSETS],
                  CGU_INT8     ShapeTableToUse,
                  CGU_INT      dimension)
{
    int   i, j;
    int   insubset = -1, inpart = 0;

    // Dont use memset: this is better for now
    for (i = 0; i < MAX_SUBSETS; i++) count[i] = 0;

    switch (ShapeTableToUse)
    {
    case    0:
    case    1:
        insubset = 0;
        inpart = 0;
        break;
    case    2:
        insubset = 1;
        inpart = shape;
        break;
    default:
        break;
    }

    // Nothing to do!!: Must indicate an error to user
    if (insubset == -1) return; // Nothing to do!!

    for (i = 0; i < MAX_SUBSET_SIZE; i++)
    {
        int   subset = get_partition_subset(insubset, inpart, i);
        for (j = 0; j < dimension; j++)
        {
            subsets[subset][count[subset]][j] = in[i][j];
        }
        if (dimension < MAX_DIMENSION_BIG)
        {
            subsets[subset][count[subset]][j] = 0.0;
        }
        count[subset]++;
    }

}

void GetEndPoints(CGU_FLOAT EndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], CGU_FLOAT outB[MAX_SUBSETS][MAX_SUBSET_SIZE][MAX_DIMENSION_BIG], CGU_INT max_subsets, int entryCount[MAX_SUBSETS])
{
    // Should have some sort of error notification!
    if (max_subsets > MAX_SUBSETS) return;

    // Save Min and Max OutB points as EndPoints
    for (int subset = 0; subset < max_subsets; subset++)
    {
        // We now have points on direction vector(s) 
        // find the min and max points
        CGU_FLOAT min = CMP_HALF_MAX;
        CGU_FLOAT max = 0;
        CGU_FLOAT val;
        int mini = 0;
        int maxi = 0;


        for (int i = 0; i < entryCount[subset]; i++)
        {
            val = outB[subset][i][0] + outB[subset][i][1] + outB[subset][i][2];
            if (val < min)
            {
                min = val;
                mini = i;
            }
            if (val > max)
            {
                max = val;
                maxi = i;
            }
        }

        // Is round best for this !
        for (int c = 0; c < MAX_DIMENSION_BIG; c++)
        {
            EndPoints[subset][0][c] = outB[subset][mini][c];
        }

        for (int c = 0; c < MAX_DIMENSION_BIG; c++)
        {
            EndPoints[subset][1][c] = outB[subset][maxi][c];
        }
    }
}

void covariance_d(CGU_FLOAT data[][MAX_DIMENSION_BIG], CGU_INT numEntries, CGU_FLOAT cov[MAX_DIMENSION_BIG][MAX_DIMENSION_BIG], CGU_INT dimension)
{
#ifdef USE_DBGTRACE
    DbgTrace(());
#endif
    int i, j, k;

    for (i = 0; i < dimension; i++)
        for (j = 0; j <= i; j++)
        {
            cov[i][j] = 0;
            for (k = 0; k < numEntries; k++)
                cov[i][j] += data[k][i] * data[k][j];
        }

    for (i = 0; i < dimension; i++)
        for (j = i + 1; j < dimension; j++)
            cov[i][j] = cov[j][i];
}

void centerInPlace_d(CGU_FLOAT data[][MAX_DIMENSION_BIG], int numEntries, CGU_FLOAT mean[MAX_DIMENSION_BIG], CGU_INT dimension)
{
#ifdef USE_DBGTRACE
    DbgTrace(());
#endif
    int i, k;

    for (i = 0; i < dimension; i++)
    {
        mean[i] = 0;
        for (k = 0; k < numEntries; k++)
            mean[i] += data[k][i];
    }

    if (!numEntries)
        return;

    for (i = 0; i < dimension; i++)
    {
        mean[i] /= numEntries;
        for (k = 0; k < numEntries; k++)
            data[k][i] -= mean[i];
    }
}

void eigenVector_d(CGU_FLOAT cov[MAX_DIMENSION_BIG][MAX_DIMENSION_BIG], CGU_FLOAT vector[MAX_DIMENSION_BIG], CGU_INT dimension)
{
#ifdef USE_DBGTRACE
    DbgTrace(());
#endif
    // calculate an eigenvecto corresponding to a biggest eigenvalue
    // will work for non-zero non-negative matricies only

#define EV_ITERATION_NUMBER 20
#define EV_SLACK            2        /* additive for exp base 2)*/    


    CGU_INT i, j, k, l, m, n, p, q;
    CGU_FLOAT c[2][MAX_DIMENSION_BIG][MAX_DIMENSION_BIG];
    CGU_FLOAT maxDiag;

    for (i = 0; i < dimension; i++)
        for (j = 0; j < dimension; j++)
            c[0][i][j] = cov[i][j];

    p = (int)floorf(static_cast<float>(log((FLT_MAX_EXP - EV_SLACK) / ceilf(logf((CGU_FLOAT)dimension) / logf(2.0f))) / logf(2.0f)));

    //assert(p>0);

    p = p > 0 ? p : 1;

    q = (EV_ITERATION_NUMBER + p - 1) / p;

    l = 0;

    for (n = 0; n < q; n++)
    {
        maxDiag = 0;

        for (i = 0; i < dimension; i++)
            maxDiag = c[l][i][i] > maxDiag ? c[l][i][i] : maxDiag;

        if (maxDiag <= 0)
        {
            return;
        }

        //assert(maxDiag >0);

        for (i = 0; i < dimension; i++)
            for (j = 0; j < dimension; j++)
                c[l][i][j] /= maxDiag;

        for (m = 0; m < p; m++) {
            for (i = 0; i < dimension; i++)
                for (j = 0; j < dimension; j++) {
                    CGU_FLOAT temp = 0;
                    for (k = 0; k < dimension; k++)
                    {
                        // Notes: 
                        // This is the most consuming portion of the code and needs optimizing for perfromance
                        temp += c[l][i][k] * c[l][k][j];
                    }
                    c[1 - l][i][j] = temp;
                }
            l = 1 - l;
        }
    }

    maxDiag = 0;
    k = 0;

    for (i = 0; i < dimension; i++)
    {
        k = c[l][i][i] > maxDiag ? i : k;
        maxDiag = c[l][i][i] > maxDiag ? c[l][i][i] : maxDiag;
    }
    CGU_FLOAT t;
    t = 0;
    for (i = 0; i < dimension; i++)
    {
        t += c[l][k][i] * c[l][k][i];
        vector[i] = c[l][k][i];
    }
    // normalization is really optional
    t = sqrtf(t);
    //assert(t>0);

    if (t <= 0)
    {
        return;
    }
    for (i = 0; i < dimension; i++)
        vector[i] /= t;
}

void project_d(CGU_FLOAT data[][MAX_DIMENSION_BIG], CGU_INT numEntries, CGU_FLOAT vector[MAX_DIMENSION_BIG], CGU_FLOAT projection[MAX_ENTRIES], CGU_INT dimension)
{
#ifdef USE_DBGTRACE
    DbgTrace(());
#endif
    // assume that vector is normalized already
    int i, k;

    for (k = 0; k < numEntries; k++)
    {
        projection[k] = 0;
        for (i = 0; i < dimension; i++)
        {
            projection[k] += data[k][i] * vector[i];
        }
    }
}

typedef struct {
    CGU_FLOAT d;
    int i;
} a;

inline CGU_INT a_compare(const void *arg1, const void *arg2)
{
    if (((a*)arg1)->d - ((a*)arg2)->d > 0) return 1;
    if (((a*)arg1)->d - ((a*)arg2)->d < 0) return -1;
    return 0;
};

void sortProjection(CGU_FLOAT projection[MAX_ENTRIES], CGU_INT order[MAX_ENTRIES], CGU_INT numEntries)
{
    int i;
    a what[MAX_ENTRIES + MAX_PARTITIONS_TABLE];

    for (i = 0; i < numEntries; i++)
        what[what[i].i = i].d = projection[i];

#ifdef USE_QSORT
    qsort((void*)&what, numEntries, sizeof(a), a_compare);
#else
    {
        int j;
        int tmp;
        CGU_FLOAT tmp_d;
        for (i = 1; i < numEntries; i++)
        {
            for (j = i; j > 0; j--)
            {
                if (what[j - 1].d > what[j].d)
                {
                    tmp = what[j].i;
                    tmp_d = what[j].d;
                    what[j].i = what[j - 1].i;
                    what[j].d = what[j - 1].d;
                    what[j - 1].i = tmp;
                    what[j - 1].d = tmp_d;
                }
            }
        }
    }
#endif


    for (i = 0; i < numEntries; i++)
        order[i] = what[i].i;
};

CGU_FLOAT totalError_d(CGU_FLOAT data[MAX_ENTRIES][MAX_DIMENSION_BIG], CGU_FLOAT data2[MAX_ENTRIES][MAX_DIMENSION_BIG], CGU_INT numEntries, CGU_INT dimension)
{
    int i, j;
    CGU_FLOAT t = 0;
    for (i = 0; i < numEntries; i++)
        for (j = 0; j < dimension; j++)
            t += (data[i][j] - data2[i][j])*(data[i][j] - data2[i][j]);

    return t;
};

// input:
//
// v_  points, might be uncentered
// k - number of points in the ramp
// n - number of points in v_
//
// output:
// index, uncentered, in the range 0..k-1
//

void quant_AnD_Shell(CGU_FLOAT* v_, CGU_INT k, CGU_INT n, CGU_INT *idx)
{
#define MAX_BLOCK MAX_ENTRIES
    CGU_INT i, j;
    CGU_FLOAT v[MAX_BLOCK];
    CGU_FLOAT z[MAX_BLOCK];
    a d[MAX_BLOCK];
    CGU_FLOAT l;
    CGU_FLOAT mm;
    CGU_FLOAT r = 0;
    CGU_INT mi;

    CGU_FLOAT m, M, s, dm = 0.;
    m = M = v_[0];

    for (i = 1; i < n; i++) {
        m = m < v_[i] ? m : v_[i];
        M = M > v_[i] ? M : v_[i];
    }
    if (M == m) {
        for (i = 0; i < n; i++)
            idx[i] = 0;
        return;
    }

    //assert(M - m >0);
    s = (k - 1) / (M - m);
    for (i = 0; i < n; i++) {
        v[i] = v_[i] * s;

        idx[i] = (int)(z[i] = (v[i] + 0.5f /* stabilizer*/ - m * s));  //floorf(v[i] + 0.5f /* stabilizer*/ - m *s));

        d[i].d = v[i] - z[i] - m * s;
        d[i].i = i;
        dm += d[i].d;
        r += d[i].d*d[i].d;
    }
    if (n*r - dm * dm >= (CGU_FLOAT)(n - 1) / 4 /*slack*/ / 2) {

        dm /= (CGU_FLOAT)n;

        for (i = 0; i < n; i++)
            d[i].d -= dm;


        //!!! Need an OpenCL version of qsort
#ifdef USE_QSORT
        qsort((void*)&d, n, sizeof(a), a_compare);
#else
        {
            CGU_INT tmp;
            CGU_FLOAT tmp_d;
            for (i = 1; i < n; i++) {
                for (j = i; j > 0; j--)
                {
                    if (d[j - 1].d > d[j].d)
                    {
                        tmp = d[j].i;
                        tmp_d = d[j].d;
                        d[j].i = d[j - 1].i;
                        d[j].d = d[j - 1].d;
                        d[j - 1].i = tmp;
                        d[j - 1].d = tmp_d;
                    }
                }
            }
        }
#endif
        // got into fundamental simplex
        // move coordinate system origin to its center
        for (i = 0; i < n; i++)
            d[i].d -= (2.0f*(CGU_FLOAT)i + 1.0f - (CGU_FLOAT)n) / 2.0f / (CGU_FLOAT)n;

        mm = l = 0.;
        j = -1;
        for (i = 0; i < n; i++) {
            l += d[i].d;
            if (l < mm) {
                mm = l;
                j = i;
            }
        }

        // position which should be in 0 
        j = j + 1;
        j = j % n;

        for (i = j; i < n; i++)
            idx[d[i].i]++;
    }
    // get rid of an offset in idx
    mi = idx[0];
    for (i = 1; i < n; i++)
        mi = mi < idx[i] ? mi : idx[i];

    for (i = 0; i < n; i++)
        idx[i] -= mi;
}

CGU_FLOAT optQuantAnD_d(CGU_FLOAT data[MAX_ENTRIES][MAX_DIMENSION_BIG],
                        CGU_INT numEntries,
                        CGU_INT numClusters,
                        CGU_INT index[MAX_ENTRIES],
                        CGU_FLOAT out[MAX_ENTRIES][MAX_DIMENSION_BIG],
                        CGU_FLOAT direction[MAX_DIMENSION_BIG], CGU_FLOAT *step,
                        CGU_INT dimension,
                        CGU_FLOAT quality)
{
    CGU_INT index_[MAX_ENTRIES];

    CGU_INT maxTry = (int)(MAX_TRY * quality);
    CGU_INT try_two = 50;

    CGU_INT i, j, k;
    CGU_FLOAT t, s;

    CGU_FLOAT centered[MAX_ENTRIES][MAX_DIMENSION_BIG];

    CGU_FLOAT mean[MAX_DIMENSION_BIG];

    CGU_FLOAT cov[MAX_DIMENSION_BIG][MAX_DIMENSION_BIG];

    CGU_FLOAT projected[MAX_ENTRIES];

    CGU_INT order_[MAX_ENTRIES];


    for (i = 0; i < numEntries; i++)
        for (j = 0; j < dimension; j++)
            centered[i][j] = data[i][j];

    centerInPlace_d(centered, numEntries, mean, dimension);
    covariance_d(centered, numEntries, cov, dimension);

    // check if they all are the same 

    t = 0;
    for (j = 0; j < dimension; j++)
        t += cov[j][j];

    if (numEntries == 0) {
        for (i = 0; i < numEntries; i++) {
            index[i] = 0;
            for (j = 0; j < dimension; j++)
                out[i][j] = mean[j];
        }
        return 0.0f;
    }

    eigenVector_d(cov, direction, dimension);
    project_d(centered, numEntries, direction, projected, dimension);

    for (i = 0; i < maxTry; i++)
    {
        CGU_INT done = 0;

        if (i)
        {
            do
            {
                CGU_FLOAT q;
                q = s = t = 0;

                for (k = 0; k < numEntries; k++)
                {
                    s += index[k];
                    t += index[k] * index[k];
                }

                for (j = 0; j < dimension; j++)
                {
                    direction[j] = 0;
                    for (k = 0; k < numEntries; k++)
                        direction[j] += centered[k][j] * index[k];
                    q += direction[j] * direction[j];

                }

                s /= (CGU_FLOAT)numEntries;
                t = t - s * s * (CGU_FLOAT)numEntries;
                //assert(t != 0);
                t = (t == 0.0f ? 0.0f : 1.0f / t);
                // We need to requantize 

                q = sqrtf(q);
                t *= q;

                if (q != 0)
                    for (j = 0; j < dimension; j++)
                        direction[j] /= q;

                // direction normalized

                project_d(centered, numEntries, direction, projected, dimension);
                sortProjection(projected, order_, numEntries);

                CGU_INT index__[MAX_ENTRIES];

                // it's projected and centered; cluster centers are (index[i]-s)*t (*dir)
                k = 0;
                for (j = 0; j < numEntries; j++)
                {
                    while (projected[order_[j]] > (k + 0.5 - s)*t  && k < numClusters - 1)
                        k++;
                    index__[order_[j]] = k;
                }
                done = 1;
                for (j = 0; j < numEntries; j++)
                {
                    done = (done && (index__[j] == index[j]));
                    index[j] = index__[j];
                }
            } while (!done && try_two--);

            if (i == 1)
                for (j = 0; j < numEntries; j++)
                    index_[j] = index[j];
            else
            {
                done = 1;
                for (j = 0; j < numEntries; j++)
                {
                    done = (done && (index_[j] == index[j]));
                    index_[j] = index_[j];
                }
                if (done)
                    break;

            }
        }

        quant_AnD_Shell(projected, numClusters, numEntries, index);
    }
    s = t = 0;

    CGU_FLOAT q = 0;

    for (k = 0; k < numEntries; k++)
    {
        s += index[k];
        t += index[k] * index[k];
    }

    for (j = 0; j < dimension; j++)
    {
        direction[j] = 0;
        for (k = 0; k < numEntries; k++)
            direction[j] += centered[k][j] * index[k];
        q += direction[j] * direction[j];
    }

    s /= (CGU_FLOAT)numEntries;

    t = t - s * s * (CGU_FLOAT)numEntries;

    //assert(t != 0);

    t = (t == 0.0 ? 0.0f : 1.0f / t);

    for (i = 0; i < numEntries; i++)
        for (j = 0; j < dimension; j++)
            out[i][j] = mean[j] + direction[j] * t*(index[i] - s);

    // normalize direction for output

    q = sqrtf(q);
    *step = t * q;
    for (j = 0; j < dimension; j++)
        direction[j] /= q;

    return totalError_d(data, out, numEntries, dimension);
}

void clampF16Max(CGU_FLOAT EndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], CGU_BOOL isSigned)
{
    for (CGU_INT region = 0; region < 2; region++)
        for (CGU_INT ab = 0; ab < 2; ab++)
            for (CGU_INT rgb = 0; rgb < 3; rgb++)
            {
                if (isSigned)
                {
                    if (EndPoints[region][ab][rgb] < -FLT16_MAX) EndPoints[region][ab][rgb] = -FLT16_MAX;
                    else if (EndPoints[region][ab][rgb] > FLT16_MAX) EndPoints[region][ab][rgb] = FLT16_MAX;
                }
                else
                {
                    if (EndPoints[region][ab][rgb] < 0.0) EndPoints[region][ab][rgb] = 0.0;
                    else if (EndPoints[region][ab][rgb] > FLT16_MAX) EndPoints[region][ab][rgb] = FLT16_MAX;
                }
                // Zero region
                // if ((EndPoints[region][ab][rgb] > -0.01) && ((EndPoints[region][ab][rgb] < 0.01))) EndPoints[region][ab][rgb] = 0.0;
            }
}

//=====================================================================================================================
#define LOG_CL_BASE         2
#define BIT_BASE            5
#define LOG_CL_RANGE        5
#define BIT_RANGE           9
#define MAX_CLUSTERS_BIG    16
#ifndef BTT
#define BTT(bits)           (bits-BIT_BASE)
#endif
#ifndef CLT
#define CLT(cl)             (cl-LOG_CL_BASE)
#endif

#ifdef USE_BC6RAMPS

int spidx(int in_data, int in_clogs, int in_bits, int in_p2, int in_o1, int in_o2, int in_i)
{
    // use BC7 sp_idx
    return 0;
}

float sperr(int in_data, int clogs, int bits, int p2, int o1, int o2)
{
     // use BC7 sp_err
    return 0,0f;
}
#endif

__constant CGU_FLOAT rampLerpWeightsBC6[5][16] =
{
    { 0.0 }, // 0 bit index
    { 0.0, 1.0 }, // 1 bit index
    { 0.0, 21.0 / 64.0, 43.0 / 64.0, 1.0 }, // 2 bit index
    { 0.0, 9.0 / 64.0, 18.0 / 64.0, 27.0 / 64.0, 37.0 / 64.0, 46.0 / 64.0, 55.0 / 64.0, 1.0 }, // 3 bit index
    { 0.0, 4.0 / 64.0, 9.0 / 64.0, 13.0 / 64.0, 17.0 / 64.0, 21.0 / 64.0, 26.0 / 64.0, 30.0 / 64.0,
    34.0 / 64.0, 38.0 / 64.0, 43.0 / 64.0, 47.0 / 64.0, 51.0 / 64.0, 55.0 / 64.0, 60.0 / 64.0, 1.0 } // 4 bit index
};


CGU_FLOAT rampf(CGU_INT clogs, CGU_FLOAT p1, CGU_FLOAT p2, CGU_INT indexPos)
{
    // (clogs+ LOG_CL_BASE) starts from 2 to 4
    return  (CGU_FLOAT)p1 + rampLerpWeightsBC6[clogs + LOG_CL_BASE][indexPos] * (p2 - p1);
}

CGU_INT all_same_d(CGU_FLOAT d[][MAX_DIMENSION_BIG], CGU_INT n, CGU_INT dimension)
{
    CGU_INT i, j;
    CGU_INT same = 1;
    for (i = 1; i < n; i++)
        for (j = 0; j < dimension; j++)
            same = same && (d[0][j] == d[i][j]);

    return(same);
}

// return the max index from a set of indexes
CGU_INT max_index(CGU_INT a[], CGU_INT n)
{
    CGU_INT i, m = a[0];
    for (i = 0; i < n; i++)
        m = m > a[i] ? m : a[i];
    return (m);
}

CGU_INT cluster_mean_d_d(CGU_FLOAT d[MAX_ENTRIES][MAX_DIMENSION_BIG], CGU_FLOAT mean[MAX_ENTRIES][MAX_DIMENSION_BIG], CGU_INT index[], CGU_INT i_comp[], CGU_INT i_cnt[], CGU_INT n, CGU_INT dimension)
{
    // unused index values are underfined
    CGU_INT i, j, k;
    //assert(n!=0);

    for (i = 0; i < n; i++)
        for (j = 0; j < dimension; j++) {
            // assert(index[i]<MAX_CLUSTERS_BIG);
            mean[index[i]][j] = 0;
            i_cnt[index[i]] = 0;
        }
    k = 0;
    for (i = 0; i < n; i++) {
        for (j = 0; j < dimension; j++)
            mean[index[i]][j] += d[i][j];
        if (i_cnt[index[i]] == 0)
            i_comp[k++] = index[i];
        i_cnt[index[i]]++;
    }

    for (i = 0; i < k; i++)
        for (j = 0; j < dimension; j++)
            mean[i_comp[i]][j] /= (CGU_FLOAT)i_cnt[i_comp[i]];
    return k;
}

void mean_d_d(CGU_FLOAT d[][MAX_DIMENSION_BIG], CGU_FLOAT mean[MAX_DIMENSION_BIG], CGU_INT n, CGU_INT dimension)
{
    CGU_INT i, j;
    for (j = 0; j < dimension; j++)
        mean[j] = 0;
    for (i = 0; i < n; i++)
        for (j = 0; j < dimension; j++)
            mean[j] += d[i][j];
    for (j = 0; j < dimension; j++)
        mean[j] /= (CGU_FLOAT)n;
}

void index_collapse_kernel(CGU_INT index[], CGU_INT numEntries)
{
    CGU_INT k;
    CGU_INT d, D;
    CGU_INT mi;
    CGU_INT Mi;
    if (numEntries == 0)
        return;

    mi = Mi = index[0];
    for (k = 1; k < numEntries; k++) {
        mi = mi < index[k] ? mi : index[k];
        Mi = Mi > index[k] ? Mi : index[k];
    }
    D = 1;
    for (d = 2; d <= Mi - mi; d++) {

        for (k = 0; k < numEntries; k++)
            if ((index[k] - mi) % d != 0)
                break;
        if (k >= numEntries)
            D = d;
    }
    for (k = 0; k < numEntries; k++)
        index[k] = (index[k] - mi) / D;
}

CGU_INT max_int(CGU_INT a[], CGU_INT n)
{
    CGU_INT i, m = a[0];
    for (i = 0; i < n; i++)
        m = m > a[i] ? m : a[i];
    return (m);
}

__constant CGU_INT npv_nd[2][2 * MAX_DIMENSION_BIG] =
{
    { 1,2,4,8,16,32,0,0 }, //dimension = 3
    { 1,2,4,0,0,0,0,0 }    //dimension = 4
};

__constant short par_vectors_nd[2][8][128][2][MAX_DIMENSION_BIG] =
{
    { // Dimension = 3
        {
            { { 0,0,0,0 },{ 0,0,0,0 } },
            { { 0,0,0,0 },{ 0,0,0,0 } }
        },

    // 3*n+1    BCC          3*n+1        Cartesian 3*n            //same parity
        { // SAME_PAR
            { { 0,0,0 },{ 0,0,0 } },
            { { 1,1,1 },{ 1,1,1 } }
        },
    // 3*n+2    BCC          3*n+1        BCC          3*n+1    
        { // BCC
            { { 0,0,0 },{ 0,0,0 } },
            { { 0,0,0 },{ 1,1,1 } },
            { { 1,1,1 },{ 0,0,0 } },
            { { 1,1,1 },{ 1,1,1 } }
        },
    // 3*n+3    FCC                    ???                        // ??????
    // BCC with FCC same or inverted, symmetric
        { // BCC_SAME_FCC
            { { 0,0,0 },{ 0,0,0 } },
            { { 1,1,0 },{ 1,1,0 } },
            { { 1,0,1 },{ 1,0,1 } },
            { { 0,1,1 },{ 0,1,1 } },

            { { 0,0,0 },{ 1,1,1 } },
            { { 1,1,1 },{ 0,0,0 } },
            { { 0,1,0 },{ 0,1,0 } },  // ??
            { { 1,1,1 },{ 1,1,1 } },

        },
        // 3*n+4    FCC          3*n+2        FCC          3*n+2
        {

            { { 0,0,0 },{ 0,0,0 } },
            { { 1,1,0 },{ 0,0,0 } },
            { { 1,0,1 },{ 0,0,0 } },
            { { 0,1,1 },{ 0,0,0 } },

            { { 0,0,0 },{ 1,1,0 } },
            { { 1,1,0 },{ 1,1,0 } },
            { { 1,0,1 },{ 1,1,0 } },
            { { 0,1,1 },{ 1,1,0 } },

            { { 0,0,0 },{ 1,0,1 } },
            { { 1,1,0 },{ 1,0,1 } },
            { { 1,0,1 },{ 1,0,1 } },
            { { 0,1,1 },{ 1,0,1 } },

            { { 0,0,0 },{ 0,1,1 } },
            { { 1,1,0 },{ 0,1,1 } },
            { { 1,0,1 },{ 0,1,1 } },
            { { 0,1,1 },{ 0,1,1 } }
        },


    // 3*n+5    Cartesian 3*n+3        FCC          3*n+2            //D^*[6]  
        {

            { { 0,0,0 },{ 0,0,0 } },
            { { 1,1,0 },{ 0,0,0 } },
            { { 1,0,1 },{ 0,0,0 } },
            { { 0,1,1 },{ 0,0,0 } },

            { { 0,0,0 },{ 1,1,0 } },
            { { 1,1,0 },{ 1,1,0 } },
            { { 1,0,1 },{ 1,1,0 } },
            { { 0,1,1 },{ 1,1,0 } },

            { { 0,0,0 },{ 1,0,1 } },
            { { 1,1,0 },{ 1,0,1 } },
            { { 1,0,1 },{ 1,0,1 } },
            { { 0,1,1 },{ 1,0,1 } },

            { { 0,0,0 },{ 0,1,1 } },
            { { 1,1,0 },{ 0,1,1 } },
            { { 1,0,1 },{ 0,1,1 } },
            { { 0,1,1 },{ 0,1,1 } },


            { { 1,0,0 },{ 1,1,1 } },
            { { 0,1,0 },{ 1,1,1 } },
            { { 0,0,1 },{ 1,1,1 } },
            { { 1,1,1 },{ 1,1,1 } },

            { { 1,0,0 },{ 0,0,1 } },
            { { 0,1,0 },{ 0,0,1 } },
            { { 0,0,1 },{ 0,0,1 } },
            { { 1,1,1 },{ 0,0,1 } },

            { { 1,0,0 },{ 1,0,0 } },
            { { 0,1,0 },{ 1,0,0 } },
            { { 0,0,1 },{ 1,0,0 } },
            { { 1,1,1 },{ 1,0,0 } },

            { { 1,0,0 },{ 0,1,0 } },
            { { 0,1,0 },{ 0,1,0 } },
            { { 0,0,1 },{ 0,1,0 } },
            { { 1,1,1 },{ 0,1,0 } }
        }
    },// Dimension = 3
    { // Dimension = 4
        {
            { { 0,0,0,0 },{ 0,0,0,0 } },
            { { 0,0,0,0 },{ 0,0,0,0 } }
        },

    // 3*n+1    BCC          3*n+1        Cartesian 3*n            //same parity
        { // SAME_PAR
            { { 0,0,0,0 },{ 0,0,0,0 } },
            { { 1,1,1,1 },{ 1,1,1,1 } }
        },
    // 3*n+2    BCC          3*n+1        BCC          3*n+1    
        { // BCC
            { { 0,0,0,0 },{ 0,0,0,0 } },
            { { 0,0,0,0 },{ 1,1,1,1 } },
            { { 1,1,1,1 },{ 0,0,0,0 } },
            { { 1,1,1,1 },{ 1,1,1,1 } }
        },
    // 3 PBIT
        {
            { { 0,0,0,0 },{ 0,0,0,0 } },
            { { 0,0,0,0 },{ 0,1,1,1 } },
            { { 0,1,1,1 },{ 0,0,0,0 } },
            { { 0,1,1,1 },{ 0,1,1,1 } },

            { { 1,0,0,0 },{ 1,0,0,0 } },
            { { 1,0,0,0 },{ 1,1,1,1 } },
            { { 1,1,1,1 },{ 1,0,0,0 } },
            { { 1,1,1,1 },{ 1,1,1,1 } }
        },

    // 4 PBIT
        {
            { { 0,0,0,0 },{ 0,0,0,0 } },
            { { 0,0,0,0 },{ 0,1,1,1 } },
            { { 0,1,1,1 },{ 0,0,0,0 } },
            { { 0,1,1,1 },{ 0,1,1,1 } },

            { { 1,0,0,0 },{ 1,0,0,0 } },
            { { 1,0,0,0 },{ 1,1,1,1 } },
            { { 1,1,1,1 },{ 1,0,0,0 } },
            { { 1,1,1,1 },{ 1,1,1,1 } },

            { { 0,0,0,0 },{ 0,0,0,0 } },
            { { 0,0,0,0 },{ 0,0,1,1 } },
            { { 0,0,1,1 },{ 0,0,0,0 } },
            { { 0,1,0,1 },{ 0,1,0,1 } },

            { { 1,0,0,0 },{ 1,0,0,0 } },
            { { 1,0,0,0 },{ 1,0,1,1 } },
            { { 1,0,1,1 },{ 1,0,0,0 } },
            { { 1,1,0,1 },{ 1,1,0,1 } },

        },

    } // Dimension = 4

};

CGU_INT get_par_vector(CGU_INT dim1, CGU_INT dim2, CGU_INT dim3, CGU_INT dim4, CGU_INT dim5)
{
    return par_vectors_nd[dim1][dim2][dim3][dim4][dim5];
}

CGU_FLOAT quant_single_point_d(CGU_FLOAT data[MAX_ENTRIES][MAX_DIMENSION_BIG],
                               CGU_INT numEntries, CGU_INT index[MAX_ENTRIES],
                               CGU_FLOAT out[MAX_ENTRIES][MAX_DIMENSION_BIG],
                               CGU_INT epo_1[2][MAX_DIMENSION_BIG],
                               CGU_INT Mi_,                // last cluster
                               CGU_INT type,
                               CGU_INT dimension)
{
    if (dimension < 3) return CMP_FLOAT_MAX;

    CGU_INT i, j;

    CGU_FLOAT err_0 = CMP_FLOAT_MAX;
    CGU_FLOAT err_1 = CMP_FLOAT_MAX;

    CGU_INT idx = 0;
    CGU_INT idx_1 = 0;

    CGU_INT epo_0[2][MAX_DIMENSION_BIG];

    CGU_INT use_par = (type != 0);

    CGU_INT clogs = 0;
    i = Mi_ + 1;
    while (i >>= 1)
        clogs++;

    //    assert((1<<clogs)== Mi_+1);

    CGU_INT pn;
    for (pn = 0; pn < npv_nd[dimension - 3][type]; pn++)
    { //1

        CGU_INT dim1 = dimension - 3;
        CGU_INT dim2 = type;
        CGU_INT dim3 = pn;


        CGU_INT o1[2][MAX_DIMENSION_BIG]; // = { 0,2 };
        CGU_INT o2[2][MAX_DIMENSION_BIG]; // = { 0,2 };

        for (j = 0; j < dimension; j++)
        { //A
            o2[0][j] = o1[0][j] = 0;
            o2[1][j] = o1[1][j] = 2;

            if (use_par)
            {
                if (get_par_vector(dim1, dim2, dim3, 0, j))
                    o1[0][j] = 1;
                else
                    o1[1][j] = 1;
                if (get_par_vector(dim1, dim2, dim3, 1, j))
                    o2[0][j] = 1;
                else
                    o2[1][j] = 1;
            }
        } //A

        CGU_INT t1, t2;

        CGU_INT dr[MAX_DIMENSION_BIG];
        CGU_INT dr_0[MAX_DIMENSION_BIG];
        //CGU_FLOAT tr;

        for (i = 0; i < (1 << clogs); i++)
        { //E
            CGU_FLOAT t = 0;
            CGU_INT t1o[MAX_DIMENSION_BIG], t2o[MAX_DIMENSION_BIG];

            for (j = 0; j < dimension; j++)
            { // D
                CGU_FLOAT t_ = CMP_FLOAT_MAX;

                for (t1 = o1[0][j]; t1 < o1[1][j]; t1++)
                { // C
                    for (t2 = o2[0][j]; t2 < o2[1][j]; t2++)
                        // This is needed for non-integer mean points of "collapsed" sets
                    { // B

#ifdef USE_BC6RAMPS
                        CGU_INT tf = (int)floorf(data[0][j]);
                        CGU_INT tc = (int)ceilf(data[0][j]);
                        // if they are not equal, the same representalbe point is used for 
                        // both of them, as all representable points are integers in the rage 
                        if (sperr(tf, CLT(clogs), BTT(bits[j]), t1, t2, i) > sperr(tc, CLT(clogs), BTT(bits[j]), t1, t2, i))
                            dr[j] = tc;
                        else if (sperr(tf, CLT(clogs), BTT(bits[j]), t1, t2, i) < sperr(tc, CLT(clogs), BTT(bits[j]), t1, t2, i))
                            dr[j] = tf;
                        else
#endif
                            dr[j] = (int)floorf(data[0][j] + 0.5f);

#ifdef USE_BC6RAMPS
                        tr = sperr(dr[j], CLT(clogs), BTT(bits[j]), t1, t2, i) + 2.0f * sqrtf(sperr(dr[j], CLT(clogs), BTT(bits[j]), t1, t2, i)) * fabsf((float)dr[j] - data[0][j]) +
                            (dr[j] - data[0][j])* (dr[j] - data[0][j]);
                        if (tr < t_)
                        {
                            t_ = tr;
#else
                        t_ = 0;
#endif

                        t1o[j] = t1;
                        t2o[j] = t2;
                        dr_0[j] = dr[j];
#ifdef USE_BC6RAMPS
                        if ((dr_0[j] < 0) || (dr_0[j] > 255))
                        {
                            dr_0[j] = 0; // Error!
                        }
                        }
#endif
                    } // B
                } //C

            t += t_;
            } // D


        if (t < err_0)
        {

            idx = i;

            for (j = 0; j < dimension; j++)
            {
#ifdef USE_BC6RAMPS
                CGU_INT p1 = CLT(clogs);        // < 3
                CGU_INT p2 = BTT(bits[j]);     // < 4
                CGU_INT in_data = dr_0[j];          // < SP_ERRIDX_MAX
                CGU_INT p4 = t1o[j];           // < 2
                CGU_INT p5 = t2o[j];           // < 2
                CGU_INT p6 = i;                // < 16

                                           // New spidx
                epo_0[0][j] = spidx(in_data, p1, p2, p4, p5, p6, 0);
                epo_0[1][j] = spidx(in_data, p1, p2, p4, p5, p6, 1);

                if (epo_0[1][j] >= SP_ERRIDX_MAX)
                {
                    epo_0[1][j] = 0; // Error!!
                }
#else
                epo_0[0][j] = 0;
                epo_0[1][j] = 0;
#endif
            }
            err_0 = t;
        }
        if (err_0 == 0)
            break;
        } // E

    if (err_0 < err_1)
    {
        idx_1 = idx;
        for (j = 0; j < dimension; j++)
        {
            epo_1[0][j] = epo_0[0][j];
            epo_1[1][j] = epo_0[1][j];
        }
        err_1 = err_0;
    }

    if (err_1 == 0)
        break;
    } //1

for (i = 0; i < numEntries; i++)
{
    index[i] = idx_1;
    for (j = 0; j < dimension; j++)
    {
        CGU_INT p1 = CLT(clogs);        // < 3
        CGU_INT p3 = epo_1[0][j];      // < SP_ERRIDX_MAX
        CGU_INT p4 = epo_1[1][j];      // < SP_ERRIDX_MAX
        CGU_INT p5 = idx_1;            // < 16
#pragma warning( push )
#pragma warning(disable:4244)
        out[i][j] = (int)rampf(p1, p3, p4, p5);
#pragma warning( pop )
    }
}
return err_1 * numEntries;
}

//========================================================================================================================

CGU_FLOAT ep_shaker_HD(CGU_FLOAT   data[MAX_ENTRIES][MAX_DIMENSION_BIG],
                       CGU_INT     numEntries,
                       CGU_INT     index_[MAX_ENTRIES],
                       CGU_FLOAT   out[MAX_ENTRIES][MAX_DIMENSION_BIG],
                       CGU_INT     epo_code_out[2][MAX_DIMENSION_BIG],
                       CGU_INT     Mi_,                // last cluster
                       CGU_INT     bits[3],            // including parity
                       CGU_INT     channels3or4
)
{
    CGU_INT i, j, k;
    CGU_INT use_par = 0;
    CGU_INT clogs = 0;

    i = Mi_ + 1;
    while (i >>= 1)
        clogs++;

    CGU_FLOAT mean[MAX_DIMENSION_BIG];
    CGU_INT index[MAX_ENTRIES];
    CGU_INT Mi;

    CGU_INT maxTry = 1;

    for (k = 0; k < numEntries; k++)
    {
        index[k] = index_[k];
    }

    CGU_INT done;
    CGU_INT change;

    CGU_INT better;

    CGU_FLOAT   err_o = CMP_FLOAT_MAX;
    CGU_FLOAT   out_2[MAX_ENTRIES][MAX_DIMENSION_BIG];
    CGU_INT     idx_2[MAX_ENTRIES];
    CGU_INT     epo_2[2][MAX_DIMENSION_BIG];

    CGU_INT max_bits[MAX_DIMENSION_BIG];
    CGU_INT type = bits[0] % (2 * channels3or4);

    for (j = 0; j < channels3or4; j++)
        max_bits[j] = (bits[0] + 2 * channels3or4 - 1) / (2 * channels3or4);


    // handled below automatically
    CGU_INT alls = all_same_d(data, numEntries, channels3or4);

    mean_d_d(data, mean, numEntries, channels3or4);

    do {
        index_collapse_kernel(index, numEntries);

        Mi = max_index(index, numEntries);  // index can be from requantizer

        CGU_INT p, q;
        CGU_INT p0 = -1, q0 = -1;

        CGU_FLOAT err_2 = CMP_FLOAT_MAX;

        if (Mi == 0) {
            CGU_FLOAT t;
            CGU_INT    epo_0[2][MAX_DIMENSION_BIG];
            // either sinle point from the beginning or collapsed index
            if (alls) {
                t = quant_single_point_d(data, numEntries, index, out_2, epo_0, Mi_, type, channels3or4);
            }
            else
            {
                quant_single_point_d(&mean, numEntries, index, out_2, epo_0, Mi_, type, channels3or4);
                t = totalError_d(data, out_2, numEntries, channels3or4);
            }

            if (t < err_o) {
                for (k = 0; k < numEntries; k++) {
                    index_[k] = index[k];
                    for (j = 0; j < channels3or4; j++) {
                        out[k][j] = out_2[k][j];
                        epo_code_out[0][j] = epo_0[0][j];
                        epo_code_out[1][j] = epo_0[1][j];
                    }
                };
                err_o = t;
            }
            return err_o;
        }

        //===============================
        // We have ramp colors to process
        //===============================

        for (q = 1; Mi != 0 && q*Mi <= Mi_; q++) // does not work for single point collapsed index!!!
        {
            for (p = 0; p <= Mi_ - q * Mi; p++)
            {

                //-------------------------------------
                // set a new index data to try
                //-------------------------------------
                CGU_INT cidx[MAX_ENTRIES];

                for (k = 0; k < numEntries; k++)
                {
                    cidx[k] = index[k] * q + p;
                }

                CGU_FLOAT epa[2][MAX_DIMENSION_BIG];

                //
                // solve RMS problem for center
                //

                CGU_FLOAT im[2][2] = { { 0,0 },{ 0,0 } };   // matrix /inverse matrix
                CGU_FLOAT rp[2][MAX_DIMENSION_BIG];            // right part for RMS fit problem

                                                           // get ideal clustr centers
                CGU_FLOAT cc[MAX_CLUSTERS_BIG][MAX_DIMENSION_BIG];
                CGU_INT index_cnt[MAX_CLUSTERS_BIG];                        // count of index entries
                CGU_INT index_comp[MAX_CLUSTERS_BIG];                       // compacted index
                CGU_INT index_ncl;                                            // number of unique indexes

                index_ncl = cluster_mean_d_d(data, cc, cidx, index_comp, index_cnt, numEntries, channels3or4); // unrounded

                for (i = 0; i < index_ncl; i++)
                    for (j = 0; j < channels3or4; j++)
                        cc[index_comp[i]][j] = (CGU_FLOAT)floorf(cc[index_comp[i]][j] + 0.5f); // more or less ideal location

                for (j = 0; j < channels3or4; j++)
                {
                    rp[0][j] = rp[1][j] = 0;
                }

                // weight with cnt if runnning on compacted index
                for (k = 0; k < numEntries; k++)
                {
                    im[0][0] += (Mi_ - cidx[k])* (Mi_ - cidx[k]);
                    im[0][1] += cidx[k] * (Mi_ - cidx[k]);           // im is symmetric
                    im[1][1] += cidx[k] * cidx[k];

                    for (j = 0; j < channels3or4; j++)
                    {
                        rp[0][j] += (Mi_ - cidx[k]) * cc[cidx[k]][j];
                        rp[1][j] += cidx[k] * cc[cidx[k]][j];
                    }
                }

                CGU_FLOAT dd = im[0][0] * im[1][1] - im[0][1] * im[0][1];

                //assert(dd !=0);

                // dd=0 means that cidx[k] and (Mi_-cidx[k]) collinear which implies only one active index;
                // taken care of separately

                im[1][0] = im[0][0];
                im[0][0] = im[1][1] / dd;
                im[1][1] = im[1][0] / dd;
                im[1][0] = im[0][1] = -im[0][1] / dd;

                for (j = 0; j < channels3or4; j++) {
                    epa[0][j] = (im[0][0] * rp[0][j] + im[0][1] * rp[1][j])*Mi_;
                    epa[1][j] = (im[1][0] * rp[0][j] + im[1][1] * rp[1][j])*Mi_;
                }

                CGU_FLOAT err_1 = CMP_FLOAT_MAX;
                CGU_FLOAT out_1[MAX_ENTRIES][MAX_DIMENSION_BIG];
                CGU_INT idx_1[MAX_ENTRIES];
                CGU_INT epo_1[2][MAX_DIMENSION_BIG];
                CGU_INT s1 = 0;
                CGU_FLOAT epd[2][MAX_DIMENSION_BIG][2];   // first second, coord, begin range end range

                for (j = 0; j < channels3or4; j++)
                {
                    for (i = 0; i < 2; i++)
                    {     // set range
                        epd[i][j][0] = epd[i][j][1] = epa[i][j];
                        epd[i][j][1] += ((1 << bits[j]) - 1 - (int)epd[i][j][1] < (1 << use_par) ?
                            (1 << bits[j]) - 1 - (int)epd[i][j][1] : (1 << use_par)) & (~use_par);
                    }
                }

                CGU_FLOAT ce[MAX_ENTRIES][MAX_CLUSTERS_BIG][MAX_DIMENSION_BIG];
                CGU_FLOAT err_0 = 0;
                CGU_FLOAT out_0[MAX_ENTRIES][MAX_DIMENSION_BIG];
                CGU_INT idx_0[MAX_ENTRIES];

                for (i = 0; i < numEntries; i++)
                {
                    CGU_FLOAT d[4];
                    d[0] = data[i][0];
                    d[1] = data[i][1];
                    d[2] = data[i][2];
                    d[3] = data[i][3];
                    for (j = 0; j < (1 << clogs); j++)
                        for (k = 0; k < channels3or4; k++)
                        {
                            ce[i][j][k] = (rampf(CLT(clogs), epd[0][k][0], epd[1][k][0], j) - d[k])*
                                (rampf(CLT(clogs), epd[0][k][0], epd[1][k][0], j) - d[k]);
                        }
                }

                CGU_INT s = 0, p1, g;
                CGU_INT ei0 = 0, ei1 = 0;

                for (p1 = 0; p1 < 64; p1++)
                {
                    CGU_INT j0 = 0;

                    // Gray code increment
                    g = p1 & (-p1);

                    err_0 = 0;

                    for (j = 0; j < channels3or4; j++)
                    {
                        if (((g >> (2 * j)) & 0x3) != 0)
                        {
                            j0 = j;
                            // new cords
                            ei0 = (((s^g) >> (2 * j)) & 0x1);
                            ei1 = (((s^g) >> (2 * j + 1)) & 0x1);
                        }
                    }
                    s = s ^ g;
                    err_0 = 0;

                    for (i = 0; i < numEntries; i++)
                    {
                        CGU_FLOAT d[4];
                        d[0] = data[i][0];
                        d[1] = data[i][1];
                        d[2] = data[i][2];
                        d[3] = data[i][3];
                        CGU_INT    ci = 0;
                        CGU_FLOAT cmin = CMP_FLOAT_MAX;

                        for (j = 0; j < (1 << clogs); j++)
                        {
                            float t_ = 0.;
                            ce[i][j][j0] = (rampf(CLT(clogs), epd[0][j0][ei0], epd[1][j0][ei1], j) - d[j0])*
                                (rampf(CLT(clogs), epd[0][j0][ei0], epd[1][j0][ei1], j) - d[j0]);
                            for (k = 0; k < channels3or4; k++)
                            {
                                t_ += ce[i][j][k];
                            }

                            if (t_ < cmin)
                            {
                                cmin = t_;
                                ci = j;
                            }
                        }

                        idx_0[i] = ci;
                        for (k = 0; k < channels3or4; k++)
                        {
                            out_0[i][k] = rampf(CLT(clogs), epd[0][k][ei0], epd[1][k][ei1], ci);
                        }
                        err_0 += cmin;
                    }

                    if (err_0 < err_1)
                    {
                        // best in the curent ep cube run
                        for (i = 0; i < numEntries; i++)
                        {
                            idx_1[i] = idx_0[i];
                            for (j = 0; j < channels3or4; j++)
                                out_1[i][j] = out_0[i][j];
                        }
                        err_1 = err_0;

                        s1 = s; // epo coding             
                    }
                }

                // reconstruct epo
                for (j = 0; j < channels3or4; j++)
                {
                    {
                        // new cords
                        ei0 = ((s1 >> (2 * j)) & 0x1);
                        ei1 = ((s1 >> (2 * j + 1)) & 0x1);
                        epo_1[0][j] = (int)epd[0][j][ei0];
                        epo_1[1][j] = (int)epd[1][j][ei1];
                    }
                }

                if (err_1 < err_2)
                {
                    // best in the curent ep cube run
                    for (i = 0; i < numEntries; i++)
                    {
                        idx_2[i] = idx_1[i];
                        for (j = 0; j < channels3or4; j++)
                            out_2[i][j] = out_1[i][j];
                    }
                    err_2 = err_1;
                    for (j = 0; j < channels3or4; j++)
                    {
                        epo_2[0][j] = epo_1[0][j];
                        epo_2[1][j] = epo_1[1][j];
                    }
                    p0 = p;
                    q0 = q;
                }
            }
        }

        // change/better
        change = 0;
        for (k = 0; k < numEntries; k++)
            change = change || (index[k] * q0 + p0 != idx_2[k]);

        better = err_2 < err_o;

        if (better)
        {
            for (k = 0; k < numEntries; k++)
            {
                index_[k] = index[k] = idx_2[k];
                for (j = 0; j < channels3or4; j++)
                {
                    out[k][j] = out_2[k][j];
                    epo_code_out[0][j] = epo_2[0][j];
                    epo_code_out[1][j] = epo_2[1][j];
                }
            }
            err_o = err_2;
        }

        done = !(change  &&  better);

        if (maxTry > 0) maxTry--;
        else maxTry = 0;

    } while (!done && maxTry);

    return err_o;
}


#ifndef ASPM_GPU
static CGU_INT g_aWeights3[] = { 0, 9, 18, 27, 37, 46, 55, 64 };                                // 3 bit color Indices
static CGU_INT g_aWeights4[] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 }; // 4 bit color indices

CGU_FLOAT lerpf(CGU_FLOAT a, CGU_FLOAT b, CGU_INT i, CGU_INT denom)
{
    assert(denom == 3 || denom == 7 || denom == 15);
    assert(i >= 0 && i <= denom);

    CGU_INT *weights = NULL;

    switch (denom)
    {
    case 3:     denom *= 5; i *= 5;    // fall through to case 15
    case 7:     weights = g_aWeights3; break;
    case 15:    weights = g_aWeights4; break;
    default:    assert(0);
    }
    return (a*weights[denom - i] + b * weights[i]) / 64.0f;
}
#else

CGU_FLOAT lerpf(CGU_FLOAT a, CGU_FLOAT b, CGU_INT i, CGU_INT denom)
{
    CGU_INT g_aWeights3[] = { 0, 9, 18, 27, 37, 46, 55, 64 };                                // 3 bit color Indices
    CGU_INT g_aWeights4[] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 }; // 4 bit color indices
    switch (denom)
    {
    case 7:     return ((a*g_aWeights3[denom - i] + b * g_aWeights3[i]) / 64.0f); break;
    case 15:    return ((a*g_aWeights4[denom - i] + b * g_aWeights4[i]) / 64.0f); break;
    default:
    case 3:// fall through to case 15
        denom *= 5;
        i *= 5;
        return ((a*g_aWeights3[denom - i] + b * g_aWeights3[i]) / 64.0f);   break;
    }
}
#endif

void palitizeEndPointsF(BC6H_Encode_local *BC6H_data, CGU_FLOAT fEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG])
{
    // scale endpoints
    CGU_FLOAT  Ar, Ag, Ab, Br, Bg, Bb;


    // Compose index colors from end points
    if (BC6H_data->region == 1)
    {
        Ar = fEndPoints[0][0][0];
        Ag = fEndPoints[0][0][1];
        Ab = fEndPoints[0][0][2];
        Br = fEndPoints[0][1][0];
        Bg = fEndPoints[0][1][1];
        Bb = fEndPoints[0][1][2];

        for (CGU_INT i = 0; i < 16; i++)
        {

            // Red
            BC6H_data->Paletef[0][i].x = lerpf(Ar, Br, i, 15);
            // Green
            BC6H_data->Paletef[0][i].y = lerpf(Ag, Bg, i, 15);
            // Blue
            BC6H_data->Paletef[0][i].z = lerpf(Ab, Bb, i, 15);
        }

    }
    else //mode.type == BC6_TWO
    {
        for (CGU_INT region = 0; region < 2; region++)
        {
            Ar = fEndPoints[region][0][0];
            Ag = fEndPoints[region][0][1];
            Ab = fEndPoints[region][0][2];
            Br = fEndPoints[region][1][0];
            Bg = fEndPoints[region][1][1];
            Bb = fEndPoints[region][1][2];
            for (CGU_INT i = 0; i < 8; i++)
            {
                // Red
                BC6H_data->Paletef[region][i].x = lerpf(Ar, Br, i, 7);
                // Greed
                BC6H_data->Paletef[region][i].y = lerpf(Ag, Bg, i, 7);
                // Blue
                BC6H_data->Paletef[region][i].z = lerpf(Ab, Bb, i, 7);
            }

        }
    }
}

CGU_FLOAT CalcShapeError(BC6H_Encode_local *BC6H_data, CGU_FLOAT fEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], CGU_BOOL SkipPallet)
{
    CGU_INT maxPallet;
    CGU_INT subset = 0;
    CGU_FLOAT  totalError = 0.0f;
    CGU_INT region = (BC6H_data->region - 1);

    if (region == 0)
        maxPallet = 16;
    else
        maxPallet = 8;

    if (!SkipPallet)
        palitizeEndPointsF(BC6H_data, fEndPoints);

    for (CGU_INT i = 0; i < MAX_SUBSET_SIZE; i++)
    {
        CGU_FLOAT error = 0.0f;
        CGU_FLOAT bestError = 0.0f;

        if (region == 0)
        {
            subset = 0;
        }
        else
        {
            // get the shape subset 0 or  1
            subset = BC6_PARTITIONS[BC6H_data->d_shape_index][i];
        }

        // initialize bestError to the difference for first data
        bestError = abs(BC6H_data->din[i][0] - BC6H_data->Paletef[subset][0].x) +
            abs(BC6H_data->din[i][1] - BC6H_data->Paletef[subset][0].y) +
            abs(BC6H_data->din[i][2] - BC6H_data->Paletef[subset][0].z);

        // loop through the rest of the data until find the best error 
        for (CGU_INT j = 1; j < maxPallet && bestError > 0; j++)
        {
            error = abs(BC6H_data->din[i][0] - BC6H_data->Paletef[subset][j].x) +
                abs(BC6H_data->din[i][1] - BC6H_data->Paletef[subset][j].y) +
                abs(BC6H_data->din[i][2] - BC6H_data->Paletef[subset][j].z);

            if (error <= bestError)
                bestError = error;
            else
                break;
        }
        totalError += bestError;
    }

    return totalError;
}

CGU_FLOAT FindBestPattern(BC6H_Encode_local * BC6H_data, CGU_BOOL TwoRegionShapes, CGU_INT8 shape_pattern, CGU_FLOAT quality)
{
    // Index bit size for the patterns been used. 
    // All two zone shapes have 3 bits per color, max index value < 8  
    // All one zone shapes gave 4 bits per color, max index value < 16
    CGU_INT8   Index_BitSize = TwoRegionShapes ? 8 : 16;
    CGU_INT8   max_subsets = TwoRegionShapes ? 2 : 1;
    CGU_FLOAT  direction[NCHANNELS];
    CGU_FLOAT  step;

    BC6H_data->region = max_subsets;
    BC6H_data->index = 0;
    BC6H_data->d_shape_index = shape_pattern;
    memset((CGU_UINT8 *)BC6H_data->partition, 0, sizeof(BC6H_data->partition));
    memset((CGU_UINT8 *)BC6H_data->shape_indices, 0, sizeof(BC6H_data->shape_indices));

    // Get the pattern to encode with
    Partition(shape_pattern,          // Shape pattern we want to get
        BC6H_data->din,          // Input data
        BC6H_data->partition,    // Returns the patterned shape data
        BC6H_data->entryCount,   // counts the number of pixel used in each subset region num of 0's amd 1's
        max_subsets,            // Table Shapes to use eithe one regions 1 or two regions 2
        3);                     // rgb no alpha always = 3

    CGU_FLOAT  error[MAX_SUBSETS] = { 0.0, CMP_FLOAT_MAX,CMP_FLOAT_MAX };
    CGU_INT    BestOutB = 0;
    CGU_FLOAT  BestError;        //the lowest error from vector direction quantization
    CGU_FLOAT  BestError_endpts; //the lowest error from endpoints extracted from the vector direction quantization

    CGU_FLOAT   outB[2][2][MAX_SUBSET_SIZE][MAX_DIMENSION_BIG];
    CGU_INT         shape_indicesB[2][MAX_SUBSETS][MAX_SUBSET_SIZE];

    for (CGU_INT subset = 0; subset < max_subsets; subset++)
    {
        error[0] += optQuantAnD_d(
            BC6H_data->partition[subset],        // input data 
            BC6H_data->entryCount[subset],       // number of input points above (not clear about 1, better to avoid)
            Index_BitSize,                      // number of clusters on the ramp, 8  or 16
            shape_indicesB[0][subset],          // output index, if not all points of the ramp used, 0 may not be assigned
            outB[0][subset],                    // resulting quantization
            direction,                          // direction vector of the ramp (check normalization) 
            &step,                              // step size (check normalization) 
            3,                                  // number of channels (always 3 = RGB for BC6H)
            quality                           // Quality set number of retry to get good end points 
                                                // Max retries = MAX_TRY = 4000 when Quality is 1.0
                                                // Min = 0 and default with quality 0.05 is 200 times
        );
    }

    BestError = error[0];
    BestOutB = 0;

    // The following code is almost complete - runs very slow and not sure if % of improvement is justified..
#ifdef USE_SHAKERHD
    // Valid only for 2 region shapes
    if ((max_subsets > 1) && (quality > 0.80))
    {
        CGU_INT     tempIndices[MAX_SUBSET_SIZE];
        // CGU_INT     temp_epo_code[2][2][MAX_DIMENSION_BIG];
        CGU_INT     bits[3] = { 8,8,8 };     // Channel index bit size

        // CGU_FLOAT   epo[2][MAX_DIMENSION_BIG];
        CGU_INT     epo_code[MAX_SUBSETS][2][MAX_DIMENSION_BIG];
        // CGU_INT     shakeSize = 8;

        error[1] = 0.0;
        for (CGU_INT subset = 0; subset < max_subsets; subset++)
        {
            for (CGU_INT k = 0; k < BC6H_data->entryCount[subset]; k++)
            {
                tempIndices[k] = shape_indicesB[0][subset][k];
            }

            error[1] += ep_shaker_HD(
                BC6H_data->partition[subset],
                BC6H_data->entryCount[subset],
                tempIndices,                    // output index, if not all points of the ramp used, 0 may not be assigned
                outB[1][subset],                // resulting quantization
                epo_code[subset],
                BC6H_data->entryCount[subset] - 1,
                bits,
                3
            );

            // error[1] += ep_shaker_2_d(
            //      BC6H_data.partition[subset],
            //      BC6H_data.entryCount[subset],
            //      tempIndices,                    // output index, if not all points of the ramp used, 0 may not be assigned
            //      outB[1][subset],                // resulting quantization
            //      epo_code[subset],
            //      shakeSize,
            //      BC6H_data.entryCount[subset] - 1,
            //      bits[0],
            //      3,
            //      epo
            //      );


            for (CGU_INT k = 0; k < BC6H_data->entryCount[subset]; k++)
            {
                shape_indicesB[1][subset][k] = tempIndices[k];
            }

        } // subsets

        if (BestError > error[1])
        {
            BestError = error[1];
            BestOutB = 1;
            for (CGU_INT subset = 0; subset < max_subsets; subset++)
            {
                for (CGU_INT k = 0; k < MAX_DIMENSION_BIG; k++)
                {
                    BC6H_data->fEndPoints[subset][0][k] = (CGU_FLOAT)epo_code[subset][0][k];
                    BC6H_data->fEndPoints[subset][1][k] = (CGU_FLOAT)epo_code[subset][1][k];
                }
            }
        }

    }
#endif

    // Save the best for BC6H data processing later
    if (BestOutB == 0)
        GetEndPoints(BC6H_data->fEndPoints, outB[BestOutB], max_subsets, BC6H_data->entryCount);

    memcpy((CGU_UINT8 *)BC6H_data->shape_indices, (CGU_UINT8 *)shape_indicesB[BestOutB], sizeof(BC6H_data->shape_indices));
    clampF16Max(BC6H_data->fEndPoints, BC6H_data->issigned);

    BestError_endpts = CalcShapeError(BC6H_data, BC6H_data->fEndPoints, false);
    return BestError_endpts;
}

#ifndef ASPM_GPU
void SaveDataBlock(BC6H_Encode_local *bc6h_format, CMP_GLOBAL CGU_UINT8 cmpout[COMPRESSED_BLOCK_SIZE])
{
    BitHeader header(NULL, COMPRESSED_BLOCK_SIZE);

    // Save the RGB end point values
    switch (bc6h_format->m_mode)
    {
    case 1: //0x00
        header.setvalue(0, 2, 0x00);
        header.setvalue(2, 1, bc6h_format->gy, 4);        //        gy[4]
        header.setvalue(3, 1, bc6h_format->by, 4);        //        by[4]
        header.setvalue(4, 1, bc6h_format->bz, 4);        //        bz[4]
        header.setvalue(5, 10, bc6h_format->rw);          // 10:    rw[9:0] 
        header.setvalue(15, 10, bc6h_format->gw);          // 10:    gw[9:0]
        header.setvalue(25, 10, bc6h_format->bw);          // 10:    bw[9:0]
        header.setvalue(35, 5, bc6h_format->rx);          // 5:     rx[4:0]
        header.setvalue(40, 1, bc6h_format->gz, 4);        //        gz[4]
        header.setvalue(41, 4, bc6h_format->gy);          // 5:     gy[3:0]
        header.setvalue(45, 5, bc6h_format->gx);          // 5:     gx[4:0]
        header.setvalue(50, 1, bc6h_format->bz);          // 5:     bz[0]
        header.setvalue(51, 4, bc6h_format->gz);          // 5:     gz[3:0]
        header.setvalue(55, 5, bc6h_format->bx);          // 5:     bx[4:0]
        header.setvalue(60, 1, bc6h_format->bz, 1);        //        bz[1]
        header.setvalue(61, 4, bc6h_format->by);          // 5:     by[3:0]
        header.setvalue(65, 5, bc6h_format->ry);          // 5:     ry[4:0]
        header.setvalue(70, 1, bc6h_format->bz, 2);        //        bz[2]
        header.setvalue(71, 5, bc6h_format->rz);          // 5:     rz[4:0]
        header.setvalue(76, 1, bc6h_format->bz, 3);        //        bz[3]
        break;
    case 2: // 0x01
        header.setvalue(0, 2, 0x01);
        header.setvalue(2, 1, bc6h_format->gy, 5);        //        gy[5]
        header.setvalue(3, 1, bc6h_format->gz, 4);        //        gz[4]
        header.setvalue(4, 1, bc6h_format->gz, 5);        //        gz[5]
        header.setvalue(5, 7, bc6h_format->rw);          //        rw[6:0] 
        header.setvalue(12, 1, bc6h_format->bz);          //        bz[0]
        header.setvalue(13, 1, bc6h_format->bz, 1);        //        bz[1]
        header.setvalue(14, 1, bc6h_format->by, 4);        //        by[4]
        header.setvalue(15, 7, bc6h_format->gw);          //        gw[6:0]
        header.setvalue(22, 1, bc6h_format->by, 5);        //        by[5]
        header.setvalue(23, 1, bc6h_format->bz, 2);        //        bz[2]
        header.setvalue(24, 1, bc6h_format->gy, 4);        //        gy[4]
        header.setvalue(25, 7, bc6h_format->bw);          // 7:     bw[6:0]
        header.setvalue(32, 1, bc6h_format->bz, 3);        //        bz[3]
        header.setvalue(33, 1, bc6h_format->bz, 5);        //        bz[5]
        header.setvalue(34, 1, bc6h_format->bz, 4);        //        bz[4]
        header.setvalue(35, 6, bc6h_format->rx);          // 6:     rx[5:0]
        header.setvalue(41, 4, bc6h_format->gy);          // 6:     gy[3:0]
        header.setvalue(45, 6, bc6h_format->gx);          // 6:     gx[5:0]
        header.setvalue(51, 4, bc6h_format->gz);          // 6:     gz[3:0]
        header.setvalue(55, 6, bc6h_format->bx);          // 6:     bx[5:0]
        header.setvalue(61, 4, bc6h_format->by);          // 6:     by[3:0]
        header.setvalue(65, 6, bc6h_format->ry);          // 6:     ry[5:0]
        header.setvalue(71, 6, bc6h_format->rz);          // 6:     rz[5:0]
        break;
    case 3: // 0x02
        header.setvalue(0, 5, 0x02);
        header.setvalue(5, 10, bc6h_format->rw);          // 11:    rw[9:0] 
        header.setvalue(15, 10, bc6h_format->gw);          // 11:    gw[9:0]
        header.setvalue(25, 10, bc6h_format->bw);          // 11:    bw[9:0]
        header.setvalue(35, 5, bc6h_format->rx);          // 5:     rx[4:0]
        header.setvalue(40, 1, bc6h_format->rw, 10);       //        rw[10]
        header.setvalue(41, 4, bc6h_format->gy);          // 4:     gy[3:0]
        header.setvalue(45, 4, bc6h_format->gx);          // 4:     gx[3:0]
        header.setvalue(49, 1, bc6h_format->gw, 10);       //        gw[10]
        header.setvalue(50, 1, bc6h_format->bz);          // 4:     bz[0]
        header.setvalue(51, 4, bc6h_format->gz);          // 4:     gz[3:0]
        header.setvalue(55, 4, bc6h_format->bx);          // 4:     bx[3:0]
        header.setvalue(59, 1, bc6h_format->bw, 10);       //        bw[10]
        header.setvalue(60, 1, bc6h_format->bz, 1);        //        bz[1]
        header.setvalue(61, 4, bc6h_format->by);          // 4:     by[3:0]
        header.setvalue(65, 5, bc6h_format->ry);          // 5:     ry[4:0]
        header.setvalue(70, 1, bc6h_format->bz, 2);        //        bz[2]
        header.setvalue(71, 5, bc6h_format->rz);          // 5:     rz[4:0]
        header.setvalue(76, 1, bc6h_format->bz, 3);        //        bz[3]
        break;
    case 4: // 0x06
        header.setvalue(0, 5, 0x06);
        header.setvalue(5, 10, bc6h_format->rw);          // 11:    rw[9:0] 
        header.setvalue(15, 10, bc6h_format->gw);          // 11:    gw[9:0]
        header.setvalue(25, 10, bc6h_format->bw);          // 11:    bw[9:0]
        header.setvalue(35, 4, bc6h_format->rx);          //        rx[3:0]
        header.setvalue(39, 1, bc6h_format->rw, 10);       //        rw[10]
        header.setvalue(40, 1, bc6h_format->gz, 4);        //        gz[4]
        header.setvalue(41, 4, bc6h_format->gy);          // 5:     gy[3:0]
        header.setvalue(45, 5, bc6h_format->gx);          //        gx[4:0]
        header.setvalue(50, 1, bc6h_format->gw, 10);       // 5:     gw[10]
        header.setvalue(51, 4, bc6h_format->gz);          // 5:     gz[3:0]
        header.setvalue(55, 4, bc6h_format->bx);          // 4:     bx[3:0]
        header.setvalue(59, 1, bc6h_format->bw, 10);       //        bw[10]
        header.setvalue(60, 1, bc6h_format->bz, 1);        //        bz[1]
        header.setvalue(61, 4, bc6h_format->by);          // 4:     by[3:0]
        header.setvalue(65, 4, bc6h_format->ry);          // 4:     ry[3:0]
        header.setvalue(69, 1, bc6h_format->bz);          // 4:     bz[0]
        header.setvalue(70, 1, bc6h_format->bz, 2);        //        bz[2]
        header.setvalue(71, 4, bc6h_format->rz);          // 4:     rz[3:0]
        header.setvalue(75, 1, bc6h_format->gy, 4);        //        gy[4]
        header.setvalue(76, 1, bc6h_format->bz, 3);        //        bz[3]
        break;
    case 5: // 0x0A
        header.setvalue(0, 5, 0x0A);
        header.setvalue(5, 10, bc6h_format->rw);           // 11:   rw[9:0] 
        header.setvalue(15, 10, bc6h_format->gw);           // 11:   gw[9:0]
        header.setvalue(25, 10, bc6h_format->bw);           // 11:   bw[9:0]
        header.setvalue(35, 4, bc6h_format->rx);           // 4:    rx[3:0]
        header.setvalue(39, 1, bc6h_format->rw, 10);        //       rw[10]
        header.setvalue(40, 1, bc6h_format->by, 4);         //       by[4]
        header.setvalue(41, 4, bc6h_format->gy);           // 4:    gy[3:0]
        header.setvalue(45, 4, bc6h_format->gx);           // 4:    gx[3:0]
        header.setvalue(49, 1, bc6h_format->gw, 10);        //       gw[10]
        header.setvalue(50, 1, bc6h_format->bz);           // 5:    bz[0]
        header.setvalue(51, 4, bc6h_format->gz);           // 4:    gz[3:0]
        header.setvalue(55, 5, bc6h_format->bx);           // 5:    bx[4:0]
        header.setvalue(60, 1, bc6h_format->bw, 10);        //       bw[10]
        header.setvalue(61, 4, bc6h_format->by);           // 5:    by[3:0]
        header.setvalue(65, 4, bc6h_format->ry);           // 4:    ry[3:0]
        header.setvalue(69, 1, bc6h_format->bz, 1);         //       bz[1]
        header.setvalue(70, 1, bc6h_format->bz, 2);         //       bz[2]
        header.setvalue(71, 4, bc6h_format->rz);           // 4:    rz[3:0]
        header.setvalue(75, 1, bc6h_format->bz, 4);         //       bz[4]
        header.setvalue(76, 1, bc6h_format->bz, 3);         //       bz[3]
        break;
    case 6: // 0x0E
        header.setvalue(0, 5, 0x0E);
        header.setvalue(5, 9, bc6h_format->rw);           // 9:    rw[8:0] 
        header.setvalue(14, 1, bc6h_format->by, 4);         //       by[4]
        header.setvalue(15, 9, bc6h_format->gw);           // 9:    gw[8:0]
        header.setvalue(24, 1, bc6h_format->gy, 4);         //       gy[4]
        header.setvalue(25, 9, bc6h_format->bw);           // 9:    bw[8:0]
        header.setvalue(34, 1, bc6h_format->bz, 4);         //       bz[4]
        header.setvalue(35, 5, bc6h_format->rx);           // 5:    rx[4:0]
        header.setvalue(40, 1, bc6h_format->gz, 4);         //       gz[4]
        header.setvalue(41, 4, bc6h_format->gy);           // 5:    gy[3:0]
        header.setvalue(45, 5, bc6h_format->gx);           // 5:    gx[4:0]
        header.setvalue(50, 1, bc6h_format->bz);           // 5:    bz[0]
        header.setvalue(51, 4, bc6h_format->gz);           // 5:    gz[3:0]
        header.setvalue(55, 5, bc6h_format->bx);           // 5:    bx[4:0]
        header.setvalue(60, 1, bc6h_format->bz, 1);         //       bz[1]
        header.setvalue(61, 4, bc6h_format->by);           // 5:    by[3:0]
        header.setvalue(65, 5, bc6h_format->ry);           // 5:    ry[4:0]
        header.setvalue(70, 1, bc6h_format->bz, 2);         //       bz[2]
        header.setvalue(71, 5, bc6h_format->rz);           // 5:    rz[4:0]
        header.setvalue(76, 1, bc6h_format->bz, 3);         //       bz[3]
        break;
    case 7: // 0x12
        header.setvalue(0, 5, 0x12);
        header.setvalue(5, 8, bc6h_format->rw);           // 8:    rw[7:0] 
        header.setvalue(13, 1, bc6h_format->gz, 4);         //       gz[4]
        header.setvalue(14, 1, bc6h_format->by, 4);         //       by[4]
        header.setvalue(15, 8, bc6h_format->gw);           // 8:    gw[7:0]
        header.setvalue(23, 1, bc6h_format->bz, 2);         //       bz[2]
        header.setvalue(24, 1, bc6h_format->gy, 4);         //       gy[4]
        header.setvalue(25, 8, bc6h_format->bw);           // 8:    bw[7:0]
        header.setvalue(33, 1, bc6h_format->bz, 3);         //       bz[3]
        header.setvalue(34, 1, bc6h_format->bz, 4);         //       bz[4]
        header.setvalue(35, 6, bc6h_format->rx);           // 6:    rx[5:0]
        header.setvalue(41, 4, bc6h_format->gy);           // 5:    gy[3:0]
        header.setvalue(45, 5, bc6h_format->gx);           // 5:    gx[4:0]
        header.setvalue(50, 1, bc6h_format->bz);           // 5:    bz[0]
        header.setvalue(51, 4, bc6h_format->gz);           // 5:    gz[3:0]
        header.setvalue(55, 5, bc6h_format->bx);           // 5:    bx[4:0]
        header.setvalue(60, 1, bc6h_format->bz, 1);         //       bz[1]
        header.setvalue(61, 4, bc6h_format->by);           // 5:    by[3:0]
        header.setvalue(65, 6, bc6h_format->ry);           // 6:    ry[5:0]
        header.setvalue(71, 6, bc6h_format->rz);           // 6:    rz[5:0]
        break;
    case 8: // 0x16
        header.setvalue(0, 5, 0x16);
        header.setvalue(5, 8, bc6h_format->rw);            // 8:   rw[7:0] 
        header.setvalue(13, 1, bc6h_format->bz);            // 5:   bz[0]
        header.setvalue(14, 1, bc6h_format->by, 4);          //      by[4]
        header.setvalue(15, 8, bc6h_format->gw);            // 8:   gw[7:0]
        header.setvalue(23, 1, bc6h_format->gy, 5);          //      gy[5]
        header.setvalue(24, 1, bc6h_format->gy, 4);          //      gy[4]
        header.setvalue(25, 8, bc6h_format->bw);            // 8:   bw[7:0]
        header.setvalue(33, 1, bc6h_format->gz, 5);          //      gz[5]
        header.setvalue(34, 1, bc6h_format->bz, 4);          //      bz[4]
        header.setvalue(35, 5, bc6h_format->rx);            // 5:   rx[4:0]
        header.setvalue(40, 1, bc6h_format->gz, 4);          //      gz[4]
        header.setvalue(41, 4, bc6h_format->gy);            // 6:   gy[3:0]
        header.setvalue(45, 6, bc6h_format->gx);            // 6:   gx[5:0]
        header.setvalue(51, 4, bc6h_format->gz);            // 6:   gz[3:0]
        header.setvalue(55, 5, bc6h_format->bx);            // 5:   bx[4:0]
        header.setvalue(60, 1, bc6h_format->bz, 1);          //      bz[1]
        header.setvalue(61, 4, bc6h_format->by);            // 5:   by[3:0]
        header.setvalue(65, 5, bc6h_format->ry);            // 5:   ry[4:0]
        header.setvalue(70, 1, bc6h_format->bz, 2);          //      bz[2]
        header.setvalue(71, 5, bc6h_format->rz);            // 5:   rz[4:0]
        header.setvalue(76, 1, bc6h_format->bz, 3);          //      bz[3]
        break;
    case 9: // 0x1A
        header.setvalue(0, 5, 0x1A);
        header.setvalue(5, 8, bc6h_format->rw);            // 8:   rw[7:0] 
        header.setvalue(13, 1, bc6h_format->bz, 1);          //      bz[1]
        header.setvalue(14, 1, bc6h_format->by, 4);          //      by[4]
        header.setvalue(15, 8, bc6h_format->gw);            // 8:   gw[7:0]
        header.setvalue(23, 1, bc6h_format->by, 5);          //      by[5]
        header.setvalue(24, 1, bc6h_format->gy, 4);          //      gy[4]
        header.setvalue(25, 8, bc6h_format->bw);            // 8:   bw[7:0]
        header.setvalue(33, 1, bc6h_format->bz, 5);          //      bz[5]
        header.setvalue(34, 1, bc6h_format->bz, 4);          //      bz[4]
        header.setvalue(35, 5, bc6h_format->rx);            // 5:   rx[4:0]
        header.setvalue(40, 1, bc6h_format->gz, 4);          //      gz[4]
        header.setvalue(41, 4, bc6h_format->gy);            // 5:   gy[3:0]
        header.setvalue(45, 5, bc6h_format->gx);            // 5:   gx[4:0]
        header.setvalue(50, 1, bc6h_format->bz);            // 6:   bz[0]
        header.setvalue(51, 4, bc6h_format->gz);            // 5:   gz[3:0]
        header.setvalue(55, 6, bc6h_format->bx);            // 6:   bx[5:0]
        header.setvalue(61, 4, bc6h_format->by);            // 6:   by[3:0]
        header.setvalue(65, 5, bc6h_format->ry);            // 5:   ry[4:0]
        header.setvalue(70, 1, bc6h_format->bz, 2);          //      bz[2]
        header.setvalue(71, 5, bc6h_format->rz);            // 5:   rz[4:0]
        header.setvalue(76, 1, bc6h_format->bz, 3);          //      bz[3]
        break;
    case 10: // 0x1E
        header.setvalue(0, 5, 0x1E);
        header.setvalue(5, 6, bc6h_format->rw);            // 6:   rw[5:0] 
        header.setvalue(11, 1, bc6h_format->gz, 4);          //      gz[4]
        header.setvalue(12, 1, bc6h_format->bz);            // 6:   bz[0]
        header.setvalue(13, 1, bc6h_format->bz, 1);          //      bz[1]
        header.setvalue(14, 1, bc6h_format->by, 4);          //      by[4]
        header.setvalue(15, 6, bc6h_format->gw);            // 6:   gw[5:0]
        header.setvalue(21, 1, bc6h_format->gy, 5);          //      gy[5]
        header.setvalue(22, 1, bc6h_format->by, 5);          //      by[5]
        header.setvalue(23, 1, bc6h_format->bz, 2);          //      bz[2]
        header.setvalue(24, 1, bc6h_format->gy, 4);          //      gy[4]
        header.setvalue(25, 6, bc6h_format->bw);            // 6:   bw[5:0]
        header.setvalue(31, 1, bc6h_format->gz, 5);          //      gz[5]
        header.setvalue(32, 1, bc6h_format->bz, 3);          //      bz[3]
        header.setvalue(33, 1, bc6h_format->bz, 5);          //      bz[5]
        header.setvalue(34, 1, bc6h_format->bz, 4);          //      bz[4]
        header.setvalue(35, 6, bc6h_format->rx);            // 6:   rx[5:0]
        header.setvalue(41, 4, bc6h_format->gy);            // 6:   gy[3:0]
        header.setvalue(45, 6, bc6h_format->gx);            // 6:   gx[5:0]
        header.setvalue(51, 4, bc6h_format->gz);            // 6:   gz[3:0]
        header.setvalue(55, 6, bc6h_format->bx);            // 6:   bx[5:0]
        header.setvalue(61, 4, bc6h_format->by);            // 6:   by[3:0]
        header.setvalue(65, 6, bc6h_format->ry);            // 6:   ry[5:0]
        header.setvalue(71, 6, bc6h_format->rz);            // 6:   rz[5:0]
        break;

        // Single regions Modes
    case 11: // 0x03
        header.setvalue(0, 5, 0x03);
        header.setvalue(5, 10, bc6h_format->rw);            // 10:   rw[9:0] 
        header.setvalue(15, 10, bc6h_format->gw);            // 10:   gw[9:0]
        header.setvalue(25, 10, bc6h_format->bw);            // 10:   bw[9:0]
        header.setvalue(35, 10, bc6h_format->rx);            // 10:   rx[9:0]
        header.setvalue(45, 10, bc6h_format->gx);            // 10:   gx[9:0]
        header.setvalue(55, 10, bc6h_format->bx);            // 10:   bx[9:0]
        break;
    case 12: // 0x07
        header.setvalue(0, 5, 0x07);
        header.setvalue(5, 10, bc6h_format->rw);            // 11:   rw[9:0] 
        header.setvalue(15, 10, bc6h_format->gw);            // 11:   gw[9:0]
        header.setvalue(25, 10, bc6h_format->bw);            // 11:   bw[9:0]
        header.setvalue(35, 9, bc6h_format->rx);            // 9:    rx[8:0]
        header.setvalue(44, 1, bc6h_format->rw, 10);         //       rw[10]
        header.setvalue(45, 9, bc6h_format->gx);            // 9:    gx[8:0]
        header.setvalue(54, 1, bc6h_format->gw, 10);         //       gw[10]
        header.setvalue(55, 9, bc6h_format->bx);            // 9:    bx[8:0]
        header.setvalue(64, 1, bc6h_format->bw, 10);         //       bw[10]
        break;
    case 13: // 0x0B
        header.setvalue(0, 5, 0x0B);
        header.setvalue(5, 10, bc6h_format->rw);            // 12:   rw[9:0] 
        header.setvalue(15, 10, bc6h_format->gw);            // 12:   gw[9:0]
        header.setvalue(25, 10, bc6h_format->bw);            // 12:   bw[9:0]
        header.setvalue(35, 8, bc6h_format->rx);            // 8:    rx[7:0]
        header.setvalue(43, 1, bc6h_format->rw, 11);         //       rw[11]
        header.setvalue(44, 1, bc6h_format->rw, 10);         //       rw[10]
        header.setvalue(45, 8, bc6h_format->gx);            // 8:    gx[7:0]
        header.setvalue(53, 1, bc6h_format->gw, 11);         //       gw[11]
        header.setvalue(54, 1, bc6h_format->gw, 10);         //       gw[10]
        header.setvalue(55, 8, bc6h_format->bx);            // 8:    bx[7:0]
        header.setvalue(63, 1, bc6h_format->bw, 11);         //       bw[11]
        header.setvalue(64, 1, bc6h_format->bw, 10);         //       bw[10]
        break;
    case 14: // 0x0F
        header.setvalue(0, 5, 0x0F);
        header.setvalue(5, 10, bc6h_format->rw);            // 16:   rw[9:0] 
        header.setvalue(15, 10, bc6h_format->gw);            // 16:   gw[9:0]
        header.setvalue(25, 10, bc6h_format->bw);            // 16:   bw[9:0]
        header.setvalue(35, 4, bc6h_format->rx);            //  4:   rx[3:0]
        header.setvalue(39, 6, bc6h_format->rw, 10);         //       rw[15:10]
        header.setvalue(45, 4, bc6h_format->gx);            //  4:   gx[3:0]
        header.setvalue(49, 6, bc6h_format->gw, 10);         //       gw[15:10]
        header.setvalue(55, 4, bc6h_format->bx);            //  4:   bx[3:0]
        header.setvalue(59, 6, bc6h_format->bw, 10);         //       bw[15:10]
        break;
    default: // Need to indicate error!
        return;
    }

    // Each format in the mode table can be uniquely identified by the mode bits. 
    // The first ten modes are used for two-region tiles, and the mode bit field 
    // can be either two or five bits long. These blocks also have fields for 
    // the compressed color endpoints (72 or 75 bits), the partition (5 bits), 
    // and the partition indices (46 bits).

    if (bc6h_format->m_mode >= MIN_MODE_FOR_ONE_REGION)
    {
        CGU_INT startbit = ONE_REGION_INDEX_OFFSET;
        header.setvalue(startbit, 3, bc6h_format->indices16[0]);
        startbit += 3;
        for (CGU_INT i = 1; i < 16; i++)
        {
            header.setvalue(startbit, 4, bc6h_format->indices16[i]);
            startbit += 4;
        }
    }
    else
    {
        header.setvalue(77, 5, bc6h_format->d_shape_index);            // Shape Index
        CGU_INT startbit = TWO_REGION_INDEX_OFFSET,
            nbits = 2;
        header.setvalue(startbit, nbits, bc6h_format->indices16[0]);
        for (CGU_INT i = 1; i < 16; i++)
        {
            startbit += nbits; // offset start bit for next index using prior nbits used
            nbits = g_indexfixups[bc6h_format->d_shape_index] == i ? 2 : 3; // get new number of bit to save index with
            header.setvalue(startbit, nbits, bc6h_format->indices16[i]);
        }
    }

    // save to output buffer our new bit values
    // this can be optimized if header is part of bc6h_format struct
    header.transferbits(cmpout, 16);
}
#else
void SaveDataBlock(BC6H_Encode_local *bc6h_format, CMP_GLOBAL CGU_UINT8 out[COMPRESSED_BLOCK_SIZE])
{
    // ToDo
}
#endif

void SwapIndices(CGU_INT32 iEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], CGU_INT32 iIndices[3][MAX_SUBSET_SIZE], CGU_INT  entryCount[MAX_SUBSETS], CGU_INT max_subsets, CGU_INT mode, CGU_INT shape_pattern)
{

    CGU_UINT32 uNumIndices = 1 << ModePartition[mode].IndexPrec;
    CGU_UINT32 uHighIndexBit = uNumIndices >> 1;

    for (CGU_INT subset = 0; subset < max_subsets; ++subset)
    {
        // region 0 (subset = 0) The fix-up index for this subset is allways index 0
        // region 1 (subset = 1) The fix-up index for this subset varies based on the shape 
        size_t i = subset ? g_Region2FixUp[shape_pattern] : 0;

        if (iIndices[subset][i] & uHighIndexBit)
        {
            // high bit is set, swap the aEndPts and indices for this region
            swap(iEndPoints[subset][0][0], iEndPoints[subset][1][0]);
            swap(iEndPoints[subset][0][1], iEndPoints[subset][1][1]);
            swap(iEndPoints[subset][0][2], iEndPoints[subset][1][2]);

            for (size_t j = 0; j < (size_t)entryCount[subset]; ++j)
            {
                iIndices[subset][j] = uNumIndices - 1 - iIndices[subset][j];
            }
        }

    }
}

// helper function to check transform overflow
// todo: check overflow by checking against sign
CGU_BOOL isOverflow(CGU_INT endpoint, CGU_INT nbit)
{
    CGU_INT maxRange = (int)pow(2.0f, (CGU_FLOAT)nbit - 1.0f) - 1;
    CGU_INT minRange = (int)-(pow(2.0f, (CGU_FLOAT)nbit - 1.0f));

    //no overflow
    if ((endpoint >= minRange) && (endpoint <= maxRange))
        return false;
    else //overflow
        return true;
}

CGU_BOOL TransformEndPoints(BC6H_Encode_local *BC6H_data, CGU_INT iEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], CGU_INT oEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], CGU_INT max_subsets, CGU_INT mode)
{
    CGU_INT Mask;
    if (ModePartition[mode].transformed)
    {
        BC6H_data->istransformed = true;
        for (CGU_INT i = 0; i < 3; ++i)
        {
            Mask = MASK(ModePartition[mode].nbits);
            oEndPoints[0][0][i] = iEndPoints[0][0][i] & Mask;    // [0][A]

            Mask = MASK(ModePartition[mode].prec[i]);
            oEndPoints[0][1][i] = iEndPoints[0][1][i] - iEndPoints[0][0][i]; // [0][B] - [0][A]

            if (isOverflow(oEndPoints[0][1][i], ModePartition[mode].prec[i]))
                return false;

            oEndPoints[0][1][i] = (oEndPoints[0][1][i] & Mask);

            //redo the check for sign overflow for one region case
            if (max_subsets <= 1)
            {
                if (isOverflow(oEndPoints[0][1][i], ModePartition[mode].prec[i]))
                    return false;
            }

            if (max_subsets > 1)
            {
                oEndPoints[1][0][i] = iEndPoints[1][0][i] - iEndPoints[0][0][i];  // [1][A] - [0][A]
                if (isOverflow(oEndPoints[1][0][i], ModePartition[mode].prec[i]))
                    return false;

                oEndPoints[1][0][i] = (oEndPoints[1][0][i] & Mask);

                oEndPoints[1][1][i] = iEndPoints[1][1][i] - iEndPoints[0][0][i];  // [1][B] - [0][A]
                if (isOverflow(oEndPoints[1][1][i], ModePartition[mode].prec[i]))
                    return false;

                oEndPoints[1][1][i] = (oEndPoints[1][1][i] & Mask);
            }
        }
    }
    else
    {
        BC6H_data->istransformed = false;
        for (CGU_INT i = 0; i < 3; ++i)
        {
            Mask = MASK(ModePartition[mode].nbits);
            oEndPoints[0][0][i] = iEndPoints[0][0][i] & Mask;

            Mask = MASK(ModePartition[mode].prec[i]);
            oEndPoints[0][1][i] = iEndPoints[0][1][i] & Mask;

            if (max_subsets > 1)
            {
                oEndPoints[1][0][i] = iEndPoints[1][0][i] & Mask;
                oEndPoints[1][1][i] = iEndPoints[1][1][i] & Mask;
            }
        }
    }

    return true;
}

void SaveCompressedBlockData(BC6H_Encode_local *BC6H_data,
    CGU_INT oEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
    CGU_INT iIndices[2][MAX_SUBSET_SIZE],
    CGU_INT8 max_subsets,
    CGU_INT8 mode)
{
    BC6H_data->m_mode = mode;
    BC6H_data->index++;

    // Save the data to output
    BC6H_data->rw = oEndPoints[0][0][0]; // rw
    BC6H_data->gw = oEndPoints[0][0][1]; // gw
    BC6H_data->bw = oEndPoints[0][0][2]; // bw
    BC6H_data->rx = oEndPoints[0][1][0]; // rx
    BC6H_data->gx = oEndPoints[0][1][1]; // gx
    BC6H_data->bx = oEndPoints[0][1][2]; // bx

    if (max_subsets > 1)
    {
        // Save the data to output
        BC6H_data->ry = oEndPoints[1][0][0]; // ry
        BC6H_data->gy = oEndPoints[1][0][1]; // gy
        BC6H_data->by = oEndPoints[1][0][2]; // by
        BC6H_data->rz = oEndPoints[1][1][0]; // rz
        BC6H_data->gz = oEndPoints[1][1][1]; // gz
        BC6H_data->bz = oEndPoints[1][1][2]; // bz
    }

    // Map our two subset Indices for the shape to output 4x4 block
    CGU_INT pos[2] = { 0,0 };
    CGU_INT asubset;
    for (CGU_INT i = 0; i < MAX_SUBSET_SIZE; i++)
    {
        if (max_subsets > 1)
            asubset = BC6_PARTITIONS[BC6H_data->d_shape_index][i]; // Two region shapes 
        else
            asubset = 0; // One region shapes 
        BC6H_data->indices16[i] = (CGU_UINT8)iIndices[asubset][pos[asubset]];
        pos[asubset]++;
    }

}

CGU_FLOAT CalcOneRegionEndPtsError(BC6H_Encode_local *BC6H_data, CGU_FLOAT fEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], CGU_INT shape_indices[MAX_SUBSETS][MAX_SUBSET_SIZE])
{
    CGU_FLOAT error = 0;

    for (CGU_INT i = 0; i < MAX_SUBSET_SIZE; i++)
    {
        for (CGU_INT m = 0; m < MAX_END_POINTS; m++)
        {
            for (CGU_INT n = 0; n < NCHANNELS; n++)
            {
                CGU_FLOAT calencpts = fEndPoints[0][m][n] + (abs(fEndPoints[0][m][n] - fEndPoints[0][m][n]) * (shape_indices[0][i] / 15));
                error += abs(BC6H_data->din[i][n] - calencpts);
            }
        }
    }

    return error;
}

void ReIndexShapef(BC6H_Encode_local *BC6H_data, CGU_INT shape_indices[MAX_SUBSETS][MAX_SUBSET_SIZE])
{
    CGU_FLOAT error = 0;
    CGU_FLOAT bestError;
    CGU_INT bestIndex = 0;
    CGU_INT sub0index = 0;
    CGU_INT sub1index = 0;
    CGU_INT MaxPallet;
    CGU_INT region = (BC6H_data->region - 1);

    if (region == 0)
        MaxPallet = 16;
    else
        MaxPallet = 8;

    CGU_UINT8 isSet = 0;
    for (CGU_INT i = 0; i < MAX_SUBSET_SIZE; i++)
    {
        // subset 0 or subset 1
        if (region)
            isSet = BC6_PARTITIONS[BC6H_data->d_shape_index][i];

        if (isSet)
        {
            bestError = CMP_HALF_MAX;
            bestIndex = 0;

            // For two shape regions max Pallet is 8
            for (CGU_INT j = 0; j < MaxPallet; j++)
            {
                // Calculate error from original
                error = abs(BC6H_data->din[i][0] - BC6H_data->Paletef[1][j].x) +
                    abs(BC6H_data->din[i][1] - BC6H_data->Paletef[1][j].y) +
                    abs(BC6H_data->din[i][2] - BC6H_data->Paletef[1][j].z);
                if (error < bestError)
                {
                    bestError = error;
                    bestIndex = j;
                }
            }

            shape_indices[1][sub1index] = bestIndex;
            sub1index++;
        }
        else
        {
            // This is shared for one or two shape regions max Pallet either 16 or 8
            bestError = CMP_FLOAT_MAX;
            bestIndex = 0;

            for (CGU_INT j = 0; j < MaxPallet; j++)
            {
                // Calculate error from original
                error = abs(BC6H_data->din[i][0] - BC6H_data->Paletef[0][j].x) +
                    abs(BC6H_data->din[i][1] - BC6H_data->Paletef[0][j].y) +
                    abs(BC6H_data->din[i][2] - BC6H_data->Paletef[0][j].z);
                if (error < bestError)
                {
                    bestError = error;
                    bestIndex = j;
                }
            }

            shape_indices[0][sub0index] = bestIndex;
            sub0index++;
        }
    }

}

CGU_INT Unquantize(CGU_INT comp, unsigned char uBitsPerComp, CGU_BOOL bSigned)
{
    CGU_INT unq = 0, s = 0;
    if (bSigned)
    {
        if (uBitsPerComp >= 16)
        {
            unq = comp;
        }
        else
        {
            if (comp < 0)
            {
                s = 1;
                comp = -comp;
            }

            if (comp == 0) unq = 0;
            else if (comp >= ((1 << (uBitsPerComp - 1)) - 1)) unq = 0x7FFF;
            else unq = ((comp << 15) + 0x4000) >> (uBitsPerComp - 1);

            if (s) unq = -unq;
        }
    }
    else
    {
        if (uBitsPerComp >= 15) unq = comp;
        else if (comp == 0) unq = 0;
        else if (comp == ((1 << uBitsPerComp) - 1)) unq = 0xFFFF;
        else unq = ((comp << 16) + 0x8000) >> uBitsPerComp;
    }

    return unq;
}

CGU_INT finish_unquantizeF16(CGU_INT q, CGU_BOOL isSigned)
{
    // Is it F16 Signed else F16 Unsigned
    if (isSigned)
        return (q < 0) ? -(((-q) * 31) >> 5) : (q * 31) >> 5;       // scale the magnitude by 31/32
    else
        return (q * 31) >> 6;                                       // scale the magnitude by 31/64

                                                                    // Note for Undefined we should return q as is
}

// decompress endpoints
void decompress_endpoints1(BC6H_Encode_local * bc6h_format, CGU_INT oEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], CGU_FLOAT outf[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], CGU_INT mode)
{
    CGU_INT i;
    CGU_INT t;
    CGU_FLOAT out[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];

    if (bc6h_format->issigned)
    {
        if (bc6h_format->istransformed)
        {
            for (i = 0; i < NCHANNELS; i++)
            {
                out[0][0][i] = (CGU_FLOAT)SIGN_EXTEND_TYPELESS(oEndPoints[0][0][i], ModePartition[mode].nbits);

                t = SIGN_EXTEND_TYPELESS(oEndPoints[0][1][i], ModePartition[mode].prec[i]); //C_RED
                t = (t + oEndPoints[0][0][i]) & MASK(ModePartition[mode].nbits);
                out[0][1][i] = (CGU_FLOAT)SIGN_EXTEND_TYPELESS(t, ModePartition[mode].nbits);

                // Unquantize all points to nbits
                out[0][0][i] = (CGU_FLOAT)Unquantize((int)out[0][0][i], (unsigned char)ModePartition[mode].nbits, false);
                out[0][1][i] = (CGU_FLOAT)Unquantize((int)out[0][1][i], (unsigned char)ModePartition[mode].nbits, false);

                // F16 format
                outf[0][0][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[0][0][i], false);
                outf[0][1][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[0][1][i], false);
            }
        }
        else
        {
            for (i = 0; i < NCHANNELS; i++)
            {
                out[0][0][i] = (CGU_FLOAT)SIGN_EXTEND_TYPELESS(oEndPoints[0][0][i], ModePartition[mode].nbits);
                out[0][1][i] = (CGU_FLOAT)SIGN_EXTEND_TYPELESS(oEndPoints[0][1][i], ModePartition[mode].prec[i]);

                // Unquantize all points to nbits
                out[0][0][i] = (CGU_FLOAT)Unquantize((int)out[0][0][i], (unsigned char)ModePartition[mode].nbits, false);
                out[0][1][i] = (CGU_FLOAT)Unquantize((int)out[0][1][i], (unsigned char)ModePartition[mode].nbits, false);

                // F16 format
                outf[0][0][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[0][0][i], false);
                outf[0][1][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[0][1][i], false);
            }
        }

    }
    else
    {
        if (bc6h_format->istransformed)
        {
            for (i = 0; i < NCHANNELS; i++)
            {
                out[0][0][i] = (CGU_FLOAT)oEndPoints[0][0][i];
                t = SIGN_EXTEND_TYPELESS(oEndPoints[0][1][i], ModePartition[mode].prec[i]);
                out[0][1][i] = (CGU_FLOAT)((t + oEndPoints[0][0][i]) & MASK(ModePartition[mode].nbits));

                // Unquantize all points to nbits
                out[0][0][i] = (CGU_FLOAT)Unquantize((int)out[0][0][i], (unsigned char)ModePartition[mode].nbits, false);
                out[0][1][i] = (CGU_FLOAT)Unquantize((int)out[0][1][i], (unsigned char)ModePartition[mode].nbits, false);

                // F16 format
                outf[0][0][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[0][0][i], false);
                outf[0][1][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[0][1][i], false);
            }
        }
        else
        {
            for (i = 0; i < NCHANNELS; i++)
            {
                out[0][0][i] = (CGU_FLOAT)oEndPoints[0][0][i];
                out[0][1][i] = (CGU_FLOAT)oEndPoints[0][1][i];

                // Unquantize all points to nbits
                out[0][0][i] = (CGU_FLOAT)Unquantize((int)out[0][0][i], (unsigned char)ModePartition[mode].nbits, false);
                out[0][1][i] = (CGU_FLOAT)Unquantize((int)out[0][1][i], (unsigned char)ModePartition[mode].nbits, false);

                // F16 format
                outf[0][0][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[0][0][i], false);
                outf[0][1][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[0][1][i], false);
            }
        }
    }
}

void decompress_endpoints2(BC6H_Encode_local * bc6h_format, CGU_INT oEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], CGU_FLOAT outf[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], CGU_INT mode)
{
    CGU_INT i;
    CGU_INT t;
    CGU_FLOAT out[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];

    if (bc6h_format->issigned)
    {
        if (bc6h_format->istransformed)
        {
            for (i = 0; i < NCHANNELS; i++)
            {
                // get the quantized values 
                out[0][0][i] = (CGU_FLOAT)SIGN_EXTEND_TYPELESS(oEndPoints[0][0][i], ModePartition[mode].nbits);

                t = SIGN_EXTEND_TYPELESS(oEndPoints[0][1][i], ModePartition[mode].prec[i]);
                t = (t + oEndPoints[0][0][i]) & MASK(ModePartition[mode].nbits);
                out[0][1][i] = (CGU_FLOAT)SIGN_EXTEND_TYPELESS(t, ModePartition[mode].nbits);

                t = SIGN_EXTEND_TYPELESS(oEndPoints[1][0][i], ModePartition[mode].prec[i]);
                t = (t + oEndPoints[0][0][i]) & MASK(ModePartition[mode].nbits);
                out[1][0][i] = (CGU_FLOAT)SIGN_EXTEND_TYPELESS(t, ModePartition[mode].nbits);

                t = SIGN_EXTEND_TYPELESS(oEndPoints[1][1][i], ModePartition[mode].prec[i]);
                t = (t + oEndPoints[0][0][i]) & MASK(ModePartition[mode].nbits);
                out[1][1][i] = (CGU_FLOAT)SIGN_EXTEND_TYPELESS(t, ModePartition[mode].nbits);

                // Unquantize all points to nbits 
                out[0][0][i] = (CGU_FLOAT)Unquantize((int)out[0][0][i], (unsigned char)ModePartition[mode].nbits, true);
                out[0][1][i] = (CGU_FLOAT)Unquantize((int)out[0][1][i], (unsigned char)ModePartition[mode].nbits, true);
                out[1][0][i] = (CGU_FLOAT)Unquantize((int)out[1][0][i], (unsigned char)ModePartition[mode].nbits, true);
                out[1][1][i] = (CGU_FLOAT)Unquantize((int)out[1][1][i], (unsigned char)ModePartition[mode].nbits, true);

                // F16 format
                outf[0][0][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[0][0][i], true);
                outf[0][1][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[0][1][i], true);
                outf[1][0][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[1][0][i], true);
                outf[1][1][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[1][1][i], true);

            }
        }
        else
        {
            for (i = 0; i < NCHANNELS; i++)
            {
                out[0][0][i] = (CGU_FLOAT)SIGN_EXTEND_TYPELESS(oEndPoints[0][0][i], ModePartition[mode].nbits);
                out[0][1][i] = (CGU_FLOAT)SIGN_EXTEND_TYPELESS(oEndPoints[0][1][i], ModePartition[mode].prec[i]);
                out[1][0][i] = (CGU_FLOAT)SIGN_EXTEND_TYPELESS(oEndPoints[1][0][i], ModePartition[mode].prec[i]);
                out[1][1][i] = (CGU_FLOAT)SIGN_EXTEND_TYPELESS(oEndPoints[1][1][i], ModePartition[mode].prec[i]);

                // Unquantize all points to nbits
                out[0][0][i] = (CGU_FLOAT)Unquantize((int)out[0][0][i], (unsigned char)ModePartition[mode].nbits, false);
                out[0][1][i] = (CGU_FLOAT)Unquantize((int)out[0][1][i], (unsigned char)ModePartition[mode].nbits, false);
                out[1][0][i] = (CGU_FLOAT)Unquantize((int)out[1][0][i], (unsigned char)ModePartition[mode].nbits, false);
                out[1][1][i] = (CGU_FLOAT)Unquantize((int)out[1][1][i], (unsigned char)ModePartition[mode].nbits, false);

                // nbits to F16 format
                outf[0][0][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[0][0][i], false);
                outf[0][1][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[0][1][i], false);
                outf[1][0][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[1][0][i], false);
                outf[1][1][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[1][1][i], false);
            }
        }

    }
    else
    {
        if (bc6h_format->istransformed)
        {
            for (i = 0; i < NCHANNELS; i++)
            {
                out[0][0][i] = (CGU_FLOAT)oEndPoints[0][0][i];
                t = SIGN_EXTEND_TYPELESS(oEndPoints[0][1][i], ModePartition[mode].prec[i]);
                out[0][1][i] = (CGU_FLOAT)((t + oEndPoints[0][0][i]) & MASK(ModePartition[mode].nbits));

                t = SIGN_EXTEND_TYPELESS(oEndPoints[1][0][i], ModePartition[mode].prec[i]);
                out[1][0][i] = (CGU_FLOAT)((t + oEndPoints[0][0][i]) & MASK(ModePartition[mode].nbits));

                t = SIGN_EXTEND_TYPELESS(oEndPoints[1][1][i], ModePartition[mode].prec[i]);
                out[1][1][i] = (CGU_FLOAT)((t + oEndPoints[0][0][i]) & MASK(ModePartition[mode].nbits));

                // Unquantize all points to nbits
                out[0][0][i] = (CGU_FLOAT)Unquantize((int)out[0][0][i], (unsigned char)ModePartition[mode].nbits, false);
                out[0][1][i] = (CGU_FLOAT)Unquantize((int)out[0][1][i], (unsigned char)ModePartition[mode].nbits, false);
                out[1][0][i] = (CGU_FLOAT)Unquantize((int)out[1][0][i], (unsigned char)ModePartition[mode].nbits, false);
                out[1][1][i] = (CGU_FLOAT)Unquantize((int)out[1][1][i], (unsigned char)ModePartition[mode].nbits, false);

                // nbits to F16 format
                outf[0][0][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[0][0][i], false);
                outf[0][1][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[0][1][i], false);
                outf[1][0][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[1][0][i], false);
                outf[1][1][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[1][1][i], false);

            }
        }
        else
        {
            for (i = 0; i < NCHANNELS; i++)
            {
                out[0][0][i] = (CGU_FLOAT)oEndPoints[0][0][i];
                out[0][1][i] = (CGU_FLOAT)oEndPoints[0][1][i];
                out[1][0][i] = (CGU_FLOAT)oEndPoints[1][0][i];
                out[1][1][i] = (CGU_FLOAT)oEndPoints[1][1][i];

                // Unquantize all points to nbits
                out[0][0][i] = (CGU_FLOAT)Unquantize((int)out[0][0][i], (unsigned char)ModePartition[mode].nbits, false);
                out[0][1][i] = (CGU_FLOAT)Unquantize((int)out[0][1][i], (unsigned char)ModePartition[mode].nbits, false);
                out[1][0][i] = (CGU_FLOAT)Unquantize((int)out[1][0][i], (unsigned char)ModePartition[mode].nbits, false);
                out[1][1][i] = (CGU_FLOAT)Unquantize((int)out[1][1][i], (unsigned char)ModePartition[mode].nbits, false);

                // nbits to F16 format
                outf[0][0][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[0][0][i], false);
                outf[0][1][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[0][1][i], false);
                outf[1][0][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[1][0][i], false);
                outf[1][1][i] = (CGU_FLOAT)finish_unquantizeF16((int)out[1][1][i], false);
            }
        }
    }
}

// decompress endpoints
static void decompress_endpts(const CGU_INT in[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], CGU_INT out[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], const CGU_INT mode, CGU_BOOL issigned)
{

    if (ModePartition[mode].transformed)
    {
        for (CGU_INT i = 0; i < 3; ++i)
        {
            R_0(out) = issigned ? SIGN_EXTEND_TYPELESS(R_0(in), ModePartition[mode].IndexPrec) : R_0(in);
            CGU_INT t;
            t = SIGN_EXTEND_TYPELESS(R_1(in), ModePartition[mode].prec[i]);
            t = (t + R_0(in)) & MASK(ModePartition[mode].nbits);
            R_1(out) = issigned ? SIGN_EXTEND_TYPELESS(t, ModePartition[mode].nbits) : t;

            t = SIGN_EXTEND_TYPELESS(R_2(in), ModePartition[mode].prec[i]);
            t = (t + R_0(in)) & MASK(ModePartition[mode].nbits);
            R_2(out) = issigned ? SIGN_EXTEND_TYPELESS(t, ModePartition[mode].nbits) : t;

            t = SIGN_EXTEND_TYPELESS(R_3(in), ModePartition[mode].prec[i]);
            t = (t + R_0(in)) & MASK(ModePartition[mode].nbits);
            R_3(out) = issigned ? SIGN_EXTEND_TYPELESS(t, ModePartition[mode].nbits) : t;
        }
    }
    else
    {
        for (CGU_INT i = 0; i < 3; ++i)
        {
            R_0(out) = issigned ? SIGN_EXTEND_TYPELESS(R_0(in), ModePartition[mode].nbits) : R_0(in);
            R_1(out) = issigned ? SIGN_EXTEND_TYPELESS(R_1(in), ModePartition[mode].prec[i]) : R_1(in);
            R_2(out) = issigned ? SIGN_EXTEND_TYPELESS(R_2(in), ModePartition[mode].prec[i]) : R_2(in);
            R_3(out) = issigned ? SIGN_EXTEND_TYPELESS(R_3(in), ModePartition[mode].prec[i]) : R_3(in);
        }
    }
}

// endpoints fit only if the compression was lossless
static CGU_BOOL endpts_fit(const CGU_INT orig[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], const CGU_INT compressed[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], const CGU_INT mode, CGU_INT max_subsets, CGU_BOOL issigned)
{
    CGU_INT uncompressed[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];

    decompress_endpts(compressed, uncompressed, mode, issigned);

    for (CGU_INT j = 0; j < max_subsets; ++j)
        for (CGU_INT i = 0; i < 3; ++i)
        {
            if (orig[j][0][i] != uncompressed[j][0][i]) return false;
            if (orig[j][1][i] != uncompressed[j][1][i]) return false;
        }

    return true;
}

//todo: check overflow
CGU_INT QuantizeToInt(short value, CGU_INT prec, CGU_BOOL signedfloat16)
{

    if (prec <= 1) return 0;
    CGU_BOOL negvalue = false;

    // move data to use extra bits for processing
    CGU_INT ivalue = value;

    if (signedfloat16)
    {
        if (value < 0)
        {
            negvalue = true;
            value = -value;
        }
        prec--;
    }
    else
    {
        // clamp -ve
        if (value < 0)
            value = 0;
    }

    CGU_INT iQuantized;
    CGU_INT bias = (prec > 10 && prec != 16) ? ((1 << (prec - 11)) - 1) : 0;
    bias = (prec == 16) ? 15 : bias;

    iQuantized = ((ivalue << prec) + bias) / (FLT16_MAX + 1);

    return (negvalue ? -iQuantized : iQuantized);
}

//todo: checkoverflow
void QuantizeEndPointToF16Prec(CGU_FLOAT EndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], CGU_INT iEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], CGU_INT max_subsets, CGU_INT prec, CGU_BOOL isSigned)
{

    for (CGU_INT subset = 0; subset < max_subsets; ++subset)
    {
        iEndPoints[subset][0][0] = QuantizeToInt((short)EndPoints[subset][0][0], prec, isSigned);    // A.Red
        iEndPoints[subset][0][1] = QuantizeToInt((short)EndPoints[subset][0][1], prec, isSigned);    // A.Green
        iEndPoints[subset][0][2] = QuantizeToInt((short)EndPoints[subset][0][2], prec, isSigned);    // A.Blue
        iEndPoints[subset][1][0] = QuantizeToInt((short)EndPoints[subset][1][0], prec, isSigned);    // B.Red
        iEndPoints[subset][1][1] = QuantizeToInt((short)EndPoints[subset][1][1], prec, isSigned);    // B.Green
        iEndPoints[subset][1][2] = QuantizeToInt((short)EndPoints[subset][1][2], prec, isSigned);    // B.Blue
    }
}

CGU_FLOAT  EncodePattern(BC6H_Encode_local *BC6H_data, CGU_FLOAT  error)
{
    CGU_INT8        max_subsets = BC6H_data->region;

    // now we have input colors (in), output colors (outB) mapped to a line of ends (EndPoints)
    // and a set of colors on the line equally spaced (indexedcolors)
    // Lets assign indices

    //CGU_FLOAT SrcEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];                  // temp endpoints used during calculations

    // Quantize the EndPoints 
    CGU_INT F16EndPoints[MAX_BC6H_MODES + 1][MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];                    // temp endpoints used during calculations
    CGU_INT quantEndPoints[MAX_BC6H_MODES + 1][MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];                    // endpoints to save for a given mode

                                                                                                                    // ModePartition[] starts from 1 to 14
                                                                                                                    // If we have a shape pattern set the loop to check modes from 1 to 10 else from 11 to 14
                                                                                                                    // of the ModePartition table
    CGU_INT     min_mode = (BC6H_data->region == 2) ? 1 : 11;
    CGU_INT     max_mode = (BC6H_data->region == 2) ? MAX_TWOREGION_MODES : MAX_BC6H_MODES;

    CGU_BOOL    fits[15];
    memset((CGU_UINT8 *)fits, 0, sizeof(fits));

    CGU_INT bestFit = 0;
    CGU_INT bestEndpointMode = 0;
    CGU_FLOAT bestError = CMP_FLOAT_MAX;
    CGU_FLOAT bestEndpointsErr = CMP_FLOAT_MAX;
    CGU_FLOAT endPointErr = 0;

    // Try Optimization for the Mode
    CGU_FLOAT       best_EndPoints[MAX_BC6H_MODES + 1][MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];
    CGU_INT         best_Indices[MAX_BC6H_MODES + 1][MAX_SUBSETS][MAX_SUBSET_SIZE];
    CGU_FLOAT      opt_toterr[MAX_BC6H_MODES + 1] = { 0 };

    memset((CGU_UINT8 *)opt_toterr, 0, sizeof(opt_toterr));

    CGU_INT numfits = 0;
    //
    // Notes;  Only the endpoints are varying; the indices stay fixed in values!
    // so to optimize which mode we need only check the endpoints error against our original to pick the mode to save
    //
    for (CGU_INT modes = min_mode; modes <= max_mode; ++modes)
    {
        memcpy((CGU_UINT8 *)best_EndPoints[modes], (CGU_UINT8 *)BC6H_data->fEndPoints, sizeof(BC6H_data->fEndPoints));
        memcpy((CGU_UINT8 *)best_Indices[modes]  , (CGU_UINT8 *)BC6H_data->shape_indices, sizeof(BC6H_data->shape_indices));

        {
            QuantizeEndPointToF16Prec(best_EndPoints[modes], F16EndPoints[modes], max_subsets, ModePartition[ModeFitOrder[modes]].nbits, BC6H_data->issigned);
        }

        // Indices data to save for given mode
        SwapIndices(F16EndPoints[modes], best_Indices[modes], BC6H_data->entryCount, max_subsets, ModeFitOrder[modes], BC6H_data->d_shape_index);
        CGU_BOOL transformfit = TransformEndPoints(BC6H_data, F16EndPoints[modes], quantEndPoints[modes], max_subsets, ModeFitOrder[modes]);
        fits[modes] = endpts_fit(F16EndPoints[modes], quantEndPoints[modes], ModeFitOrder[modes], max_subsets, BC6H_data->issigned);

        if (fits[modes] && transformfit)
        {
            numfits++;

            // The new compressed end points fit the mode
            // recalculate the error for this mode with a new set of indices
            // since we have shifted the end points from what we origially calc
            // from the find_bestpattern
            CGU_FLOAT uncompressed[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];
            if (BC6H_data->region == 1)
                decompress_endpoints1(BC6H_data, quantEndPoints[modes], uncompressed, ModeFitOrder[modes]);
            else
                decompress_endpoints2(BC6H_data, quantEndPoints[modes], uncompressed, ModeFitOrder[modes]);
            // Takes the end points and creates a pallet of colors
            // based on preset weights along a vector formed by the two end points
            palitizeEndPointsF(BC6H_data, uncompressed);

            // Once we have the pallet - recalculate the optimal indices using the pallet
            // and the original image data stored in BC6H_data.din[]
            if (!BC6H_data->issigned)
                ReIndexShapef(BC6H_data, best_Indices[modes]);

            // Calculate the error of the new tile vs the old tile data
            opt_toterr[modes] = CalcShapeError(BC6H_data, uncompressed, true);
            if (BC6H_data->region == 1)
            {
                endPointErr = CalcOneRegionEndPtsError(BC6H_data, uncompressed, best_Indices[modes]);
                if (endPointErr < bestEndpointsErr)
                {
                    bestEndpointsErr = endPointErr;
                    bestEndpointMode = modes;
                }
            }

            CGU_BOOL transformFit = true;
            // Save hold this mode fit data if its better than the last one checked.
            if (opt_toterr[modes] < bestError)
            {
                if (!BC6H_data->issigned)
                {
                    QuantizeEndPointToF16Prec(uncompressed, F16EndPoints[modes], max_subsets, ModePartition[ModeFitOrder[modes]].nbits, BC6H_data->issigned);
                    SwapIndices(F16EndPoints[modes], best_Indices[modes], BC6H_data->entryCount, max_subsets, ModeFitOrder[modes], BC6H_data->d_shape_index);
                    transformFit = TransformEndPoints(BC6H_data, F16EndPoints[modes], quantEndPoints[modes], max_subsets, ModeFitOrder[modes]);
                }
                if (transformFit)
                {
                    if (BC6H_data->region == 1)
                    {
                        bestFit = (modes == bestEndpointMode) ? modes : ((modes < bestEndpointMode) ? modes : bestEndpointMode);
                    }
                    else
                    {
                        bestFit = modes;
                    }
                    bestError = opt_toterr[bestFit];
                    error = bestError;
                }
            }

        }
    }

    if (numfits > 0)
    {
        SaveCompressedBlockData(BC6H_data, quantEndPoints[bestFit], best_Indices[bestFit], max_subsets, ModeFitOrder[bestFit]);
        return error;
    }

    // Should not get here!
    return error;
}

void CompressBlockBC6_Internal(CMP_GLOBAL  unsigned char*outdata, 
                               CGU_UINT32 destIdx,
                               BC6H_Encode_local * BC6HEncode_local,
                               CMP_GLOBAL const BC6H_Encode *BC6HEncode)
{
    //printf("---SRC---\n");
    //CGU_UINT8    blkindex = 0;
    //CGU_UINT8    srcindex = 0;
    //for ( CGU_INT32 j = 0; j < 16; j++) {
    //    printf("%5.0f,",BC6HEncode_local->din[j][0]);// R
    //    printf("%5.0f,",BC6HEncode_local->din[j][1]);// G
    //    printf("%5.0f,",BC6HEncode_local->din[j][2]);// B
    //    printf("%5.0f\n,",BC6HEncode_local->din[j][3]);// No Alpha
    //}

    CGU_UINT8 Cmp_Red_Block[16] = { 0xc2,0x7b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0x03,0x00,0x00,0x00,0x00,0x00 };

    CGU_FLOAT bestError = CMP_FLOAT_MAX;
    CGU_FLOAT error = CMP_FLOAT_MAX;
    CGU_INT8 bestShape = 0;
    CGU_FLOAT quality = BC6HEncode->m_quality;
    BC6HEncode_local->issigned = BC6HEncode->m_isSigned;
    // run through no partition first
    error = FindBestPattern(BC6HEncode_local, false, 0, quality);
    if (error < bestError)
    {
        bestError = error;
        bestShape = -1;

        memcpy((CGU_UINT8 *)BC6HEncode_local->cur_best_shape_indices,(CGU_UINT8 *) BC6HEncode_local->shape_indices, sizeof(BC6HEncode_local->shape_indices));
        memcpy((CGU_UINT8 *)BC6HEncode_local->cur_best_partition    ,(CGU_UINT8 *) BC6HEncode_local->partition, sizeof(BC6HEncode_local->partition));
        memcpy((CGU_UINT8 *)BC6HEncode_local->cur_best_fEndPoints   ,(CGU_UINT8 *) BC6HEncode_local->fEndPoints, sizeof(BC6HEncode_local->fEndPoints));
        memcpy((CGU_UINT8 *)BC6HEncode_local->cur_best_entryCount   ,(CGU_UINT8 *) BC6HEncode_local->entryCount, sizeof(BC6HEncode_local->entryCount));
        BC6HEncode_local->d_shape_index = bestShape;
    }


    // run through 32 possible partition set
    for (CGU_INT8 shape = 0; shape < MAX_BC6H_PARTITIONS; shape++)
    {
        error = FindBestPattern(BC6HEncode_local, true, shape, quality);
        if (error < bestError)
        {
            bestError = error;
            bestShape = shape;

            memcpy((CGU_UINT8 *)BC6HEncode_local->cur_best_shape_indices, (CGU_UINT8 *)BC6HEncode_local->shape_indices, sizeof(BC6HEncode_local->shape_indices));
            memcpy((CGU_UINT8 *)BC6HEncode_local->cur_best_partition    , (CGU_UINT8 *)BC6HEncode_local->partition, sizeof(BC6HEncode_local->partition));
            memcpy((CGU_UINT8 *)BC6HEncode_local->cur_best_fEndPoints   , (CGU_UINT8 *)BC6HEncode_local->fEndPoints, sizeof(BC6HEncode_local->fEndPoints));
            memcpy((CGU_UINT8 *)BC6HEncode_local->cur_best_entryCount   , (CGU_UINT8 *)BC6HEncode_local->entryCount, sizeof(BC6HEncode_local->entryCount));
            BC6HEncode_local->d_shape_index = bestShape;
        }
        else
        {
            if (bestShape != -1)
            {
                BC6HEncode_local->d_shape_index = bestShape;
                memcpy((CGU_UINT8 *)BC6HEncode_local->shape_indices, (CGU_UINT8 *)BC6HEncode_local->cur_best_shape_indices, sizeof(BC6HEncode_local->shape_indices));
                memcpy((CGU_UINT8 *)BC6HEncode_local->partition    , (CGU_UINT8 *)BC6HEncode_local->cur_best_partition, sizeof(BC6HEncode_local->partition));
                memcpy((CGU_UINT8 *)BC6HEncode_local->fEndPoints   , (CGU_UINT8 *)BC6HEncode_local->cur_best_fEndPoints, sizeof(BC6HEncode_local->fEndPoints));
                memcpy((CGU_UINT8 *)BC6HEncode_local->entryCount   , (CGU_UINT8 *)BC6HEncode_local->cur_best_entryCount, sizeof(BC6HEncode_local->entryCount));
            }
        }
    }

    bestError = EncodePattern(BC6HEncode_local, bestError);


    // used for debugging modes, set the value you want to debug with
    if (BC6HEncode_local->m_mode != 0)
    {
        // do final encoding and save to output block
        SaveDataBlock(BC6HEncode_local, &outdata[destIdx]);
    }
   else
   {
       for (CGU_INT i = 0; i < 16; i++)
           outdata[destIdx + i] = Cmp_Red_Block[i];
   }
}

//============================================== USER INTERFACES ========================================================

#ifndef ASPM_GPU
#ifndef ASPM
//======================= DECOMPRESS =========================================
using namespace std;

static AMD_BC6H_Format extract_format(const CGU_UINT8 in[COMPRESSED_BLOCK_SIZE])
{
    AMD_BC6H_Format bc6h_format;
    unsigned short decvalue;
    CGU_UINT8 iData[COMPRESSED_BLOCK_SIZE];
    memcpy(iData,in,COMPRESSED_BLOCK_SIZE);

    memset(&bc6h_format,0,sizeof(AMD_BC6H_Format));

    // 2 bit mode has Mode bit:2 = 0 and mode bits:1 = 0 or 1
    // 5 bit mode has Mode bit:2 = 1 
    if ((in[0]&0x02) > 0)
    {
        decvalue = (in[0]&0x1F);    // first five bits
    }
    else
    {
        decvalue = (in[0]&0x01);    // first two bits
    }

    BitHeader header(in,16);
    
    switch (decvalue)
    {
    case 0x00:
                bc6h_format.m_mode          = 1; // 10:5:5:5
                bc6h_format.wBits           = 10;
                bc6h_format.tBits[C_RED]    = 5;
                bc6h_format.tBits[C_GREEN]  = 5;
                bc6h_format.tBits[C_BLUE]   = 5;
                bc6h_format.rw = header.getvalue(5 ,10);            // 10:   rw[9:0] 
                bc6h_format.rx = header.getvalue(35,5);             // 5:    rx[4:0]
                bc6h_format.ry = header.getvalue(65,5);             // 5:    ry[4:0]
                bc6h_format.rz = header.getvalue(71,5);             // 5:    rz[4:0]
                bc6h_format.gw = header.getvalue(15,10);            // 10:   gw[9:0]
                bc6h_format.gx = header.getvalue(45,5);             // 5:    gx[4:0]
                bc6h_format.gy = header.getvalue(41,4) |            // 5:    gy[3:0]
                                (header.getvalue(2,1) << 4);        //       gy[4]
                bc6h_format.gz = header.getvalue(51,4) |            // 5:    gz[3:0]
                                (header.getvalue(40,1) << 4);       //       gz[4]
                bc6h_format.bw = header.getvalue(25,10);            // 10:   bw[9:0]
                bc6h_format.bx = header.getvalue(55,5);             // 5:    bx[4:0]
                bc6h_format.by = header.getvalue(61,4) |            // 5:    by[3:0]
                                (header.getvalue(3,1) << 4);        //       by[4]
                bc6h_format.bz = header.getvalue(50,1) |            // 5:    bz[0]
                                (header.getvalue(60,1) << 1) |      //       bz[1]
                                (header.getvalue(70,1) << 2) |      //       bz[2]
                                (header.getvalue(76,1) << 3) |      //       bz[3]
                                (header.getvalue(4 ,1) << 4);       //       bz[4]
                break;
    case 0x01:
                bc6h_format.m_mode          = 2;    // 7:6:6:6
                bc6h_format.wBits           = 7;
                bc6h_format.tBits[C_RED]    = 6;
                bc6h_format.tBits[C_GREEN]  = 6;
                bc6h_format.tBits[C_BLUE]   = 6;
                bc6h_format.rw = header.getvalue(5,7);               // 7:    rw[6:0] 
                bc6h_format.rx = header.getvalue(35,6);              // 6:    rx[5:0]
                bc6h_format.ry = header.getvalue(65,6);              // 6:    ry[5:0]
                bc6h_format.rz = header.getvalue(71,6);              // 6:    rz[5:0]
                bc6h_format.gw = header.getvalue(15,7);              // 7:    gw[6:0]
                bc6h_format.gx = header.getvalue(45,6);              // 6:    gx[5:0]
                bc6h_format.gy = header.getvalue(41,4)    |          // 6:    gy[3:0]
                                (header.getvalue(24,1) << 4) |       //       gy[4]
                                (header.getvalue(2,1)   << 5);       //       gy[5]
                bc6h_format.gz = header.getvalue(51,4)    |          // 6:    gz[3:0]
                                (header.getvalue(3,1) << 4) |        //       gz[4]
                                (header.getvalue(4,1) << 5);         //       gz[5]
                bc6h_format.bw = header.getvalue(25,7);              // 7:    bw[6:0]
                bc6h_format.bx = header.getvalue(55,6);              // 6:    bx[5:0]
                bc6h_format.by = header.getvalue(61,4)    |          // 6:    by[3:0]
                                (header.getvalue(14,1) << 4) |       //       by[4]
                                (header.getvalue(22,1) << 5);        //       by[5]
                bc6h_format.bz = header.getvalue(12,1)    |          // 6:    bz[0]
                                (header.getvalue(13,1) << 1) |       //       bz[1]
                                (header.getvalue(23,1) << 2) |       //       bz[2]
                                (header.getvalue(32,1) << 3) |       //       bz[3]
                                (header.getvalue(34,1) << 4) |       //       bz[4]
                                (header.getvalue(33,1) << 5);        //       bz[5]
                break;
    case 0x02:
                bc6h_format.m_mode          = 3;  // 11:5:4:4
                bc6h_format.wBits           = 11;
                bc6h_format.tBits[C_RED]    = 5;
                bc6h_format.tBits[C_GREEN]  = 4;
                bc6h_format.tBits[C_BLUE]   = 4;
                bc6h_format.rw = header.getvalue(5,10)  |            //11:    rw[9:0] 
                                (header.getvalue(40,1) << 10);       //       rw[10]
                bc6h_format.rx = header.getvalue(35,5);              // 5:    rx[4:0]
                bc6h_format.ry = header.getvalue(65,5);              // 5:    ry[4:0]
                bc6h_format.rz = header.getvalue(71,5);              // 5:    rz[4:0]
                bc6h_format.gw = header.getvalue(15,10) |            //11:    gw[9:0]
                                (header.getvalue(49,1) << 10);       //       gw[10]
                bc6h_format.gx = header.getvalue(45,4);              //4:     gx[3:0]
                bc6h_format.gy = header.getvalue(41,4);              //4:     gy[3:0]
                bc6h_format.gz = header.getvalue(51,4);              //4:     gz[3:0]
                bc6h_format.bw = header.getvalue(25,10) |            //11:    bw[9:0]
                                (header.getvalue(59,1) << 10);       //       bw[10]
                bc6h_format.bx = header.getvalue(55,4);              //4:     bx[3:0]
                bc6h_format.by = header.getvalue(61,4);              //4:     by[3:0]
                bc6h_format.bz = header.getvalue(50,1) |             //4:     bz[0]
                                (header.getvalue(60,1) << 1) |       //       bz[1]
                                (header.getvalue(70,1) << 2) |       //       bz[2]
                                (header.getvalue(76,1) << 3);        //       bz[3]
                break;
    case 0x06:
                bc6h_format.m_mode          = 4;  // 11:4:5:4
                bc6h_format.wBits           = 11;
                bc6h_format.tBits[C_RED]    = 4;
                bc6h_format.tBits[C_GREEN]  = 5;
                bc6h_format.tBits[C_BLUE]   = 4;
                bc6h_format.rw = header.getvalue(5,10)  |             //11:   rw[9:0] 
                                (header.getvalue(39,1) << 10);        //      rw[10]
                bc6h_format.rx = header.getvalue(35,4);               //4:    rx[3:0]
                bc6h_format.ry = header.getvalue(65,4);               //4:    ry[3:0]
                bc6h_format.rz = header.getvalue(71,4);               //4:    rz[3:0]
                bc6h_format.gw = header.getvalue(15,10) |             //11:   gw[9:0]
                                (header.getvalue(50,1) << 10);        //      gw[10]
                bc6h_format.gx = header.getvalue(45,5);               //5:    gx[4:0]
                bc6h_format.gy = header.getvalue(41,4) |              //5:    gy[3:0]
                                (header.getvalue(75,1) << 4);         //      gy[4]
                bc6h_format.gz = header.getvalue(51,4) |              //5:    gz[3:0]
                                (header.getvalue(40,1) << 4);         //      gz[4]
                bc6h_format.bw = header.getvalue(25,10) |             //11:   bw[9:0]
                                (header.getvalue(59,1) << 10);        //      bw[10]
                bc6h_format.bx = header.getvalue(55,4);               //4:    bx[3:0]
                bc6h_format.by = header.getvalue(61,4);               //4:    by[3:0]
                bc6h_format.bz = header.getvalue(69,1) |              //4:    bz[0]
                                (header.getvalue(60,1) << 1) |        //      bz[1]
                                (header.getvalue(70,1) << 2) |        //      bz[2]
                                (header.getvalue(76,1) << 3);         //      bz[3]
                break;
    case 0x0A:
                bc6h_format.m_mode          = 5; // 11:4:4:5
                bc6h_format.wBits           = 11;
                bc6h_format.tBits[C_RED]    = 4;
                bc6h_format.tBits[C_GREEN]  = 4;
                bc6h_format.tBits[C_BLUE]   = 5;
                bc6h_format.rw = header.getvalue(5,10)  |             //11:   rw[9:0] 
                                (header.getvalue(39,1) << 10);        //      rw[10]
                bc6h_format.rx = header.getvalue(35,4);               //4:    rx[3:0]
                bc6h_format.ry = header.getvalue(65,4);               //4:    ry[3:0]
                bc6h_format.rz = header.getvalue(71,4);               //4:    rz[3:0]
                bc6h_format.gw = header.getvalue(15,10) |             //11:   gw[9:0]
                                (header.getvalue(49,1) << 10);        //      gw[10]
                bc6h_format.gx = header.getvalue(45,4);               //4:    gx[3:0]
                bc6h_format.gy = header.getvalue(41,4);               //4:    gy[3:0]
                bc6h_format.gz = header.getvalue(51,4);               //4:    gz[3:0]
                bc6h_format.bw = header.getvalue(25,10) |             //11:   bw[9:0]
                                (header.getvalue(60,1) << 10);        //      bw[10]
                bc6h_format.bx = header.getvalue(55,5);               //5:    bx[4:0]
                bc6h_format.by = header.getvalue(61,4);               //5:    by[3:0]
                                (header.getvalue(40,1) << 4);         //      by[4]
                bc6h_format.bz = header.getvalue(50,1) |              //5:    bz[0]
                                (header.getvalue(69,1) << 1) |        //      bz[1]
                                (header.getvalue(70,1) << 2) |        //      bz[2]
                                (header.getvalue(76,1) << 3) |        //      bz[3]
                                (header.getvalue(75,1) << 4);         //      bz[4]
                break;
    case 0x0E:
                bc6h_format.m_mode          = 6;  // 9:5:5:5
                bc6h_format.wBits           = 9;
                bc6h_format.tBits[C_RED]    = 5;
                bc6h_format.tBits[C_GREEN]  = 5;
                bc6h_format.tBits[C_BLUE]   = 5;
                bc6h_format.rw = header.getvalue(5,9);                 //9:   rw[8:0] 
                bc6h_format.gw = header.getvalue(15,9);                //9:   gw[8:0]
                bc6h_format.bw = header.getvalue(25,9);                //9:   bw[8:0]
                bc6h_format.rx = header.getvalue(35,5);                //5:   rx[4:0]
                bc6h_format.gx = header.getvalue(45,5);                //5:   gx[4:0]
                bc6h_format.bx = header.getvalue(55,5);                //5:   bx[4:0]
                bc6h_format.ry = header.getvalue(65,5);                //5:   ry[4:0]
                bc6h_format.gy = header.getvalue(41,4) |               //5:   gy[3:0]
                                (header.getvalue(24,1) << 4);          //     gy[4]
                bc6h_format.by = header.getvalue(61,4) |               //5:   by[3:0]
                                (header.getvalue(14,1) << 4);          //     by[4]
                bc6h_format.rz = header.getvalue(71,5);                //5:   rz[4:0]
                bc6h_format.gz = header.getvalue(51,4) |               //5:   gz[3:0]
                                (header.getvalue(40,1) << 4);          //     gz[4]
                bc6h_format.bz = header.getvalue(50,1) |               //5:   bz[0]
                                (header.getvalue(60,1) << 1) |         //     bz[1]
                                (header.getvalue(70,1) << 2) |         //     bz[2]
                                (header.getvalue(76,1) << 3) |         //     bz[3]
                                (header.getvalue(34,1) << 4);          //     bz[4]
                break;
    case 0x12:
                bc6h_format.m_mode          = 7;  // 8:6:5:5
                bc6h_format.wBits           = 8;
                bc6h_format.tBits[C_RED]    = 6;
                bc6h_format.tBits[C_GREEN]  = 5;
                bc6h_format.tBits[C_BLUE]   = 5;
                bc6h_format.rw = header.getvalue(5,8);                 //8:    rw[7:0] 
                bc6h_format.gw = header.getvalue(15,8);                //8:    gw[7:0]
                bc6h_format.bw = header.getvalue(25,8);                //8:    bw[7:0]
                bc6h_format.rx = header.getvalue(35,6);                //6:    rx[5:0]
                bc6h_format.gx = header.getvalue(45,5);                //5:    gx[4:0]
                bc6h_format.bx = header.getvalue(55,5);                //5:    bx[4:0]
                bc6h_format.ry = header.getvalue(65,6);                //6:    ry[5:0]
                bc6h_format.gy = header.getvalue(41,4) |               //5:    gy[3:0]
                                (header.getvalue(24,1) << 4);          //      gy[4]
                bc6h_format.by = header.getvalue(61,4) |               //5:    by[3:0]
                                (header.getvalue(14,1) << 4);          //      by[4]
                bc6h_format.rz = header.getvalue(71,6);                //6:    rz[5:0]
                bc6h_format.gz = header.getvalue(51,4) |               //5:    gz[3:0]
                                (header.getvalue(13,1) << 4);          //      gz[4]
                bc6h_format.bz = header.getvalue(50,1) |               //5:    bz[0]
                                (header.getvalue(60,1) << 1) |         //      bz[1]
                                (header.getvalue(23,1) << 2) |         //      bz[2]
                                (header.getvalue(33,1) << 3) |         //      bz[3]
                                (header.getvalue(34,1) << 4);          //      bz[4]
                break;
    case 0x16:
                bc6h_format.m_mode          = 8;  // 8:5:6:5
                bc6h_format.wBits           = 8;
                bc6h_format.tBits[C_RED]    = 5;
                bc6h_format.tBits[C_GREEN]  = 6;
                bc6h_format.tBits[C_BLUE]   = 5;
                bc6h_format.rw = header.getvalue(5,8);                 //8:    rw[7:0] 
                bc6h_format.gw = header.getvalue(15,8);                //8:    gw[7:0]
                bc6h_format.bw = header.getvalue(25,8);                //8:    bw[7:0]
                bc6h_format.rx = header.getvalue(35,5);                //5:    rx[4:0]
                bc6h_format.gx = header.getvalue(45,6);                //6:    gx[5:0]
                bc6h_format.bx = header.getvalue(55,5);                //5:    bx[4:0]
                bc6h_format.ry = header.getvalue(65,5);                //5:    ry[4:0]
                bc6h_format.gy = header.getvalue(41,4) |               //6:    gy[3:0]
                                (header.getvalue(24,1) << 4) |         //      gy[4]
                                (header.getvalue(23,1) << 5);          //      gy[5]
                bc6h_format.by = header.getvalue(61,4) |               //5:    by[3:0]
                                (header.getvalue(14,1) << 4);          //      by[4]
                bc6h_format.rz = header.getvalue(71,5);                //5:    rz[4:0]
                bc6h_format.gz = header.getvalue(51,4) |               //6:    gz[3:0]
                                (header.getvalue(40,1) << 4) |         //      gz[4]
                                (header.getvalue(33,1) << 5);          //      gz[5]
                bc6h_format.bz = header.getvalue(13,1) |               //5:    bz[0]
                                (header.getvalue(60,1) << 1) |         //      bz[1]
                                (header.getvalue(70,1) << 2) |         //      bz[2]
                                (header.getvalue(76,1) << 3) |         //      bz[3]
                                (header.getvalue(34,1) << 4);          //      bz[4]
                break;
    case 0x1A:
                bc6h_format.m_mode          = 9;  // 8:5:5:6
                bc6h_format.wBits           = 8;
                bc6h_format.tBits[C_RED]    = 5;
                bc6h_format.tBits[C_GREEN]  = 5;
                bc6h_format.tBits[C_BLUE]   = 6;
                bc6h_format.rw = header.getvalue(5,8);                 //8:    rw[7:0] 
                bc6h_format.gw = header.getvalue(15,8);                //8:    gw[7:0]
                bc6h_format.bw = header.getvalue(25,8);                //8:    bw[7:0]
                bc6h_format.rx = header.getvalue(35,5);                //5:    rx[4:0]
                bc6h_format.gx = header.getvalue(45,5);                //5:    gx[4:0]
                bc6h_format.bx = header.getvalue(55,6);                //6:    bx[5:0]
                bc6h_format.ry = header.getvalue(65,5);                //5:    ry[4:0]
                bc6h_format.gy = header.getvalue(41,4) |               //5:    gy[3:0]
                                (header.getvalue(24,1) << 4);          //      gy[4]
                bc6h_format.by = header.getvalue(61,4)    |            //6:    by[3:0]
                                (header.getvalue(14,1) << 4) |         //      by[4]
                                (header.getvalue(23,1) << 5);          //      by[5]
                bc6h_format.rz = header.getvalue(71,5);                //5:    rz[4:0]
                bc6h_format.gz = header.getvalue(51,4) |               //5:    gz[3:0]
                                (header.getvalue(40,1) << 4);          //      gz[4]
                bc6h_format.bz = header.getvalue(50,1) |               //6:    bz[0]
                                (header.getvalue(13,1) << 1) |         //      bz[1]
                                (header.getvalue(70,1) << 2) |         //      bz[2]
                                (header.getvalue(76,1) << 3) |         //      bz[3]
                                (header.getvalue(34,1) << 4) |         //      bz[4]
                                (header.getvalue(33,1) << 5);          //      bz[5]
                break;
    case 0x1E:
                bc6h_format.m_mode          = 10;  // 6:6:6:6
                bc6h_format.istransformed   = FALSE;
                bc6h_format.wBits           = 6;
                bc6h_format.tBits[C_RED]    = 6;
                bc6h_format.tBits[C_GREEN]  = 6;
                bc6h_format.tBits[C_BLUE]   = 6;
                bc6h_format.rw = header.getvalue(5,6);                 //6:    rw[5:0] 
                bc6h_format.gw = header.getvalue(15,6);                //6:    gw[5:0]
                bc6h_format.bw = header.getvalue(25,6);                //6:    bw[5:0]
                bc6h_format.rx = header.getvalue(35,6);                //6:    rx[5:0]
                bc6h_format.gx = header.getvalue(45,6);                //6:    gx[5:0]
                bc6h_format.bx = header.getvalue(55,6);                //6:    bx[5:0]
                bc6h_format.ry = header.getvalue(65,6);                //6:    ry[5:0]
                bc6h_format.gy = header.getvalue(41,4) |               //6:    gy[3:0]
                                (header.getvalue(24,1) << 4) |         //      gy[4]
                                (header.getvalue(21,1) << 5);          //      gy[5]
                bc6h_format.by = header.getvalue(61,4)    |            //6:    by[3:0]
                                (header.getvalue(14,1) << 4) |         //      by[4]
                                (header.getvalue(22,1) << 5);          //      by[5]
                bc6h_format.rz = header.getvalue(71,6);                //6:    rz[5:0]
                bc6h_format.gz = header.getvalue(51,4) |               //6:    gz[3:0]
                                (header.getvalue(11,1) << 4) |         //      gz[4]
                                (header.getvalue(31,1) << 5);          //      gz[5]
                bc6h_format.bz = header.getvalue(12,1) |               //6:    bz[0]
                                (header.getvalue(13,1) << 1) |         //      bz[1]
                                (header.getvalue(23,1) << 2) |         //      bz[2]
                                (header.getvalue(32,1) << 3) |         //      bz[3]
                                (header.getvalue(34,1) << 4) |         //      bz[4]
                                (header.getvalue(33,1) << 5);          //      bz[5]
                break;

    // Single region modes    
    case 0x03:
                bc6h_format.m_mode            = 11;  // 10:10
                bc6h_format.wBits             = 10;
                bc6h_format.tBits[C_RED]      = 10;
                bc6h_format.tBits[C_GREEN]    = 10;
                bc6h_format.tBits[C_BLUE]     = 10;
                bc6h_format.rw = header.getvalue(5,10);             // 10: rw[9:0] 
                bc6h_format.gw = header.getvalue(15,10);            // 10: gw[9:0]
                bc6h_format.bw = header.getvalue(25,10);            // 10: bw[9:0]
                bc6h_format.rx = header.getvalue(35,10);            // 10: rx[9:0]
                bc6h_format.gx = header.getvalue(45,10);            // 10: gx[9:0]
                bc6h_format.bx = header.getvalue(55,10);            // 10: bx[9:0]
                break;
    case 0x07:
                bc6h_format.m_mode              = 12;  // 11:9
                bc6h_format.wBits               = 11;
                bc6h_format.tBits[C_RED]        = 9;
                bc6h_format.tBits[C_GREEN]      = 9;
                bc6h_format.tBits[C_BLUE]       = 9;
                bc6h_format.rw = header.getvalue(5,10) |               // 10:   rw[9:0] 
                                (header.getvalue(44,1) << 10);         //       rw[10]
                bc6h_format.gw = header.getvalue(15,10) |              // 10:   gw[9:0]
                                (header.getvalue(54,1) << 10);         //       gw[10]
                bc6h_format.bw = header.getvalue(25,10) |              // 10:   bw[9:0]
                                (header.getvalue(64,1) << 10);         //       bw[10]
                bc6h_format.rx = header.getvalue(35,9);                // 9:    rx[8:0]
                bc6h_format.gx = header.getvalue(45,9);                // 9:    gx[8:0]
                bc6h_format.bx = header.getvalue(55,9);                // 9:    bx[8:0]
                break;
    case 0x0B:
                bc6h_format.m_mode              = 13;  // 12:8
                bc6h_format.wBits               = 12;
                bc6h_format.tBits[C_RED]        = 8;
                bc6h_format.tBits[C_GREEN]      = 8;
                bc6h_format.tBits[C_BLUE]       = 8;
                bc6h_format.rw = header.getvalue(5, 10) |               // 12:   rw[9:0] 
                                 (header.getvalue(43, 1) << 11) |       //       rw[11]
                                 (header.getvalue(44, 1) << 10);        //       rw[10]
                bc6h_format.gw = header.getvalue(15, 10) |              // 12:   gw[9:0]
                                 (header.getvalue(53, 1) << 11) |       //       gw[11]
                                 (header.getvalue(54, 1) << 10);        //       gw[10]
                bc6h_format.bw = header.getvalue(25,10) |               // 12:   bw[9:0]
                                 (header.getvalue(63, 1) << 11) |       //       bw[11]
                                 (header.getvalue(64,1) << 10);         //       bw[10]
                bc6h_format.rx = header.getvalue(35,8);                 //  8:   rx[7:0]
                bc6h_format.gx = header.getvalue(45,8);                 //  8:   gx[7:0]
                bc6h_format.bx = header.getvalue(55,8);                 //  8:   bx[7:0]
                break;
    case 0x0F:
                bc6h_format.m_mode          = 14;  // 16:4
                bc6h_format.wBits           = 16;
                bc6h_format.tBits[C_RED]    = 4;
                bc6h_format.tBits[C_GREEN]  = 4;
                bc6h_format.tBits[C_BLUE]   = 4;
                bc6h_format.rw = header.getvalue(5,10) |                // 16:   rw[9:0] 
                                 (header.getvalue(39, 1) << 15) |       //       rw[15]
                                 (header.getvalue(40, 1) << 14) |       //       rw[14]
                                 (header.getvalue(41, 1) << 13) |       //       rw[13]
                                 (header.getvalue(42, 1) << 12) |       //       rw[12]
                                 (header.getvalue(43, 1) << 11) |       //       rw[11]
                                 (header.getvalue(44, 1) << 10);        //       rw[10]
                bc6h_format.gw = header.getvalue(15,10) |               // 16:   gw[9:0]
                                 (header.getvalue(49, 1) << 15) |       //       gw[15]
                                 (header.getvalue(50, 1) << 14) |       //       gw[14]
                                 (header.getvalue(51, 1) << 13) |       //       gw[13]
                                 (header.getvalue(52, 1) << 12) |       //       gw[12]
                                 (header.getvalue(53, 1) << 11) |       //       gw[11]
                                 (header.getvalue(54, 1) << 10);        //       gw[10]
                bc6h_format.bw = header.getvalue(25,10) |               // 16:   bw[9:0]
                                 (header.getvalue(59, 1) << 15) |       //       bw[15]
                                 (header.getvalue(60, 1) << 14) |       //       bw[14]
                                 (header.getvalue(61, 1) << 13) |       //       bw[13]
                                 (header.getvalue(62, 1) << 12) |       //       bw[12]
                                 (header.getvalue(63, 1) << 11) |       //       bw[11]
                                 (header.getvalue(64, 1) << 10);        //       bw[10]
                bc6h_format.rx = header.getvalue(35,4);                 // 4:    rx[3:0]
                bc6h_format.gx = header.getvalue(45,4);                 // 4:    gx[3:0]
                bc6h_format.bx = header.getvalue(55,4);                 // 4:    bx[3:0]
                break;
    default:
                bc6h_format.m_mode = 0;
                return bc6h_format;
    }

    // Each format in the mode table can be uniquely identified by the mode bits. 
    // The first ten modes are used for two-region tiles, and the mode bit field 
    // can be either two or five bits long. These blocks also have fields for 
    // the compressed color endpoints (72 or 75 bits), the partition (5 bits), 
    // and the partition indices (46 bits).

    if (bc6h_format.m_mode <= 10) 
    {
        bc6h_format.region = BC6_TWO;
        // Get the shape index bits 77 to 81
        bc6h_format.d_shape_index = (unsigned short) header.getvalue(77,5);
        bc6h_format.istransformed = (bc6h_format.m_mode < 10) ? TRUE : FALSE; 
    }
    else 
    {
        bc6h_format.region           = BC6_ONE;
        bc6h_format.d_shape_index    = 0;
        bc6h_format.istransformed    = (bc6h_format.m_mode > 11) ? TRUE : FALSE; 
    }

    // Save the points in a form easy to compute with
    bc6h_format.EC[0].A[0] = (CGU_FLOAT)bc6h_format.rw; 
    bc6h_format.EC[0].B[0] = (CGU_FLOAT)bc6h_format.rx; 
    bc6h_format.EC[1].A[0] = (CGU_FLOAT)bc6h_format.ry; 
    bc6h_format.EC[1].B[0] = (CGU_FLOAT)bc6h_format.rz;
    bc6h_format.EC[0].A[1] = (CGU_FLOAT)bc6h_format.gw; 
    bc6h_format.EC[0].B[1] = (CGU_FLOAT)bc6h_format.gx; 
    bc6h_format.EC[1].A[1] = (CGU_FLOAT)bc6h_format.gy; 
    bc6h_format.EC[1].B[1] = (CGU_FLOAT)bc6h_format.gz;
    bc6h_format.EC[0].A[2] = (CGU_FLOAT)bc6h_format.bw; 
    bc6h_format.EC[0].B[2] = (CGU_FLOAT)bc6h_format.bx; 
    bc6h_format.EC[1].A[2] = (CGU_FLOAT)bc6h_format.by; 
    bc6h_format.EC[1].B[2] = (CGU_FLOAT)bc6h_format.bz;

    if (bc6h_format.region    == BC6_ONE)
    {
        int startbits = ONE_REGION_INDEX_OFFSET;
        bc6h_format.indices16[0] = (CGU_UINT8) header.getvalue(startbits,3);
        startbits+=3;
        for (int i=1; i<16; i++)
        {
            bc6h_format.indices16[i] = (CGU_UINT8)header.getvalue(startbits,4);
            startbits+=4;
        }
    }
    else
    {
        int startbit = TWO_REGION_INDEX_OFFSET, 
            nbits = 2;
        bc6h_format.indices16[0 ] = (CGU_UINT8)header.getvalue(startbit,2);
        for (int i= 1; i<16; i++)
        {
            startbit += nbits; // offset start bit for next index using prior nbits used
            nbits    = g_indexfixups[bc6h_format.d_shape_index] == i?2:3; // get new number of bit to save index with
            bc6h_format.indices16[i] = (CGU_UINT8)header.getvalue(startbit,nbits);
        }

    }

    return bc6h_format;
}

static void extract_compressed_endpoints(AMD_BC6H_Format& bc6h_format)
{
    int i;
    int t;

    if (bc6h_format.issigned)
    {
        if (bc6h_format.istransformed)
        {
            for (i=0; i<NCHANNELS; i++)
            {
                bc6h_format.E[0].A[i] = (CGU_FLOAT)SIGN_EXTEND(bc6h_format.EC[0].A[i],bc6h_format.wBits);

                t = SIGN_EXTEND(bc6h_format.EC[0].B[i], bc6h_format.tBits[i]); //C_RED
                t = int(t + bc6h_format.EC[0].A[i]) & MASK(bc6h_format.wBits);
                bc6h_format.E[0].B[i] = (CGU_FLOAT)SIGN_EXTEND(t,bc6h_format.wBits);
            }
        }
        else
        {
            for (i=0; i<NCHANNELS; i++)
            {
                bc6h_format.E[0].A[i] = (CGU_FLOAT)SIGN_EXTEND(bc6h_format.EC[0].A[i],bc6h_format.wBits);
                bc6h_format.E[0].B[i] = (CGU_FLOAT)SIGN_EXTEND(bc6h_format.EC[0].B[i],bc6h_format.tBits[i]); //C_RED
            }
        }

    }
    else
    {
        if (bc6h_format.istransformed)
        {
            for (i=0; i<NCHANNELS; i++)
            {
                bc6h_format.E[0].A[i] = bc6h_format.EC[0].A[i];
                t = SIGN_EXTEND(bc6h_format.EC[0].B[i], bc6h_format.tBits[i]); //C_RED
                bc6h_format.E[0].B[i] = CGU_FLOAT(CGU_INT(t + bc6h_format.EC[0].A[i]) & MASK(bc6h_format.wBits));
            }
        }
        else
        {
            for (i=0; i<NCHANNELS; i++)
            {
                bc6h_format.E[0].A[i] = bc6h_format.EC[0].A[i];
                bc6h_format.E[0].B[i] = bc6h_format.EC[0].B[i];
            }
        }
    }
    
}

// NV code: Used with modifcations
static int unquantize(AMD_BC6H_Format& bc6h_format, int q, int prec)
{
    int unq = 0, s;

    switch (bc6h_format.format)
    {
        // modify this case to move the multiplication by 31 after interpolation.
        // Need to use finish_unquantize.

        // since we have 16 bits available, let's unquantize this to 16 bits unsigned
        // thus the scale factor is [0-7c00)/[0-10000) = 31/64
        case UNSIGNED_F16:
            if (prec >= 15) 
                unq = q;
            else if (q == 0) 
                unq = 0;
            else if (q == ((1<<prec)-1)) 
                unq = U16MAX;
            else
                unq = (q * (U16MAX+1) + (U16MAX+1)/2) >> prec;
            break;

        // here, let's stick with S16 (no apparent quality benefit from going to S17)
        // range is (-7c00..7c00)/(-8000..8000) = 31/32
        case SIGNED_F16:
            // don't remove this test even though it appears equivalent to the code below
            // as it isn't -- the code below can overflow for prec = 16
            if (prec >= 16)
                unq = q;
            else
            {
                if (q < 0) { s = 1; q = -q; } else s = 0;

                if (q == 0)
                    unq = 0;
                else if (q >= ((1<<(prec-1))-1))
                    unq = s ? -S16MAX : S16MAX;
                else
                {
                    unq = (q * (S16MAX+1) + (S16MAX+1)/2) >> (prec-1);
                    if (s)
                        unq = -unq;
                }
            }
            break;
        }
        return unq;
}

static int lerp(int a, int b, int i, int denom)
{
    assert (denom == 3 || denom == 7 || denom == 15);
    assert (i >= 0 && i <= denom);
    
    int shift = 6, *weights = NULL;

    switch(denom)
    {
    case 3:        denom *= 5; i *= 5;    // fall through to case 15
    case 15:    weights = g_aWeights4; break;
    case 7:        weights = g_aWeights3; break;
    default:    assert(0);
    }

    #pragma warning(disable:4244)
    // no need to round these as this is an exact division
    return (int)(a*weights[denom-i] +b*weights[i]) / float(1 << shift);
}

static int finish_unquantize(AMD_BC6H_Format bc6h_format, int q)
{
    if (bc6h_format.format == UNSIGNED_F16)
        return (q * 31) >> 6;                                        // scale the magnitude by 31/64
    else if (bc6h_format.format == SIGNED_F16)
        return (q < 0) ? -(((-q) * 31) >> 5) : (q * 31) >> 5;        // scale the magnitude by 31/32
    else
        return q;
}

static void generate_palette_quantized(int max, AMD_BC6H_Format& bc6h_format, int region)
{
    // scale endpoints
    int a, b, c;            // really need a IntVec3...

    a = unquantize(bc6h_format, bc6h_format.E[region].A[0], bc6h_format.wBits); 
    b = unquantize(bc6h_format, bc6h_format.E[region].B[0], bc6h_format.wBits);

    // interpolate : This part of code is used for debuging data 
    for (int i = 0; i < max; i++)
    {
        c = finish_unquantize(bc6h_format, lerp(a, b, i, max-1));
        bc6h_format.Palete[region][i].x = c;
    }

    a = unquantize(bc6h_format, bc6h_format.E[region].A[1], bc6h_format.wBits); 
    b = unquantize(bc6h_format, bc6h_format.E[region].B[1], bc6h_format.wBits);

    // interpolate
    for (int i = 0; i < max; i++)
        bc6h_format.Palete[region][i].y = finish_unquantize(bc6h_format, lerp(a, b, i, max-1));

    a = unquantize(bc6h_format,bc6h_format.E[region].A[2], bc6h_format.wBits); 
    b = unquantize(bc6h_format,bc6h_format.E[region].B[2], bc6h_format.wBits);

    // interpolate
    for (int i = 0; i < max; i++)
        bc6h_format.Palete[region][i].z = finish_unquantize(bc6h_format, lerp(a, b, i, max-1));
}

// NV code : used with modifications
static void extract_compressed_endpoints2(AMD_BC6H_Format& bc6h_format)
{
    int i;
    int t;

    if (bc6h_format.issigned)
    {
        if (bc6h_format.istransformed)
        {
            for (i=0; i<NCHANNELS; i++)
            {
                bc6h_format.E[0].A[i] = SIGN_EXTEND(bc6h_format.EC[0].A[i],bc6h_format.wBits);

                t = SIGN_EXTEND(bc6h_format.EC[0].B[i], bc6h_format.tBits[i]); // C_RED
                t = int(t + bc6h_format.EC[0].A[i]) & MASK(bc6h_format.wBits);
                bc6h_format.E[0].B[i] = SIGN_EXTEND(t,bc6h_format.wBits);
                
                t = SIGN_EXTEND(bc6h_format.EC[1].A[i], bc6h_format.tBits[i]); //C_GREEN
                t = int(t + bc6h_format.EC[0].A[i]) & MASK(bc6h_format.wBits);
                bc6h_format.E[1].A[i] = SIGN_EXTEND(t,bc6h_format.wBits);
                
                t = SIGN_EXTEND(bc6h_format.EC[1].B[i], bc6h_format.tBits[i]); //C_BLUE
                t = int(t + bc6h_format.EC[0].A[i]) & MASK(bc6h_format.wBits);
                bc6h_format.E[1].B[i] = SIGN_EXTEND(t,bc6h_format.wBits);
            }
        }
        else
        {
            for (i=0; i<NCHANNELS; i++)
            {
                bc6h_format.E[0].A[i] = SIGN_EXTEND(bc6h_format.EC[0].A[i],bc6h_format.wBits);
                bc6h_format.E[0].B[i] = SIGN_EXTEND(bc6h_format.EC[0].B[i],bc6h_format.tBits[i]); //C_RED
                bc6h_format.E[1].A[i] = SIGN_EXTEND(bc6h_format.EC[1].A[i],bc6h_format.tBits[i]); //C_GREEN
                bc6h_format.E[1].B[i] = SIGN_EXTEND(bc6h_format.EC[1].B[i],bc6h_format.tBits[i]); //C_BLUE
            }
        }

    }
    else
    {
        if (bc6h_format.istransformed)
        {
            for (i=0; i<NCHANNELS; i++)
            {
                bc6h_format.E[0].A[i] = bc6h_format.EC[0].A[i];
                t = SIGN_EXTEND(bc6h_format.EC[0].B[i], bc6h_format.tBits[i]); // C_RED
                bc6h_format.E[0].B[i] = int(t + bc6h_format.EC[0].A[i]) & MASK(bc6h_format.wBits);
                
                t = SIGN_EXTEND(bc6h_format.EC[1].A[i], bc6h_format.tBits[i]); // C_GREEN
                bc6h_format.E[1].A[i] = int(t + bc6h_format.EC[0].A[i]) & MASK(bc6h_format.wBits);
                
                t = SIGN_EXTEND(bc6h_format.EC[1].B[i], bc6h_format.tBits[i]); //C_BLUE
                bc6h_format.E[1].B[i] = int(t + bc6h_format.EC[0].A[i]) & MASK(bc6h_format.wBits);
            }
        }
        else
        {
            for (i=0; i<NCHANNELS; i++)
            {
                bc6h_format.E[0].A[i] = bc6h_format.EC[0].A[i];
                bc6h_format.E[0].B[i] = bc6h_format.EC[0].B[i];
                bc6h_format.E[1].A[i] = bc6h_format.EC[1].A[i];
                bc6h_format.E[1].B[i] = bc6h_format.EC[1].B[i];
            }
        }
    }
    
}

void  DecompressBC6_Internal(CGU_UINT16 rgbBlock[48], const CGU_UINT8 compressedBlock[16], const BC6H_Encode *BC6HEncode)
{
    if (BC6HEncode) {}
    CGU_BOOL m_bc6signed = false;
    // now determine the mode type and extract the coded endpoints data 
    AMD_BC6H_Format bc6h_format = extract_format(compressedBlock);
    if (!m_bc6signed)
        bc6h_format.format = UNSIGNED_F16;
    else
        bc6h_format.format = SIGNED_F16;

    if(bc6h_format.region == BC6_ONE)
    {
        extract_compressed_endpoints(bc6h_format);
        generate_palette_quantized(16,bc6h_format,0);
    }
    else //mode.type == BC6_TWO
    {
        extract_compressed_endpoints2(bc6h_format);
        for (int r=0; r<2; r++)
        {
            generate_palette_quantized(8,bc6h_format,r);
        }
    }

    
    BC6H_Vec3 data;
    int indexPos=0;
    int rgbPos=0;

    // Note first 32 BC6H_PARTIONS is shared with BC6H
    // Partitioning is always arranged such that index 0 is always in subset 0 of BC6H_PARTIONS array 
    // Partition order goes from top-left to bottom-right, moving left to right and then top to bottom.
    for (int block_row = 0; block_row < 4; block_row++)
    for (int block_col = 0; block_col < 4; block_col++)
    {
        // Need to check region logic
        // gets the region (0 or 1) in the partition set
        //int region = bc6h_format.region == BC6_ONE?0:REGION(block_col,block_row,bc6h_format.d_shape_index);
        // for a one region partitions : its always return 0 so there is room for performance improvement
        // by seperating the condition into another looped call.
        //int region = bc6h_format.region == BC6_ONE?0:BC6H_PARTITIONS[1][bc6h_format.d_shape_index][indexPos];
        int region = bc6h_format.region == BC6_ONE?0:BC6_PARTITIONS[bc6h_format.d_shape_index][indexPos];

        // Index is validated as ok
        int paleteIndex  = bc6h_format.indices[block_row][block_col];
        
        // this result is validated ok for region = BC6_ONE , BC6_TWO To be determined 
        data = bc6h_format.Palete[region][paleteIndex];

        rgbBlock[rgbPos++] = data.x;
        rgbBlock[rgbPos++] = data.y;
        rgbBlock[rgbPos++] = data.z;
        indexPos++;
    }

}

//======================= END OF DECOMPRESS CODE =========================================

int CMP_CDECL CreateOptionsBC6(void **options)
{
    (*options) = new BC6H_Encode;
    if (!options) return CGU_CORE_ERR_NEWMEM;
    SetDefaultBC6Options((BC6H_Encode *)(*options));
    return CGU_CORE_OK;
}

int CMP_CDECL DestroyOptionsBC6(void *options)
{
    if (!options) return CGU_CORE_ERR_INVALIDPTR;
    BC6H_Encode *BCOptions = reinterpret_cast <BC6H_Encode *>(options);
    delete BCOptions;
    return CGU_CORE_OK;
}

int CMP_CDECL SetQualityBC6(void *options, CGU_FLOAT fquality)
{
    if (!options) return CGU_CORE_ERR_INVALIDPTR;
    BC6H_Encode *BC6optionsDefault = (BC6H_Encode *)options;
    if (fquality < 0.0f) fquality = 0.0f;
    else
        if (fquality > 1.0f) fquality = 1.0f;
    BC6optionsDefault->m_quality = fquality;
    BC6optionsDefault->m_partitionSearchSize = (BC6optionsDefault->m_quality*2.0F) / qFAST_THRESHOLD;
    if (BC6optionsDefault->m_partitionSearchSize < (1.0F / 16.0F))
        BC6optionsDefault->m_partitionSearchSize = (1.0F / 16.0F);
    return CGU_CORE_OK;
}

int CMP_CDECL SetMaskBC6(void *options, CGU_UINT32 mask)
{
    if (!options) return CGU_CORE_ERR_INVALIDPTR;
    BC6H_Encode *BC6options = (BC6H_Encode *)options;
    BC6options->m_validModeMask = mask;
    return CGU_CORE_OK;
}

int CMP_CDECL CompressBlockBC6(const CGU_UINT16 *srcBlock,
                               unsigned int srcStrideInShorts,
                               CMP_GLOBAL CGU_UINT8 cmpBlock[16],
                               const CMP_GLOBAL void *options = NULL)
{

    CGU_UINT16 inBlock[48];

    //----------------------------------
    // Fill the inBlock with source data
    //----------------------------------
    CGU_INT srcpos = 0;
    CGU_INT dstptr = 0;
    for (CGU_UINT8 row = 0; row < 4; row++)
    {
        srcpos = row * srcStrideInShorts;
        for (CGU_UINT8 col = 0; col < 4; col++)
        {
            inBlock[dstptr++] = CGU_UINT16(srcBlock[srcpos++]);
            inBlock[dstptr++] = CGU_UINT16(srcBlock[srcpos++]);
            inBlock[dstptr++] = CGU_UINT16(srcBlock[srcpos++]);
        }
    }


    BC6H_Encode *BC6HEncode = (BC6H_Encode *)options;
    BC6H_Encode BC6HEncodeDefault;

    if (BC6HEncode == NULL)
    {
        BC6HEncode = &BC6HEncodeDefault;
        SetDefaultBC6Options(BC6HEncode);
    }

    BC6H_Encode_local BC6HEncode_local;
    memset((CGU_UINT8 *)&BC6HEncode_local, 0, sizeof(BC6H_Encode_local));
    CGU_UINT8    blkindex = 0;
    for ( CGU_INT32 j = 0; j < 16; j++) {
        BC6HEncode_local.din[j][0] = inBlock[blkindex++];  // R
        BC6HEncode_local.din[j][1] = inBlock[blkindex++];  // G
        BC6HEncode_local.din[j][2] = inBlock[blkindex++];  // B
        BC6HEncode_local.din[j][3] = 0;                    // A
        }

    CompressBlockBC6_Internal(cmpBlock, 0, &BC6HEncode_local,BC6HEncode);

    return CGU_CORE_OK;
}

int  CMP_CDECL DecompressBlockBC6(const unsigned char cmpBlock[16],
                            CGU_UINT16 srcBlock[48],
                            const void *options = NULL) {
    BC6H_Encode *BC6HEncode = (BC6H_Encode *)options;
    BC6H_Encode BC6HEncodeDefault;

    if (BC6HEncode == NULL)
    {
        BC6HEncode = &BC6HEncodeDefault;
        SetDefaultBC6Options(BC6HEncode);
    }
    DecompressBC6_Internal(srcBlock, cmpBlock,BC6HEncode);

    return CGU_CORE_OK;
}

#endif // !ASPM
#endif // !ASPM_GPU

//============================================== OpenCL USER INTERFACE ====================================================
#ifdef ASPM_GPU
CMP_STATIC CMP_KERNEL void CMP_GPUEncoder(
    CMP_GLOBAL  CGU_UINT8*          p_source_pixels,
    CMP_GLOBAL  CGU_UINT8*          p_encoded_blocks,
    CMP_GLOBAL  Source_Info*        SourceInfo,
    CMP_GLOBAL  BC6H_Encode *       BC6HEncode
)
{
    CGU_UINT32 x = get_global_id(0);
    CGU_UINT32 y = get_global_id(1);

    if (x >= (SourceInfo->m_src_width / BYTEPP)) return;
    if (y >= (SourceInfo->m_src_height / BYTEPP)) return;

    BC6H_Encode_local BC6HEncode_local;
    memset((CGU_UINT8 *)&BC6HEncode_local, 0, sizeof(BC6H_Encode_local));


    CGU_UINT32 stride = SourceInfo->m_src_width * BYTEPP;
    CGU_UINT32 srcOffset = (x*BlockX*BYTEPP) + (y*stride*BYTEPP);
    CGU_UINT32 destI = (x*COMPRESSED_BLOCK_SIZE) + (y*(SourceInfo->m_src_width / BlockX)*COMPRESSED_BLOCK_SIZE);
    CGU_UINT32 srcidx;

    //CGU_FLOAT block4x4[16][4];

    for (CGU_INT i = 0; i < BlockX; i++)
    {
        srcidx = i * stride;
        for (CGU_INT j = 0; j < BlockY; j++)
        {
            BC6HEncode_local.din[i*BlockX + j][0] = (CGU_UINT16)(p_source_pixels[srcOffset + srcidx++]);
            if (BC6HEncode_local.din[i*BlockX + j][0] < 0.00001 || isnan(BC6HEncode_local.din[i*BlockX + j][0]))
            {
                if (BC6HEncode->m_isSigned)
                {
                    BC6HEncode_local.din[i*BlockX + j][0] = (isnan(BC6HEncode_local.din[i*BlockX + j][0])) ? F16NEGPREC_LIMIT_VAL : -BC6HEncode_local.din[i*BlockX + j][0];
                    if (BC6HEncode_local.din[i*BlockX + j][0] < F16NEGPREC_LIMIT_VAL) {
                        BC6HEncode_local.din[i*BlockX + j][0] = F16NEGPREC_LIMIT_VAL;
                    }
                }
                else
                    BC6HEncode_local.din[i*BlockX + j][0] = 0.0;
            }

            BC6HEncode_local.din[i*BlockX + j][1] = (CGU_UINT16)(p_source_pixels[srcOffset + srcidx++]);

            if (BC6HEncode_local.din[i*BlockX + j][1] < 0.00001 || isnan(BC6HEncode_local.din[i*BlockX + j][1]))
            {
                if (BC6HEncode->m_isSigned)
                {
                    BC6HEncode_local.din[i*BlockX + j][1] = (isnan(BC6HEncode_local.din[i*BlockX + j][1])) ? F16NEGPREC_LIMIT_VAL : -BC6HEncode_local.din[i*BlockX + j][1];
                    if (BC6HEncode_local.din[i*BlockX + j][1] < F16NEGPREC_LIMIT_VAL) {
                        BC6HEncode_local.din[i*BlockX + j][1] = F16NEGPREC_LIMIT_VAL;
                    }
                }
                else
                    BC6HEncode_local.din[i*BlockX + j][1] = 0.0;
            }

            BC6HEncode_local.din[i*BlockX + j][2] = (CGU_UINT16)(p_source_pixels[srcOffset + srcidx++]);
            if (BC6HEncode_local.din[i*BlockX + j][2] < 0.00001 || isnan(BC6HEncode_local.din[i*BlockX + j][2]))
            {
                if (BC6HEncode->m_isSigned)
                {
                    BC6HEncode_local.din[i*BlockX + j][2] = (isnan(BC6HEncode_local.din[i*BlockX + j][2])) ? F16NEGPREC_LIMIT_VAL : -BC6HEncode_local.din[i*BlockX + j][2];
                    if (BC6HEncode_local.din[i*BlockX + j][2] < F16NEGPREC_LIMIT_VAL) {
                        BC6HEncode_local.din[i*BlockX + j][2] = F16NEGPREC_LIMIT_VAL;
                    }
                }
                else
                    BC6HEncode_local.din[i*BlockX + j][2] = 0.0;
            }

            BC6HEncode_local.din[i*BlockX + j][3] = 0.0f;
            //printf("Ori---src image %d, --%02x", x, (p_source_pixels[srcOffset + srcidx++]) & 0x0000ff); //for debug
        }
    }

    // printf(" X %3d Y %3d Quality %2.2f", x, y, BC6HEncode->m_quality);
    CompressBlockBC6_Internal(p_encoded_blocks, destI, &BC6HEncode_local, BC6HEncode);
}
#endif
