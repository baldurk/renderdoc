/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2019 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include <math.h>
#include "common/common.h"

///////////////////////////////////////////////////////////////////////////
// Grisu2 implementation (slightly simpler than Grisu3) for converting
// doubles to strings
//
// Sources:
// Based on Florian Loitsch 2010 "Printing Floating-Point Numbers Quickly
//                                and Accurately with Integers"
//     http://florian.loitsch.com/publications/dtoa-pldi2010.pdf
//     https://github.com/floitsch/double-conversion (BSD licensed)
//
// Also implementations by Milo Yip and night-shift used as reference
//     https://github.com/miloyip/dtoa-benchmark
//     https://github.com/night-shift/fpconv

struct diy_fp
{
  diy_fp() : mantissa(0), exp(0) {}
  diy_fp(uint64_t mant, int exponent) : mantissa(mant), exp(exponent) {}
  uint64_t mantissa;
  int exp;

  // q in the paper, bits in the mantissa of the fixed point
  // approximation
  static const int bitsq = 64;
};

// subtract from Florian paper
diy_fp operator-(const diy_fp &x, const diy_fp &y)
{
  // assume same exponent
  return diy_fp(x.mantissa - y.mantissa, x.exp);
}

// multiply from Florian paper
diy_fp operator*(const diy_fp &x, const diy_fp &y)
{
  // _a = upper 32 bits, _b = lower 32 bits
  uint64_t xa = x.mantissa >> 32, xb = x.mantissa & 0xFFFFFFFF;
  uint64_t ya = y.mantissa >> 32, yb = y.mantissa & 0xFFFFFFFF;

  // perform each pair of multiplies
  uint64_t upper = xa * ya;
  uint64_t lower = xb * yb;
  uint64_t cross1 = xb * ya;
  uint64_t cross2 = xa * yb;

  uint64_t tmp = (lower >> 32) + (cross1 & 0xFFFFFFFF) + (cross2 & 0xFFFFFFFF);
  tmp += 1U << 31;    // Round up

  // note - exponent is no longer normalised
  return diy_fp(upper + (cross1 >> 32) + (cross2 >> 32) + (tmp >> 32), x.exp + y.exp + 64);
}

static diy_fp pow10cache[] = {
    diy_fp(18054884314459144840U, -1220), diy_fp(13451937075301367670U, -1193),
    diy_fp(10022474136428063862U, -1166), diy_fp(14934650266808366570U, -1140),
    diy_fp(11127181549972568877U, -1113), diy_fp(16580792590934885855U, -1087),
    diy_fp(12353653155963782858U, -1060), diy_fp(18408377700990114895U, -1034),
    diy_fp(13715310171984221708U, -1007), diy_fp(10218702384817765436U, -980),
    diy_fp(15227053142812498563U, -954),  diy_fp(11345038669416679861U, -927),
    diy_fp(16905424996341287883U, -901),  diy_fp(12595523146049147757U, -874),
    diy_fp(9384396036005875287U, -847),   diy_fp(13983839803942852151U, -821),
    diy_fp(10418772551374772303U, -794),  diy_fp(15525180923007089351U, -768),
    diy_fp(11567161174868858868U, -741),  diy_fp(17236413322193710309U, -715),
    diy_fp(12842128665889583758U, -688),  diy_fp(9568131466127621947U, -661),
    diy_fp(14257626930069360058U, -635),  diy_fp(10622759856335341974U, -608),
    diy_fp(15829145694278690180U, -582),  diy_fp(11793632577567316726U, -555),
    diy_fp(17573882009934360870U, -529),  diy_fp(13093562431584567480U, -502),
    diy_fp(9755464219737475723U, -475),   diy_fp(14536774485912137811U, -449),
    diy_fp(10830740992659433045U, -422),  diy_fp(16139061738043178685U, -396),
    diy_fp(12024538023802026127U, -369),  diy_fp(17917957937422433684U, -343),
    diy_fp(13349918974505688015U, -316),  diy_fp(9946464728195732843U, -289),
    diy_fp(14821387422376473014U, -263),  diy_fp(11042794154864902060U, -236),
    diy_fp(16455045573212060422U, -210),  diy_fp(12259964326927110867U, -183),
    diy_fp(18268770466636286478U, -157),  diy_fp(13611294676837538539U, -130),
    diy_fp(10141204801825835212U, -103),  diy_fp(15111572745182864684U, -77),
    diy_fp(11258999068426240000U, -50),   diy_fp(16777216000000000000U, -24),
    diy_fp(12500000000000000000U, 3),     diy_fp(9313225746154785156U, 30),
    diy_fp(13877787807814456755U, 56),    diy_fp(10339757656912845936U, 83),
    diy_fp(15407439555097886824U, 109),   diy_fp(11479437019748901445U, 136),
    diy_fp(17105694144590052135U, 162),   diy_fp(12744735289059618216U, 189),
    diy_fp(9495567745759798747U, 216),    diy_fp(14149498560666738074U, 242),
    diy_fp(10542197943230523224U, 269),   diy_fp(15709099088952724970U, 295),
    diy_fp(11704190886730495818U, 322),   diy_fp(17440603504673385349U, 348),
    diy_fp(12994262207056124023U, 375),   diy_fp(9681479787123295682U, 402),
    diy_fp(14426529090290212157U, 428),   diy_fp(10748601772107342003U, 455),
    diy_fp(16016664761464807395U, 481),   diy_fp(11933345169920330789U, 508),
    diy_fp(17782069995880619868U, 534),   diy_fp(13248674568444952270U, 561),
    diy_fp(9871031767461413346U, 588),    diy_fp(14708983551653345445U, 614),
    diy_fp(10959046745042015199U, 641),   diy_fp(16330252207878254650U, 667),
    diy_fp(12166986024289022870U, 694),   diy_fp(18130221999122236476U, 720),
    diy_fp(13508068024458167312U, 747),   diy_fp(10064294952495520794U, 774),
    diy_fp(14996968138956309548U, 800),   diy_fp(11173611982879273257U, 827),
    diy_fp(16649979327439178909U, 853),   diy_fp(12405201291620119593U, 880),
    diy_fp(9242595204427927429U, 907),    diy_fp(13772540099066387757U, 933),
    diy_fp(10261342003245940623U, 960),   diy_fp(15290591125556738113U, 986),
    diy_fp(11392378155556871081U, 1013),  diy_fp(16975966327722178521U, 1039),
    diy_fp(12648080533535911531U, 1066),
};

static const int firstpow10 = -348;    // first cached power of 10
static const int cachestep = 8;        // power of 10 steps between cache items

diy_fp find_cachedpow10(int exp, int &kout)
{
  const double inv_log2_10 = 0.30102999566398114;
  const double alpha = -60.0;

  // k calculation from the paper ceil[ (alpha - exp + q - 1) * 1/log2(10) ]
  // exponent is shifted by #bits
  int k = (int)ceil((alpha - double(exp + diy_fp::bitsq) + diy_fp::bitsq - 1) * inv_log2_10);

  // determine index in above array
  int idx = (-firstpow10 + k - 1) / cachestep + 1;

  // output the decimal power that corresponds to this k
  kout = (firstpow10 + idx * cachestep);

  return pow10cache[idx];
}

static int gen_digits(const diy_fp &lower, const diy_fp &upper, char *digits, int &kout)
{
  diy_fp delta = upper - lower;

  // generate 1.0 to the desired exponent so we can split integer from decimal part
  diy_fp one(uint64_t(1) << -upper.exp, upper.exp);

  // mask off integer and decimal parts
  uint64_t intpart = upper.mantissa >> -one.exp;
  uint64_t decpart = upper.mantissa & (one.mantissa - 1);

  // len is current number of digits produced
  int len = 0;
  // kappa is an exponent shift, to account for if we don't produce exactly the number
  // of digits to reach the decimal place, and there should be extra 0s beyond the produced
  // digits. (or negative if there should be preceeding 0s)
  int kappa = 10;
  uint32_t div = 1000000000;    // highest possible pow10 in 32bits = 10^9

  // handle integer component before decimal separator
  while(kappa > 0)
  {
    // get digit at current power of ten
    uint64_t digit = intpart / div;

    // don't include preceeding 0 digits (so either include if
    // digit is non-0, or if we've started including digits ie.
    // len > 0)
    if(digit || len)
      digits[len++] = '0' + char(digit);

    // remove this pow10 from the int for future iterations
    intpart %= div;
    kappa--;
    div /= 10;

    // this is our termination condition, when we've produced the number.
    // delta is the difference between upper and lower, and the left side
    // is the current remainder after the currently generated digits have
    // been removed. If that is small enough that we've produced the number,
    // exit and increment kout to account for the extra exponential
    if((intpart << -one.exp) + decpart <= delta.mantissa)
    {
      kout += kappa;
      return len;
    }
  }

  // note, after this part if we're still here, intpart is 0 as we've
  // masked off all digits, so only decpart remains.
  // Kappa has also reached 0, beyond here it decrements below 0

  // handle decimal portion after separator
  do
  {
    decpart *= 10;
    uint64_t digit = decpart >> -one.exp;

    // don't include preceeding 0s (see above - note if we've produced
    // any integer digits at all, len will be > 0)
    if(digit || len)
      digits[len++] = '0' + char(digit);

    // remove this pow10 from the decimal part
    decpart &= (one.mantissa - 1);
    kappa--;
    delta.mantissa *= 10;

    // stop looping when decpart is lower than delta (see above for termination condition)
  } while(decpart > delta.mantissa);

  kout += kappa;

  return len;
}

int grisu2(uint64_t mantissa, int exponent, char digits[18], int &kout)
{
  // the IEEE format implicitly has a hidden 1 bit above the mantissa for all normalised
  // numbers
  const uint64_t hiddenbit = 0x0010000000000000;

  // exponent is shifted by a further 52 because input exponent is assuming mantissa
  // is 1.2345678...e exp (fraction)
  // but grisu2 treats number as
  //    12345678...e exp-52 (whole number)
  diy_fp w = diy_fp(mantissa | hiddenbit, exponent - 52);
  if(exponent == -1023)
    w.exp = 1 - (1023 + 52);    // subnormal exponent

  // we know the w input comes from a double, so is only using the lower 52 bits at
  // most. We can safely multiply by 2 (and cancel by lowering exponent to match), then
  // add 1 to get the upper value
  diy_fp upper;
  upper.mantissa = (w.mantissa << 1) + 1;
  upper.exp = w.exp - 1;

  diy_fp lower;
  if(mantissa == 0)
  {
    // if mantissa is 0 we are going to underflow, so shift by 2
    // to maintain precision/normalised value
    lower.mantissa = (w.mantissa << 2) - 1;
    lower.exp = w.exp - 2;
  }
  else
  {
    lower.mantissa = (w.mantissa << 1) - 1;
    lower.exp = w.exp - 1;
  }

  // normalise upper - shift the mantissa until the top bit is set, and decrement the
  // exponent each time to keep the number the same (1.2e5 and 12e4 are equivalent representations)
  while((upper.mantissa & 0x8000000000000000) == 0)
  {
    upper.mantissa <<= 1;
    upper.exp--;
  }

  // for valid floats shift operation will always be non-negative, but just to be paranoid let's
  // make sure that it is. This will produce incorrect results but garbage-in, garbage-out.
  if(lower.exp < upper.exp)
    lower.exp = upper.exp;

  // lower needs to be the same exponent as upper so we can calculate delta by upper-lower, so
  // it is not normalised, but shifted to upper's exponent
  lower.mantissa <<= (lower.exp - upper.exp);
  lower.exp = upper.exp;

  int k = 0;
  diy_fp ck = find_cachedpow10(upper.exp, k);

  lower = lower * ck;
  upper = upper * ck;

  // squeeze the range in by 1 ULP
  lower.mantissa++;
  upper.mantissa--;

  // set our initial exponent. This will be shifted
  // if we have any preceeding or trailing 0s to get the
  // final exponent
  kout = -k;

  return gen_digits(lower, upper, digits, kout);
}
