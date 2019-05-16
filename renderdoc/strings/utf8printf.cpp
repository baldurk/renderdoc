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

#include "common/common.h"

// grisu2 double-to-string function, returns number of digits written to digits array
int grisu2(uint64_t mantissa, int exponent, char digits[18], int &kout);

///////////////////////////////////////////////////////////////////////////////
// functions for appending to output (handling running out of buffer space)

void addchar(char *&output, size_t &actualsize, char *end, char c)
{
  actualsize++;

  if(output == end)
    return;

  *(output++) = c;
}

void addchars(char *&output, size_t &actualsize, char *end, size_t num, char c)
{
  actualsize += num;
  for(size_t i = 0; output != end && i < num; i++)
    *(output++) = c;
}

void appendstring(char *&output, size_t &actualsize, char *end, const char *str, size_t len)
{
  for(size_t i = 0; i < len; i++)
  {
    if(str[i] == 0)
      return;

    actualsize++;
    if(output != end)
      *(output++) = str[i];
  }
}

void appendstring(char *&output, size_t &actualsize, char *end, const char *str)
{
  for(size_t i = 0; *str; i++)
  {
    actualsize++;
    if(output != end)
      *(output++) = *str;
    str++;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Flags and general formatting parameters

enum FormatterFlags
{
  LeftJustify = 0x1,
  PrependPos = 0x2,
  PrependSpace = 0x4,
  AlternateForm = 0x8,
  PadZeroes = 0x10,
  // non standard
  AlwaysDecimal = 0x20,
};

enum LengthModifier
{
  None,
  HalfHalf,
  Half,
  Long,
  LongLong,
  SizeT,
};

struct FormatterParams
{
  FormatterParams() : Flags(0), Width(NoWidth), Precision(NoPrecision), Length(None) {}
  int Flags;
  int Width;
  int Precision;
  LengthModifier Length;

  static const int NoWidth = -1;    // can't set negative width, so -1 indicates no width specified
  static const int NoPrecision =
      -1;    // can't set negative precision, so -1 indicates no precision specified
};

///////////////////////////////////////////////////////////////////////////////
// Print a number in a specified base (16, 8, 10 or 2 supported)

void PrintInteger(bool typeUnsigned, uint64_t argu, int base, uint64_t numbits,
                  FormatterParams formatter, bool uppercaseDigits, char *&output,
                  size_t &actualsize, char *end)
{
  int64_t argi = 0;

  union
  {
    uint64_t *u64;
    signed int *i;
    signed char *c;
    signed short *s;
    int64_t *i64;
  } typepun;

  typepun.u64 = &argu;

  // cast the appropriate size to signed version
  switch(formatter.Length)
  {
    default:
    case None:
    case Long: argi = (int64_t)*typepun.i; break;
    case HalfHalf: argi = (int64_t)*typepun.c; break;
    case Half: argi = (int64_t)*typepun.s; break;
    case LongLong: argi = (int64_t)*typepun.i64; break;
#if ENABLED(RDOC_X64)
      RDCCOMPILE_ASSERT(sizeof(size_t) == sizeof(int64_t), "64-bit macros are wrong");
    case SizeT: argi = (int64_t)*typepun.i64; break;
#else
      RDCCOMPILE_ASSERT(sizeof(size_t) == sizeof(int32_t), "64-bit macros are wrong");
    case SizeT: argi = (int64_t)*typepun.i; break;
#endif
  }

  bool negative = false;
  if(base == 10 && !typeUnsigned)
  {
    negative = argi < 0;
  }

  int digwidth = 0;
  int numPad0s = 0;
  int numPadWidth = 0;
  {
    int intwidth = 0;
    int digits = 0;

    // work out the number of decimal digits in the integer
    if(!negative)
    {
      uint64_t accum = argu;
      while(accum)
      {
        digits += 1;
        accum /= base;
      }
    }
    else
    {
      int64_t accum = argi;
      while(accum)
      {
        digits += 1;
        accum /= base;
      }
    }

    intwidth = digwidth = RDCMAX(1, digits);

    // printed int is 2 chars larger for 0x or 0b, and 1 char for 0 (octal)
    if(base == 16 || base == 2)
      intwidth += formatter.Flags & AlternateForm ? 2 : 0;
    if(base == 8)
      intwidth += formatter.Flags & AlternateForm ? 1 : 0;

    if(formatter.Precision != FormatterParams::NoPrecision && formatter.Precision > intwidth)
      numPad0s = formatter.Precision - intwidth;

    intwidth += numPad0s;

    // for decimal we can have a negative sign (or placeholder)
    if(base == 10)
    {
      if(negative)
        intwidth++;
      else if(formatter.Flags & (PrependPos | PrependSpace))
        intwidth++;
    }

    if(formatter.Width != FormatterParams::NoWidth && formatter.Width > intwidth)
      numPadWidth = formatter.Width - intwidth;
  }

  // pad with spaces if necessary
  if((formatter.Flags & (LeftJustify | PadZeroes)) == 0 && numPadWidth > 0)
    addchars(output, actualsize, end, (size_t)numPadWidth, ' ');

  if(base == 16)
  {
    if(formatter.Flags & AlternateForm)
    {
      appendstring(output, actualsize, end, "0x");
    }

    // pad with 0s as appropriate
    if((formatter.Flags & (LeftJustify | PadZeroes)) == PadZeroes && numPadWidth > 0)
      addchars(output, actualsize, end, (size_t)numPadWidth, '0');
    if(numPad0s > 0)
      addchars(output, actualsize, end, (size_t)numPad0s, '0');

    bool left0s = true;

    // mask off each hex digit and print
    for(uint64_t i = 0; i < numbits; i += 4)
    {
      uint64_t shift = numbits - 4 - i;
      uint64_t mask = 0xfULL << shift;
      char digit = char((argu & mask) >> shift);
      if(digit == 0 && left0s && i + 4 < numbits)
        continue;
      left0s = false;

      if(digit < 10)
        addchar(output, actualsize, end, '0' + digit);
      else if(uppercaseDigits)
        addchar(output, actualsize, end, 'A' + digit - 10);
      else
        addchar(output, actualsize, end, 'a' + digit - 10);
    }
  }
  else if(base == 8)
  {
    if(formatter.Flags & AlternateForm)
      appendstring(output, actualsize, end, "0");

    if((formatter.Flags & (LeftJustify | PadZeroes)) == PadZeroes && numPadWidth > 0)
      addchars(output, actualsize, end, (size_t)numPadWidth, '0');
    if(numPad0s > 0)
      addchars(output, actualsize, end, (size_t)numPad0s, '0');

    // octal digits don't quite fit into typical integer sizes,
    // so instead we pretend the number is a little bigger, then
    // the shift just fills out the upper bits with 0s.
    uint64_t offs = 0;
    if(numbits % 3 == 1)
      offs = 2;
    if(numbits % 3 == 2)
      offs = 1;

    bool left0s = true;

    for(uint64_t i = 0; i < numbits; i += 3)
    {
      uint64_t shift = numbits - 3 - i + offs;
      uint64_t mask = 0x7ULL << shift;

      char digit = char((argu & mask) >> shift);
      if(digit == 0 && left0s && i + 3 < numbits)
        continue;
      left0s = false;

      addchar(output, actualsize, end, '0' + digit);
    }
  }
  else if(base == 2)
  {
    if(formatter.Flags & AlternateForm)
    {
      if(uppercaseDigits)
        appendstring(output, actualsize, end, "0B");
      else
        appendstring(output, actualsize, end, "0b");
    }

    if((formatter.Flags & (LeftJustify | PadZeroes)) == PadZeroes && numPadWidth > 0)
      addchars(output, actualsize, end, (size_t)numPadWidth, '0');
    if(numPad0s > 0)
      addchars(output, actualsize, end, (size_t)numPad0s, '0');

    bool left0s = true;

    for(uint64_t i = 0; i < numbits; i++)
    {
      uint64_t shift = numbits - 1 - i;
      uint64_t mask = 0x1ULL << shift;
      char digit = char((argu & mask) >> shift);
      if(digit == 0 && left0s && i + 1 < numbits)
        continue;
      left0s = false;

      addchar(output, actualsize, end, '0' + digit);
    }
  }
  else
  {
    // buffer large enough for any int (up to 64bit unsigned)
    char intbuf[32] = {0};

    // handle edge case of INT_MIN so we can negate the number and be sure we
    // won't actualsize
    if(argu == 0x8000000000000000)
    {
      addchar(output, actualsize, end, '-');
      if((formatter.Flags & (LeftJustify | PadZeroes)) == PadZeroes && numPadWidth > 0)
        addchars(output, actualsize, end, (size_t)numPadWidth, '0');
      if(numPad0s > 0)
        addchars(output, actualsize, end, (size_t)numPad0s, '0');
      appendstring(output, actualsize, end, "9223372036854775808");
    }
    else
    {
      // we know we can negate without loss of precision because we handled 64bit INT_MIN above
      if(negative)
      {
        addchar(output, actualsize, end, '-');
        argi = -argi;
      }
      else if(formatter.Flags & PrependPos)
        addchar(output, actualsize, end, '+');
      else if(formatter.Flags & PrependSpace)
        addchar(output, actualsize, end, ' ');

      if((formatter.Flags & (LeftJustify | PadZeroes)) == PadZeroes && numPadWidth > 0)
        addchars(output, actualsize, end, (size_t)numPadWidth, '0');
      if(numPad0s > 0)
        addchars(output, actualsize, end, (size_t)numPad0s, '0');

      if(typeUnsigned)
      {
        uint64_t accum = argu;
        for(int i = 0; i < digwidth; i++)
        {
          int digit = accum % 10;
          accum /= 10;

          intbuf[digwidth - 1 - i] = char('0' + digit);
        }
      }
      else
      {
        int64_t accum = argi;
        for(int i = 0; i < digwidth; i++)
        {
          int digit = accum % 10;
          accum /= 10;

          intbuf[digwidth - 1 - i] = char('0' + digit);
        }
      }

      char *istr = intbuf;
      while(*istr == '0')
        istr++;

      if(*istr == 0 && istr > intbuf)
        istr--;

      appendstring(output, actualsize, end, istr);
    }
  }

  // if we were left justifying, pad on the right with spaces
  if((formatter.Flags & LeftJustify) && numPadWidth > 0)
  {
    addchars(output, actualsize, end, (size_t)numPadWidth, ' ');
  }
}

void PrintFloat0(bool e, bool f, FormatterParams formatter, char prepend, char *&output,
                 size_t &actualsize, char *end)
{
  int numwidth = 0;

  if(e)
    numwidth = formatter.Precision + 1 + 5;    // 0 plus precision plus e+000
  else if(f || formatter.Flags & AlternateForm)
    numwidth = formatter.Precision + 1;    // 0 plus precision
  else
    numwidth = 1;

  // alternate form means . is included even if no digits after .
  if(((e || f) && formatter.Precision > 0) || (formatter.Flags & AlternateForm))
    numwidth++;    // .

  if(!e && !f && (formatter.Flags & AlwaysDecimal))
  {
    numwidth += 2;    // .0
  }

  // sign space
  if(prepend)
    numwidth++;

  int padlen = 0;

  if(formatter.Width != FormatterParams::NoWidth && formatter.Width > numwidth)
    padlen = formatter.Width - numwidth;

  if(formatter.Flags & PadZeroes)
  {
    if(prepend)
      addchar(output, actualsize, end, prepend);
    addchars(output, actualsize, end, size_t(padlen), '0');
  }
  else if(padlen > 0 && (formatter.Flags & LeftJustify) == 0)
  {
    addchars(output, actualsize, end, size_t(padlen), ' ');
    if(prepend)
      addchar(output, actualsize, end, prepend);
  }
  else
  {
    if(prepend)
      addchar(output, actualsize, end, prepend);
  }

  // print a .0 for all cases except non-alternate %g
  if(e || f || formatter.Flags & AlternateForm)
  {
    addchar(output, actualsize, end, '0');
    if(formatter.Precision > 0 || (formatter.Flags & AlternateForm))
      addchar(output, actualsize, end, '.');
    addchars(output, actualsize, end, size_t(formatter.Precision), '0');

    if(e)
      appendstring(output, actualsize, end, "e+000");
  }
  else
  {
    addchar(output, actualsize, end, '0');

    if(!e && !f && (formatter.Flags & AlwaysDecimal))
    {
      addchar(output, actualsize, end, '.');
      addchar(output, actualsize, end, '0');
    }
  }

  if(padlen > 0 && (formatter.Flags & LeftJustify))
  {
    addchars(output, actualsize, end, size_t(padlen), ' ');
  }
}

void PrintFloat(double argd, FormatterParams &formatter, bool e, bool f, bool g,
                bool uppercaseDigits, char *&output, size_t &actualsize, char *end)
{
  // extract the pieces out of the double
  uint64_t *arg64 = (uint64_t *)&argd;
  bool signbit = (*arg64 & 0x8000000000000000) ? true : false;
  uint64_t rawexp = (*arg64 & 0x7ff0000000000000) >> 52;
  int exponent = int(rawexp) - 1023;
  uint64_t mantissa = (*arg64 & 0x000fffffffffffff);

  char prepend = '\0';

  if(signbit)
    prepend = '-';
  else if(formatter.Flags & PrependPos)
    prepend = '+';
  else if(formatter.Flags & PrependSpace)
    prepend = ' ';

  // special-case handling of printing 0
  if(rawexp == 0 && mantissa == 0)
  {
    PrintFloat0(e, f, formatter, prepend, output, actualsize, end);
  }
  // handle 'special' values, inf and nan
  else if(rawexp == 0x7ff)
  {
    if(mantissa == 0)
    {
      if(signbit)
        appendstring(output, actualsize, end, uppercaseDigits ? "-INF" : "-inf");
      else
        appendstring(output, actualsize, end, uppercaseDigits ? "+INF" : "+inf");
    }
    else
    {
      appendstring(output, actualsize, end, uppercaseDigits ? "NAN" : "nan");
    }
  }
  else
  {
    // call out to grisu2 to generate digits + exponent
    char digits[18] = {0};

    int K = 0;
    int ndigits = grisu2(mantissa, exponent, digits, K);

    // this is the decimal exponent (ie. 0 if the digits are 1.2345)
    int expon = K + ndigits - 1;

    // number of digits after the decimal
    int decdigits = ndigits - expon - 1;

    // for exponential form, this is always 1 less than the total number of digits
    if(e)
      decdigits = RDCMAX(0, ndigits - 1);

    // see if we need to trim some digits (for %g, the precision is the number of
    // significant figures which is just ndigits at the moment, will be padded with 0s
    // later).
    if(decdigits > formatter.Precision || (g && ndigits > formatter.Precision))
    {
      int removedigs = decdigits - formatter.Precision;

      if(g)
        removedigs = RDCMAX(0, ndigits - formatter.Precision);

      // if we're removing all digits, just check the first to see if it should be
      // rounded up or down
      if(removedigs == ndigits)
      {
        ndigits = 1;
        if(digits[0] < '5')
        {
          digits[0] = '0';
        }
        else
        {
          // round up to "1" on the next exponent
          digits[0] = '1';
          expon++;
        }
      }
      else if(removedigs > ndigits)
      {
        ndigits = 1;
        digits[0] = '0';
        expon = 0;
      }
      else
      {
        // remove the specified number of digits
        ndigits -= removedigs;

        // round up the last digit (continually rolling up if necessary)
        // note this will look 'ahead' into the last removed digits at first
        bool carry = true;
        for(int i = ndigits - 1; i >= 0; i--)
        {
          // should we round up?
          if(digits[i + 1] >= '5')
          {
            digits[i + 1] = 0;

            // unless current digit is a 9, we can just increment it and stop
            if(digits[i] < '9')
            {
              digits[i]++;
              carry = false;
              break;
            }

            // continue (carry to next digit)
          }
          else
          {
            // didn't need to round up, everything's fine.
            carry = false;
            break;
          }

          // trim off a digit (was a 9)
          ndigits--;
          continue;
        }

        // we only get here with carry still true if digits are 9999999
        if(carry)
        {
          // round up to "1" on the next exponent
          ndigits = 1;
          digits[0] = '1';
          expon++;
        }
      }
    }

    // recalculate decimal digits with new ndigits
    decdigits = ndigits - expon - 1;
    if(e)
      decdigits = RDCMAX(0, ndigits - 1);

    // number of trailing 0s we need to pad after decimal point determined by
    // the precision
    int padtrailing0s = formatter.Precision - RDCMAX(0, decdigits);

    if(g)
    {
      // for %g if the exponent is too far out of range, we revert to exponential form
      if(expon >= formatter.Precision || expon < -4)
      {
        e = true;

        // if not alternate form, all trailing 0 digits are removed and there is no padding.
        if((formatter.Flags & AlternateForm) == 0)
        {
          while(ndigits > 1 && digits[ndigits - 1] == '0')
            ndigits--;

          padtrailing0s = 0;
        }
        else
          padtrailing0s = formatter.Precision - RDCMAX(0, ndigits);
      }
      else
      {
        padtrailing0s = formatter.Precision - RDCMAX(0, ndigits);
      }
    }

    // exponential display
    if(e)
    {
      int numwidth = 0;

      // first calculate the width of the produced output, so we can calculate any padding

      numwidth = ndigits;    // digits
      if(ndigits > 1 || (formatter.Flags & AlternateForm) || padtrailing0s > 0)
        numwidth++;    // '.'
      numwidth += padtrailing0s;
      numwidth += 2;    // 'e+' or 'e-'
      if(expon >= 1000 || expon <= -1000)
        numwidth += 4;
      else
        numwidth += 3;
      if(prepend)
        numwidth++;    // +, - or ' '

      int padlen = 0;

      if(formatter.Width != FormatterParams::NoWidth && formatter.Width > numwidth)
        padlen = formatter.Width - numwidth;

      // pad with 0s or ' 's and insert the sign character
      if(formatter.Flags & PadZeroes)
      {
        if(prepend)
          addchar(output, actualsize, end, prepend);
        addchars(output, actualsize, end, size_t(padlen), '0');
      }
      else if(padlen > 0 && (formatter.Flags & LeftJustify) == 0)
      {
        addchars(output, actualsize, end, size_t(padlen), ' ');
        if(prepend)
          addchar(output, actualsize, end, prepend);
      }
      else
      {
        if(prepend)
          addchar(output, actualsize, end, prepend);
      }

      // insert the mantissa as a 1.23456 decimal
      addchar(output, actualsize, end, digits[0]);
      if(ndigits > 1 || (formatter.Flags & AlternateForm) || padtrailing0s > 0)
        addchar(output, actualsize, end, '.');
      for(int i = 1; i < ndigits; i++)
        addchar(output, actualsize, end, digits[i]);

      // add the trailing 0s here
      if(padtrailing0s > 0)
        addchars(output, actualsize, end, size_t(padtrailing0s), '0');

      // print the e-XXX exponential
      addchar(output, actualsize, end, uppercaseDigits ? 'E' : 'e');
      if(expon >= 0)
        addchar(output, actualsize, end, '+');
      else
        addchar(output, actualsize, end, '-');

      int exponaccum = expon >= 0 ? expon : -expon;

      if(exponaccum >= 1000)
        addchar(output, actualsize, end, '0' + char(exponaccum / 1000));
      exponaccum %= 1000;

      addchar(output, actualsize, end, '0' + char(exponaccum / 100));
      exponaccum %= 100;
      addchar(output, actualsize, end, '0' + char(exponaccum / 10));
      exponaccum %= 10;
      addchar(output, actualsize, end, '0' + char(exponaccum));

      if(padlen > 0 && (formatter.Flags & LeftJustify))
      {
        addchars(output, actualsize, end, size_t(padlen), ' ');
      }
    }
    else if(digits[0] == '0' && ndigits == 1)
    {
      // if we rounded off to a 0.0, print it with special handling
      PrintFloat0(e, f, formatter, prepend, output, actualsize, end);
    }
    else
    {
      // we're printing as a normal decimal, e.g. 12345.6789

      // if %g and not in alternate form, all 0s after the decimal point are stripped
      if(g && (formatter.Flags & AlternateForm) == 0)
        while(ndigits > 1 && ndigits - 1 > expon && digits[ndigits - 1] == '0')
          ndigits--;

      int numwidth = 0;

      // first calculate the width of the produced output, so we can calculate any padding

      // always all digits are printed (after trailing 0s optionally removed above)
      numwidth = ndigits;

      if(prepend)
        numwidth++;    // prefix +, - or ' '

      // if the exponent is exactly the number of digits we have, we have one 0 to pad
      // before the decimal point, and special handling of whether to display the decimal
      // point for %g. (note that exponent 0 is mantissa x 10^0 which is 1.2345
      if(expon == ndigits)
      {
        numwidth++;    // 0 before decimal place

        // if in alternate form for %g we print a . and any trailing 0s necessary to make
        // up the precision (number of significant figures)
        if(g && (formatter.Flags & AlternateForm))
        {
          numwidth++;    // .

          if(padtrailing0s > 1)
            numwidth += (padtrailing0s - 1);
        }
        else if(!g)
        {
          // otherwise we only print the . if alternate form is specified or we need to
          // print trailing 0s
          if(padtrailing0s > 0 || (formatter.Flags & AlternateForm))
            numwidth++;    // .
          if(padtrailing0s > 0)
            numwidth += padtrailing0s;
        }
      }
      // exponent greater than ndigits means we have padding before the decimal place
      // and no values after the decimal place
      else if(expon > ndigits)
      {
        numwidth += (expon + 1 - ndigits);    // 0s between digits and decimal place
        if((!g || (formatter.Flags & AlternateForm)))
          numwidth++;    // .

        if(padtrailing0s > 0 && (!g || (formatter.Flags & AlternateForm)))
          numwidth += padtrailing0s;
      }
      else if(expon >= 0)
      {
        // expon < ndigits is true here

        if(expon < ndigits - 1 || !g || (formatter.Flags & AlternateForm))
          numwidth++;    // .

        if(g && (formatter.Flags & AlwaysDecimal))
          numwidth += 2;    // .0

        if(padtrailing0s > 0 && (!g || (formatter.Flags & AlternateForm)))
          numwidth += padtrailing0s;
      }
      else    // if(expon < 0)
      {
        numwidth += 2;               // 0.;
        numwidth += (-1 - expon);    // 0s before digits

        if(!g || (formatter.Flags & AlternateForm))
          numwidth += padtrailing0s;
      }

      int padlen = 0;

      // calculate padding and print it (0s or ' 's) with the sign character
      if(formatter.Width != FormatterParams::NoWidth && formatter.Width > numwidth)
        padlen = formatter.Width - numwidth;

      if(formatter.Flags & PadZeroes)
      {
        if(prepend)
          addchar(output, actualsize, end, prepend);
        addchars(output, actualsize, end, size_t(padlen), '0');
      }
      else if(padlen > 0 && (formatter.Flags & LeftJustify) == 0)
      {
        addchars(output, actualsize, end, size_t(padlen), ' ');
        if(prepend)
          addchar(output, actualsize, end, prepend);
      }
      else
      {
        if(prepend)
          addchar(output, actualsize, end, prepend);
      }

      // if the exponent is greater than 0 we have to handle padding,
      // placing it correctly, whether to show the decimal place or not, etc
      if(expon >= 0)
      {
        // print the digits, adding the . at the right column, as long as it's not
        // after the last column AND we are in %g that's not alternate form (ie.
        // trailing 0s and . are stripped)
        for(int i = 0; i < ndigits; i++)
        {
          addchar(output, actualsize, end, digits[i]);

          if(i == expon)
          {
            if(i < ndigits - 1 || !g || (formatter.Flags & AlternateForm))
              addchar(output, actualsize, end, '.');
          }
        }

        // handle printing trailing 0s here as well as a trailing. if it
        // wasn't printed above, and is needed for the print form.
        if(expon == ndigits)
        {
          addchar(output, actualsize, end, '0');

          if(g && (formatter.Flags & AlternateForm))
          {
            addchar(output, actualsize, end, '.');

            if(padtrailing0s > 1)
              addchars(output, actualsize, end, size_t(padtrailing0s - 1), '0');
          }
          else if(!g)
          {
            if(padtrailing0s > 0 || (formatter.Flags & AlternateForm))
              addchar(output, actualsize, end, '.');
            if(padtrailing0s > 0)
              addchars(output, actualsize, end, size_t(padtrailing0s), '0');
          }
          else if(g && (formatter.Flags & AlwaysDecimal))
          {
            addchar(output, actualsize, end, '.');
            addchar(output, actualsize, end, '0');
          }
        }
        else if(expon > ndigits)
        {
          addchars(output, actualsize, end, size_t(expon + 1 - ndigits), '0');
          if((!g || (formatter.Flags & AlternateForm)))
            addchar(output, actualsize, end, '.');

          if(padtrailing0s > 0 && (!g || (formatter.Flags & AlternateForm)))
            addchars(output, actualsize, end, size_t(padtrailing0s), '0');

          if(g && (formatter.Flags & AlwaysDecimal))
          {
            addchar(output, actualsize, end, '.');
            addchar(output, actualsize, end, '0');
          }
        }
        else
        {
          if(padtrailing0s > 0 && (!g || (formatter.Flags & AlternateForm)))
            addchars(output, actualsize, end, size_t(padtrailing0s), '0');

          if(ndigits - 1 <= expon && g && (formatter.Flags & AlwaysDecimal))
          {
            addchar(output, actualsize, end, '.');
            addchar(output, actualsize, end, '0');
          }
        }
      }
      // if exponent is less than 0 it's much easier - just print the number as
      // digits at the right column, then any trailing 0s necessary
      else
      {
        appendstring(output, actualsize, end, "0.");
        addchars(output, actualsize, end, size_t(-1 - expon), '0');

        appendstring(output, actualsize, end, digits, size_t(ndigits));

        if(padtrailing0s > 0 && (!g || (formatter.Flags & AlternateForm)))
          addchars(output, actualsize, end, size_t(padtrailing0s), '0');
      }

      if(padlen > 0 && (formatter.Flags & LeftJustify))
      {
        addchars(output, actualsize, end, size_t(padlen), ' ');
      }
    }
  }
}

void formatargument(char type, void *rawarg, FormatterParams formatter, char *&output,
                    size_t &actualsize, char *end)
{
  // print a single character (ascii or wide)
  if(type == 'c')
  {
    int arg = *(int *)rawarg;

    // left padding - character is always by definition one space wide
    if(formatter.Width != FormatterParams::NoWidth && !(formatter.Flags & LeftJustify))
      addchars(output, actualsize, end, (size_t)formatter.Width - 1, ' ');

    if(formatter.Length == Long)
    {
      wchar_t chr = (wchar_t)arg;

      // convert single wide character to UTF-8 sequence, at most
      // 4 characters
      char mbchr[4];
      int seqlen = StringFormat::Wide2UTF8(chr, mbchr);
      appendstring(output, actualsize, end, mbchr, seqlen);
    }
    else
    {
      char chr = (char)arg;
      addchar(output, actualsize, end, chr);
    }

    // right padding
    if(formatter.Width != FormatterParams::NoWidth && (formatter.Flags & LeftJustify))
      addchars(output, actualsize, end, (size_t)formatter.Width - 1, ' ');
  }
  else if(type == 's')
  {
    void *arg = *(void **)rawarg;

    if(formatter.Length == Long)
    {
      const wchar_t *ws = (const wchar_t *)arg;

      if(arg == NULL)
        ws = L"(null)";

      size_t width = (size_t)formatter.Width;
      size_t precision = (size_t)formatter.Precision;
      size_t len = wcslen(ws);
      // clip length to precision
      if(formatter.Precision != FormatterParams::NoPrecision)
        len = RDCMIN(len, precision);

      // convert the substring to UTF-8
      std::string str = StringFormat::Wide2UTF8(std::wstring(ws, ws + len));

      // add left padding, if necessary
      if(formatter.Width != FormatterParams::NoWidth && len < width &&
         !(formatter.Flags & LeftJustify))
        addchars(output, actualsize, end, width - len, ' ');

      appendstring(output, actualsize, end, str.c_str());

      // add right padding
      if(formatter.Width != FormatterParams::NoWidth && len < width && (formatter.Flags & LeftJustify))
        addchars(output, actualsize, end, width - len, ' ');
    }
    else
    {
      const char *s = (const char *)arg;

      if(arg == NULL)
        s = "(null)";

      size_t len = 0;
      size_t clipoffs = 0;
      size_t width = (size_t)formatter.Width;
      size_t precision = (size_t)formatter.Precision;

      // iterate through UTF-8 string to find its length (for padding in case
      // format width is longer than the string) or where to clip off a substring
      // (if the precision is shorter than the string)
      const char *si = s;
      while(*si && (formatter.Precision == FormatterParams::NoPrecision || precision > 0))
      {
        if((*si & 0x80) == 0)    // ascii character
        {
          si++;
        }
        else if((*si & 0xC0) == 0xC0)    // first byte of a sequence
        {
          si++;
          // skip past continuation bytes (if we hit a NULL terminator this loop will break out)
          while((*si & 0xC0) == 0x80)
            si++;
        }
        else
        {
          // invalid UTF-8 byte to encounter, bail out here.
          clipoffs = 0;
          len = 0;
          s = "";
          break;
        }

        len++;    // one more codepoint
        clipoffs = (si - s);

        // if we've reached the desired precision we can stop counting
        if(len == precision && formatter.Precision != FormatterParams::NoPrecision)
          break;
      }

      if(formatter.Width != FormatterParams::NoWidth && len < width &&
         !(formatter.Flags & LeftJustify))
        addchars(output, actualsize, end, width - len, ' ');

      appendstring(output, actualsize, end, s, clipoffs);

      if(formatter.Width != FormatterParams::NoWidth && len < width && (formatter.Flags & LeftJustify))
        addchars(output, actualsize, end, width - len, ' ');
    }
  }
  else if(type == 'p' || type == 'b' || type == 'B' || type == 'o' || type == 'x' || type == 'X' ||
          type == 'd' || type == 'i' || type == 'u')
  {
    uint64_t argu = 0;
    uint64_t numbits = 4;

    int base = 10;
    bool uppercaseDigits = false;
    bool typeUnsigned = false;

    if(type == 'p')
    {
      // fetch pointer and set settings
      argu = (uint64_t) * (void **)rawarg;
      numbits = 8 * sizeof(size_t);
      uppercaseDigits = true;
      typeUnsigned = true;
      base = 16;

      // pointer always padded to right number of hex digits
      formatter.Precision = RDCMAX(formatter.Precision, int(2 * sizeof(size_t)));

      if(formatter.Flags & AlternateForm)
        formatter.Precision += 2;
    }
    else
    {
      // fetch the parameter and set its size
      switch(formatter.Length)
      {
        default:
        case None:
        case Long:
          argu = (uint64_t) * (unsigned int *)rawarg;
          numbits = 8 * sizeof(unsigned int);
          break;
        case HalfHalf:
          numbits = 8 * sizeof(unsigned char);
          argu = (uint64_t) * (unsigned int *)rawarg;
          break;
        case Half:
          numbits = 8 * sizeof(unsigned short);
          argu = (uint64_t) * (unsigned int *)rawarg;
          break;
        case LongLong:
          numbits = 8 * sizeof(uint64_t);
          argu = (uint64_t) * (uint64_t *)rawarg;
          break;
        case SizeT:
          numbits = 8 * sizeof(size_t);
          argu = (uint64_t) * (size_t *)rawarg;
          break;
      }
      uppercaseDigits = (type < 'a');

      if(type == 'x' || type == 'X')
        base = 16;
      if(type == 'o')
        base = 8;
      if(type == 'b' || type == 'B')
        base = 2;

      if(type == 'u')
        typeUnsigned = true;
    }

    if(typeUnsigned)
      formatter.Flags &= ~(PrependPos | PrependSpace);

    PrintInteger(typeUnsigned, argu, base, numbits, formatter, uppercaseDigits, output, actualsize,
                 end);
  }
  else if(type == 'e' || type == 'E' || type == 'f' || type == 'F' || type == 'g' || type == 'G'
          //|| type == 'a' || type == 'A' // hex floats not supported
          )
  {
    bool uppercaseDigits = type < 'a';
    double argd = *(double *)rawarg;

    if(formatter.Precision == FormatterParams::NoPrecision)
      formatter.Precision = 6;

    formatter.Precision = RDCMAX(0, formatter.Precision);

    if(formatter.Precision == 0)
    {
      if(argd > 0.0f && argd < 1.0f)
        argd = argd < 0.5f ? 0.0f : 1.0f;
      else if(argd < 0.0f && argd > -1.0f)
        argd = argd > -0.5f ? 0.0f : -1.0f;
    }

    bool e = (type == 'e' || type == 'E');
    bool f = (type == 'f' || type == 'F');
    bool g = (type == 'g' || type == 'G');

    PrintFloat(argd, formatter, e, f, g, uppercaseDigits, output, actualsize, end);
  }
  else
  {
    // Unrecognised format specifier
    RDCDUMPMSG("Unrecognised % formatter");
  }
}

int utf8printf(char *buf, size_t bufsize, const char *fmt, va_list args)
{
  // format, buffer and string arguments are assumed to be UTF-8 (except wide strings).
  // note that since the format specifiers are entirely ascii, we can byte-copy safely and handle
  // UTF-8 strings, since % is not a valid UTF-8 continuation or starting character, so until we
  // reach a % we can ignore and dumbly copy any other byte

  size_t actualsize = 0;
  char *output = buf;
  char *end = buf ? buf + bufsize - 1 : NULL;

  const char *iter = fmt;

  while(*iter)
  {
    if(*iter == '%')
    {
      iter++;

      if(*iter == 0)
        RDCDUMPMSG("unterminated formatter (should be %% if you want a literal %)");

      if(*iter == '%')    // %% found, insert single % and continue copying
      {
        addchar(output, actualsize, end, *iter);
        iter++;
        continue;
      }
    }
    else
    {
      // not a %, continue copying
      addchar(output, actualsize, end, *iter);
      iter++;
      continue;
    }

    FormatterParams formatter;

    //////////////////////////////
    // now parsing an argument specifier

    // parse out 0 or more flags
    for(;;)
    {
      // if flag is found, continue looping to possibly find more flags
      // otherwise break out of this loop
      if(*iter == '-')
        formatter.Flags |= LeftJustify;
      else if(*iter == '+')
        formatter.Flags |= PrependPos;
      else if(*iter == ' ')
        formatter.Flags |= PrependSpace;
      else if(*iter == '#')
        formatter.Flags |= AlternateForm;
      else if(*iter == '@')
        formatter.Flags |= AlwaysDecimal;
      else if(*iter == '0')
        formatter.Flags |= PadZeroes;
      else
        break;

      // left justify overrides pad with zeroes
      if(formatter.Flags & LeftJustify)
        formatter.Flags &= ~PadZeroes;

      // prepend + overrides prepend ' '
      if(formatter.Flags & PrependPos)
        formatter.Flags &= ~PrependSpace;

      iter++;
    }

    // possibly parse a width. Note that width always started with 1-9 as it's decimal,
    // and 0 or - would have been picked up as a flag above
    {
      // note standard printf supports * here to read precision from a vararg before
      // the actual argument. We don't support that

      // Width found
      if(*iter >= '1' && *iter <= '9')
      {
        formatter.Width = int(*iter - '0');
        iter++;    // step to next character

        // continue while encountering digits, accumulating into width
        while(*iter >= '0' && *iter <= '9')
        {
          formatter.Width *= 10;
          formatter.Width += int(*iter - '0');
          iter++;
        }

        // unterminated formatter
        if(*iter == 0)
          RDCDUMPMSG("Unterminated % formatter found after width");
      }
      else
      {
        // no width specified
        formatter.Width = FormatterParams::NoWidth;
      }
    }

    // parse out precision. 0 is valid here, but negative isn't
    {
      // precision found
      if(*iter == '.')
      {
        iter++;

        // invalid character following '.' it should be an integer
        // note standard printf supports * here to read precision from a vararg
        if(*iter < '0' || *iter > '9')
          RDCDUMPMSG("Unexpected character expecting precision");

        formatter.Precision = int(*iter - '0');
        iter++;    // step to next character

        // continue while encountering digits, accumulating into width
        while(*iter >= '0' && *iter <= '9')
        {
          formatter.Precision *= 10;
          formatter.Precision += int(*iter - '0');
          iter++;
        }

        // unterminated formatter
        if(*iter == 0)
          RDCDUMPMSG("Unterminated % formatter found after precision");
      }
      else
      {
        // no precision specified
        formatter.Precision = FormatterParams::NoPrecision;
      }
    }

    // parse out length modifier
    {
      // length modifier characters are assumed to be disjoint with format specifiers
      // so that we don't have to look-ahead to determine if a character is a length
      // modifier or format specifier.

      if(*iter == 'z')
        formatter.Length = SizeT;
      else if(*iter == 'l')
      {
        if(*(iter + 1) == 'l')
          formatter.Length = LongLong;
        else
          formatter.Length = Long;
      }
      else if(*iter == 'L')
        formatter.Length = Long;
      else if(*iter == 'h')
      {
        if(*(iter + 1) == 'h')
          formatter.Length = HalfHalf;
        else
          formatter.Length = Half;
      }
      else
      {
        formatter.Length = None;
      }

      if(formatter.Length == HalfHalf || formatter.Length == LongLong)
        iter += 2;
      else if(formatter.Length != None)
        iter++;
    }

    // now we parse the format specifier itself and apply all the information
    // we grabbed above
    char type = *(iter++);

    // all elements fit in at most a uint64_t
    uint64_t elem;
    void *arg = (void *)&elem;

    // fetch arg here (can't pass va_list easily by reference in a portable way)
    if(type == 'c')
    {
      int *i = (int *)arg;
      *i = va_arg(args, int);
    }
    else if(type == 's' || type == 'p')
    {
      void **p = (void **)arg;
      *p = va_arg(args, void *);
    }
    else if(type == 'e' || type == 'E' || type == 'f' || type == 'F' || type == 'g' || type == 'G')
    {
      double *i = (double *)arg;
      *i = va_arg(args, double);
    }
    else if(type == 'b' || type == 'B' || type == 'o' || type == 'x' || type == 'X' ||
            type == 'd' || type == 'i' || type == 'u')
    {
      if(formatter.Length == LongLong)
      {
        uint64_t *ull = (uint64_t *)arg;
        *ull = va_arg(args, uint64_t);
      }
      else if(formatter.Length == SizeT)
      {
        size_t *s = (size_t *)arg;
        *s = va_arg(args, size_t);
      }
      else
      {
        unsigned int *u = (unsigned int *)arg;
        *u = va_arg(args, unsigned int);
      }
    }
    else
    {
      RDCDUMPMSG("Unrecognised % formatter");
    }

    formatargument(type, arg, formatter, output, actualsize, end);
  }

  // if we filled the buffer, remove any UTF-8 characters that might have been
  // truncated. We just do nothing if we encounter an invalid sequence, e.g.
  // continuation bytes without a starting byte, or two many continuation bytes
  // for a starting byte.
  if(output == end && output != NULL)
  {
    char *last = output - 1;
    int numcont = 0;
    while(last >= buf)
    {
      if((*last & 0x80) == 0)    // ascii character
      {
        break;
      }
      else if((*last & 0xC0) == 0x80)    // continuation byte
      {
        numcont++;    // count the number of continuation bytes
      }
      else if((*last & 0xC0) == 0xC0)    // first byte of a sequence
      {
        int expected = 0;

        // 110xxxxx
        if((*last & 0xE0) == 0xC0)
          expected = 1;
        // 1110xxxx
        else if((*last & 0xF0) == 0xE0)
          expected = 2;
        // 11110xxx
        else if((*last & 0xF8) == 0xF0)
          expected = 3;

        // if the sequence was truncated, remove it entirely
        if(numcont < expected)
          output = last;

        break;
      }
      last--;
    }
  }

  if(output)
    *output = 0;

  return int(actualsize);
}

#if ENABLED(ENABLE_UNIT_TESTS)

#include "3rdparty/catch/catch.hpp"

int utf8printf_wrapper(char *buf, size_t bufsize, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  int ret = utf8printf(buf, bufsize, fmt, args);

  va_end(args);

  return ret;
}

TEST_CASE("utf8printf buffer sizing", "[utf8printf]")
{
  int fourtytwo = 42;
  int largenum = 123456;
  char x = 'x';

  SECTION("NULL input buffer")
  {
    int a = utf8printf_wrapper(NULL, 0, "%d %c", fourtytwo, x);
    int b = snprintf(NULL, 0, "%d %c", fourtytwo, x);

    CHECK(a == 4);
    CHECK(a == b);
  };

  SECTION("Too small buffer")
  {
    // too small buffer
    char bufa[] = {0, 0, 0, 0, 0, 0};
    char bufb[] = {0, 0, 0, 0, 0, 0};

    int a = utf8printf_wrapper(bufa, sizeof(bufa), "%d foo", largenum);
    int b = snprintf(bufb, sizeof(bufb), "%d foo", largenum);

    RDCCOMPILE_ASSERT(sizeof(bufa) == 6, "bufa is mis-sized for test");
    CHECK(a == 10);
    CHECK(a == b);

    char ref[] = {'1', '2', '3', '4', '5', 0};

    RDCCOMPILE_ASSERT(sizeof(bufa) == sizeof(ref), "ref is mis-sized for test");

    {
      INFO("bufa is '" << std::string(bufa) << "', ref is '" << ref << "'");
      CHECK(memcmp(bufa, ref, sizeof(ref)) == 0);
    }
    {
      INFO("bufb is '" << std::string(bufb) << "', ref is '" << ref << "'");
      CHECK(memcmp(bufb, ref, sizeof(ref)) == 0);
    }

    SECTION("contains NULL terminator")
    {
      memset(bufa, 'a', sizeof(bufa));
      a = utf8printf_wrapper(bufa, sizeof(bufa), "%d foo", largenum);
      INFO("bufa is '" << std::string(bufa) << "'");
      CHECK(memcmp(bufa, ref, sizeof(ref)) == 0);
    }
  };

  SECTION("Perfectly sized buffer")
  {
    char bufa[12];
    char bufb[12];

    memset(bufa, 'a', sizeof(bufa));
    memset(bufb, 'b', sizeof(bufb));

    int a = utf8printf_wrapper(bufa, sizeof(bufa), "foobar %c %d", x, fourtytwo);
    int b = snprintf(bufb, sizeof(bufb), "foobar %c %d", x, fourtytwo);

    CHECK(a == sizeof(bufa) - 1);
    CHECK(a == b);

    char ref[] = "foobar x 42";

    RDCCOMPILE_ASSERT(sizeof(ref) <= sizeof(bufa), "ref is mis-sized for test");
    RDCCOMPILE_ASSERT(sizeof(ref) <= sizeof(bufb), "ref is mis-sized for test");

    {
      INFO("bufa is '" << std::string(bufa) << "', ref is '" << ref << "'");
      CHECK(memcmp(bufa, ref, sizeof(ref)) == 0);
    }
    {
      INFO("bufb is '" << std::string(bufb) << "', ref is '" << ref << "'");
      CHECK(memcmp(bufb, ref, sizeof(ref)) == 0);
    }
  };

  SECTION("Sizing accounts for implicit NULL")
  {
    char bufa[11];
    char bufb[11];

    memset(bufa, 'a', sizeof(bufa));
    memset(bufb, 'b', sizeof(bufb));

    int a = utf8printf_wrapper(bufa, sizeof(bufa), "foobar %c %d", x, fourtytwo);
    int b = snprintf(bufb, sizeof(bufb), "foobar %c %d", x, fourtytwo);

    CHECK(a == sizeof(bufa));
    CHECK(a == b);

    char ref[] = "foobar x 4";

    RDCCOMPILE_ASSERT(sizeof(ref) <= sizeof(bufa), "ref is mis-sized for test");
    RDCCOMPILE_ASSERT(sizeof(ref) <= sizeof(bufb), "ref is mis-sized for test");

    {
      INFO("bufa is '" << std::string(bufa) << "', ref is '" << ref << "'");
      CHECK(memcmp(bufa, ref, sizeof(ref)) == 0);
    }
    {
      INFO("bufb is '" << std::string(bufb) << "', ref is '" << ref << "'");
      CHECK(memcmp(bufb, ref, sizeof(ref)) == 0);
    }
  };

  SECTION("Too large buffer")
  {
    char bufa[20];
    char bufb[20];

    memset(bufa, 'a', sizeof(bufa));
    memset(bufb, 'b', sizeof(bufb));

    int a = utf8printf_wrapper(bufa, sizeof(bufa), "foobar %c %d", x, fourtytwo);
    int b = snprintf(bufb, sizeof(bufb), "foobar %c %d", x, fourtytwo);

    CHECK(a == 11);
    CHECK(a == b);

    char refa[20] = "foobar x 42\0aaaaaaa";
    char refb[20] = "foobar x 42\0bbbbbbb";

    refa[19] = 'a';
    refb[19] = 'b';

    {
      INFO("bufa is '" << std::string(bufa) << "', refa is '" << refa << "'");
      INFO("bufa+12 is '" << std::string(bufa + 12, bufa + 20) << "', refa+12 is '"
                          << std::string(refa + 12, refa + 20) << "'");
      CHECK(memcmp(bufa, refa, sizeof(refa)) == 0);
    }
    {
      INFO("bufb is '" << std::string(bufb) << "', refb is '" << refb << "'");
      INFO("bufb+12 is '" << std::string(bufb + 12, bufb + 20) << "', refb+12 is '"
                          << std::string(refb + 12, refb + 20) << "'");
      CHECK(memcmp(bufb, refb, sizeof(refb)) == 0);
    }
  };
};

TEST_CASE("utf8printf standard string formatters", "[utf8printf]")
{
  const wchar_t wc = L'\xe1';

  SECTION("Basic characters")
  {
    CHECK(StringFormat::Fmt("%c", 'a') == "a");
    CHECK(StringFormat::Fmt("%c", '\0') == "");
    CHECK(StringFormat::Fmt("%lc", wc) == "á");
  };

  const std::string s = "ελληνικά";
  const std::wstring ws = L"\x3b5\x3bb\x3bb\x3b7\x3bd\x3b9\x3ba\x3ac";

  SECTION("Basic strings")
  {
    CHECK(StringFormat::Fmt("%s", NULL) == "(null)");
    CHECK(StringFormat::Fmt("%ls", NULL) == "(null)");
    CHECK(StringFormat::Fmt("%s", "foobar") == "foobar");
    CHECK(StringFormat::Fmt("%s", "foo\0bar") == "foo");
    CHECK(StringFormat::Fmt("%s", s.c_str()) == s);
    CHECK(StringFormat::Fmt("%ls", ws.c_str()) == s);
  };

  SECTION("Character padding")
  {
    CHECK(StringFormat::Fmt("%0c", 'a') == "a");
    CHECK(StringFormat::Fmt("%1c", 'a') == "a");
    CHECK(StringFormat::Fmt("%4c", 'a') == "   a");
    CHECK(StringFormat::Fmt("%6c", 'a') == "     a");
    CHECK(StringFormat::Fmt("%-4c", 'a') == "a   ");
    CHECK(StringFormat::Fmt("%-6c", 'a') == "a     ");
  };

  SECTION("String padding")
  {
    CHECK(StringFormat::Fmt("%0s", "foobar") == "foobar");
    CHECK(StringFormat::Fmt("%1s", "foobar") == "foobar");
    CHECK(StringFormat::Fmt("%10s", "foobar") == "    foobar");
    CHECK(StringFormat::Fmt("%-10s", "foobar") == "foobar    ");
    CHECK(StringFormat::Fmt("%10s", s.c_str()) == "  " + s);
  };

  SECTION("String clipping")
  {
    CHECK(StringFormat::Fmt("%.4s", "foobar") == "foob");
    CHECK(StringFormat::Fmt("%.0s", "foobar") == "");
    CHECK(StringFormat::Fmt("%10.4s", "foobar") == "      foob");
    CHECK(StringFormat::Fmt("%-10.4s", "foobar") == "foob      ");
    // these string constants take 2 bytes for each code point in UTF-8, so it's 8 bytes for 4 chars
    CHECK(StringFormat::Fmt("%.4s", s.c_str()) == s.substr(0, 8));
    CHECK(StringFormat::Fmt("%.4ls", ws.c_str()) == s.substr(0, 8));
  };
};

TEST_CASE("utf8printf standard integer types", "[utf8printf]")
{
  SECTION("Unsigned integers")
  {
    CHECK(StringFormat::Fmt("%hhu", uint8_t(161)) == "161");
    CHECK(StringFormat::Fmt("%hu", uint16_t(45502)) == "45502");
    CHECK(StringFormat::Fmt("%u", uint32_t(3147483647)) == "3147483647");
    CHECK(StringFormat::Fmt("%lu", uint32_t(3147483647)) == "3147483647");
    CHECK(StringFormat::Fmt("%llu", uint64_t(19813088135479LLU)) == "19813088135479");

#if ENABLED(RDOC_X64)
    CHECK(StringFormat::Fmt("%zu", size_t(19813088135479LLU)) == "19813088135479");
    CHECK(StringFormat::Fmt("%zu", uintptr_t(19813088135479LLU)) == "19813088135479");
#else
    CHECK(StringFormat::Fmt("%zu", size_t(3147483647)) == "3147483647");
    CHECK(StringFormat::Fmt("%zu", uintptr_t(3147483647)) == "3147483647");
#endif
  };

  SECTION("Signed integers (%d)")
  {
    CHECK(StringFormat::Fmt("%hhd", int8_t(-61)) == "-61");
    CHECK(StringFormat::Fmt("%hd", int16_t(-4502)) == "-4502");
    CHECK(StringFormat::Fmt("%d", int32_t(-1147483647)) == "-1147483647");
    CHECK(StringFormat::Fmt("%ld", int32_t(-1147483647)) == "-1147483647");
    CHECK(StringFormat::Fmt("%lld", int64_t(-19813088135479LL)) == "-19813088135479");

#if ENABLED(RDOC_X64)
    CHECK(StringFormat::Fmt("%zd", intptr_t(-19813088135479LL)) == "-19813088135479");
#else
    CHECK(StringFormat::Fmt("%zd", intptr_t(-1147483647)) == "-1147483647");
#endif
  };

  SECTION("Signed integers (%i)")
  {
    CHECK(StringFormat::Fmt("%hhi", int8_t(-61)) == "-61");
    CHECK(StringFormat::Fmt("%hi", int16_t(-4502)) == "-4502");
    CHECK(StringFormat::Fmt("%i", int32_t(-1147483647)) == "-1147483647");
    CHECK(StringFormat::Fmt("%li", int32_t(-1147483647)) == "-1147483647");
    CHECK(StringFormat::Fmt("%lli", int64_t(-19813088135479LL)) == "-19813088135479");

#if ENABLED(RDOC_X64)
    CHECK(StringFormat::Fmt("%zi", intptr_t(-19813088135479LL)) == "-19813088135479");
#else
    CHECK(StringFormat::Fmt("%zi", intptr_t(-1147483647)) == "-1147483647");
#endif
  };

  SECTION("Pointers")
  {
#if ENABLED(RDOC_X64)
    CHECK(StringFormat::Fmt("%p", (void *)NULL) == "0000000000000000");
    CHECK(StringFormat::Fmt("%p", (void *)(0xDEADBEEFLLU)) == "00000000DEADBEEF");

    CHECK(StringFormat::Fmt("%#p", (void *)NULL) == "0x0000000000000000");
    CHECK(StringFormat::Fmt("%#p", (void *)(0xDEADBEEFLLU)) == "0x00000000DEADBEEF");
#else
    CHECK(StringFormat::Fmt("%p", (void *)NULL) == "00000000");
    CHECK(StringFormat::Fmt("%p", (void *)(0xDEADBEEFLLU)) == "DEADBEEF");

    CHECK(StringFormat::Fmt("%#p", (void *)NULL) == "0x00000000");
    CHECK(StringFormat::Fmt("%#p", (void *)(0xDEADBEEFLLU)) == "0xDEADBEEF");
#endif
  };

  SECTION("Hex")
  {
    CHECK(StringFormat::Fmt("%x", uint32_t(0x62a45f)) == "62a45f");
    CHECK(StringFormat::Fmt("%hx", uint16_t(0xe947)) == "e947");
    CHECK(StringFormat::Fmt("%llx", uint64_t(0x291f16b54ccLLU)) == "291f16b54cc");

    CHECK(StringFormat::Fmt("%#x", uint32_t(0x62a45f)) == "0x62a45f");
    CHECK(StringFormat::Fmt("%#X", uint32_t(0x62a45f)) == "0x62A45F");
  };

  SECTION("Binary")
  {
    CHECK(StringFormat::Fmt("%b", uint32_t(0x62a45f)) == "11000101010010001011111");
    CHECK(StringFormat::Fmt("%hb", uint16_t(0xe947)) == "1110100101000111");
    CHECK(StringFormat::Fmt("%llb", uint64_t(0x291f16b54ccLLU)) ==
          "101001000111110001011010110101010011001100");

    CHECK(StringFormat::Fmt("%#b", uint32_t(0x62a45f)) == "0b11000101010010001011111");
    CHECK(StringFormat::Fmt("%#B", uint32_t(0x62a45f)) == "0B11000101010010001011111");
  };

  SECTION("Octal")
  {
    CHECK(StringFormat::Fmt("%o", uint32_t(0627451)) == "627451");
    CHECK(StringFormat::Fmt("%ho", uint16_t(015477)) == "15477");
    CHECK(StringFormat::Fmt("%llo", uint64_t(022171675477LLU)) == "22171675477");
    CHECK(StringFormat::Fmt("%#o", uint32_t(0627451)) == "0627451");
  };
};

TEST_CASE("utf8printf printing integer formatters", "[utf8printf]")
{
  SECTION("Sign prepending")
  {
    CHECK(StringFormat::Fmt("% u", uint32_t(123)) == "123");
    CHECK(StringFormat::Fmt("%+u", uint32_t(123)) == "123");
    CHECK(StringFormat::Fmt("% d", int32_t(123)) == " 123");
    CHECK(StringFormat::Fmt("%+d", int32_t(123)) == "+123");
  };

  SECTION("Padding and alignment")
  {
    CHECK(StringFormat::Fmt("% 7u", uint32_t(123)) == "    123");
    CHECK(StringFormat::Fmt("%-7u", uint32_t(123)) == "123    ");
    CHECK(StringFormat::Fmt("%-7d", int32_t(123)) == "123    ");
    CHECK(StringFormat::Fmt("%+-7d", int32_t(123)) == "+123   ");

    CHECK(StringFormat::Fmt("%+7u", uint32_t(123)) == "    123");
    CHECK(StringFormat::Fmt("%+7d", int32_t(123)) == "   +123");
    CHECK(StringFormat::Fmt("%+ 7d", int32_t(123)) == "   +123");
    CHECK(StringFormat::Fmt("% +7d", int32_t(123)) == "   +123");

    CHECK(StringFormat::Fmt("%07u", uint32_t(123)) == "0000123");
    CHECK(StringFormat::Fmt("%-07u", uint32_t(123)) == "123    ");

    CHECK(StringFormat::Fmt("%07d", int32_t(123)) == "0000123");
    CHECK(StringFormat::Fmt("%-07d", int32_t(123)) == "123    ");

    CHECK(StringFormat::Fmt("%+07d", int32_t(123)) == "+000123");
    CHECK(StringFormat::Fmt("%+-07d", int32_t(123)) == "+123   ");

    CHECK(StringFormat::Fmt("%3d", int32_t(123456)) == "123456");
    CHECK(StringFormat::Fmt("%03d", int32_t(123456)) == "123456");
    CHECK(StringFormat::Fmt("%-3d", int32_t(123456)) == "123456");

    CHECK(StringFormat::Fmt("%3d", int32_t(-123456)) == "-123456");
    CHECK(StringFormat::Fmt("%03d", int32_t(-123456)) == "-123456");
    CHECK(StringFormat::Fmt("%-3d", int32_t(-123456)) == "-123456");

    CHECK(StringFormat::Fmt("% 7x", uint32_t(0x123)) == "    123");
    CHECK(StringFormat::Fmt("%-7x", uint32_t(0x123)) == "123    ");
    CHECK(StringFormat::Fmt("%07x", uint32_t(0x123)) == "0000123");

    CHECK(StringFormat::Fmt("%# 7x", uint32_t(0x123)) == "  0x123");
    CHECK(StringFormat::Fmt("%#-7x", uint32_t(0x123)) == "0x123  ");
    CHECK(StringFormat::Fmt("%#07x", uint32_t(0x123)) == "0x00123");
  };

  SECTION("Precision")
  {
    CHECK(StringFormat::Fmt("%.3d", int32_t(123456)) == "123456");
    CHECK(StringFormat::Fmt("%3.3d", int32_t(123456)) == "123456");
    CHECK(StringFormat::Fmt("%.9d", int32_t(123456)) == "000123456");
  };
};

TEST_CASE("utf8printf printing floats", "[utf8printf]")
{
  volatile float zero = 0.0f;
  volatile float one = 1.0f;
  volatile float negone = -1.0f;

  SECTION("Specials")
  {
    CHECK(StringFormat::Fmt("%f", one / zero) == "+inf");
    CHECK(StringFormat::Fmt("%f", negone / zero) == "-inf");
    CHECK(StringFormat::Fmt("%f", sqrt(negone)) == "nan");
    CHECK(StringFormat::Fmt("%f", -sqrt(negone)) == "nan");

    CHECK(StringFormat::Fmt("%F", one / zero) == "+INF");
    CHECK(StringFormat::Fmt("%F", negone / zero) == "-INF");
    CHECK(StringFormat::Fmt("%F", sqrt(negone)) == "NAN");
    CHECK(StringFormat::Fmt("%F", -sqrt(negone)) == "NAN");
  }

  SECTION("Basic numbers as %f")
  {
    CHECK(StringFormat::Fmt("%f", 0.0) == "0.000000");
    CHECK(StringFormat::Fmt("%f", 1.0) == "1.000000");
    CHECK(StringFormat::Fmt("%f", 2.0) == "2.000000");
    CHECK(StringFormat::Fmt("%f", 3.0) == "3.000000");
    CHECK(StringFormat::Fmt("%f", 5.0) == "5.000000");
    CHECK(StringFormat::Fmt("%f", 0.1) == "0.100000");
    CHECK(StringFormat::Fmt("%f", 0.2) == "0.200000");
    CHECK(StringFormat::Fmt("%f", 0.3) == "0.300000");
    CHECK(StringFormat::Fmt("%f", 0.5) == "0.500000");
    CHECK(StringFormat::Fmt("%f", 1.234567890123456) == "1.234568");
    CHECK(StringFormat::Fmt("%f", 1.234567123456) == "1.234567");
    CHECK(StringFormat::Fmt("%f", 12345671234.56) == "12345671234.560000");
    CHECK(StringFormat::Fmt("%f", 12345671234.56e+20) == "1234567123456000000000000000000.000000");
    CHECK(StringFormat::Fmt("%f", 12345671234.56e-20) == "0.000000");
  };

  SECTION("Basic numbers as %e")
  {
    CHECK(StringFormat::Fmt("%e", 0.0) == "0.000000e+000");
    CHECK(StringFormat::Fmt("%e", 1.0) == "1.000000e+000");
    CHECK(StringFormat::Fmt("%e", 2.0) == "2.000000e+000");
    CHECK(StringFormat::Fmt("%e", 3.0) == "3.000000e+000");
    CHECK(StringFormat::Fmt("%e", 5.0) == "5.000000e+000");
    CHECK(StringFormat::Fmt("%e", 0.1) == "1.000000e-001");
    CHECK(StringFormat::Fmt("%e", 0.2) == "2.000000e-001");
    CHECK(StringFormat::Fmt("%e", 0.3) == "3.000000e-001");
    CHECK(StringFormat::Fmt("%e", 0.5) == "5.000000e-001");
    CHECK(StringFormat::Fmt("%e", 1.234567890123456) == "1.234568e+000");
    CHECK(StringFormat::Fmt("%e", 1.234567123456) == "1.234567e+000");
    CHECK(StringFormat::Fmt("%e", 12345671234.56) == "1.234567e+010");
    CHECK(StringFormat::Fmt("%e", 12345671234.56e+20) == "1.234567e+030");
    CHECK(StringFormat::Fmt("%e", 12345671234.56e-20) == "1.234567e-010");
  };

  SECTION("Basic numbers as %g")
  {
    CHECK(StringFormat::Fmt("%g", 0.0) == "0");
    CHECK(StringFormat::Fmt("%g", 1.0) == "1");
    CHECK(StringFormat::Fmt("%g", 2.0) == "2");
    CHECK(StringFormat::Fmt("%g", 3.0) == "3");
    CHECK(StringFormat::Fmt("%g", 5.0) == "5");
    CHECK(StringFormat::Fmt("%g", 0.1) == "0.1");
    CHECK(StringFormat::Fmt("%g", 0.2) == "0.2");
    CHECK(StringFormat::Fmt("%g", 0.3) == "0.3");
    CHECK(StringFormat::Fmt("%g", 0.5) == "0.5");
    CHECK(StringFormat::Fmt("%g", 1.234567890123456) == "1.23457");
    CHECK(StringFormat::Fmt("%g", 1.23456123456) == "1.23456");
    CHECK(StringFormat::Fmt("%g", 12345671234.56) == "1.23457e+010");
    CHECK(StringFormat::Fmt("%g", 12345671234.56e+20) == "1.23457e+030");
    CHECK(StringFormat::Fmt("%g", 12345671234.56e-20) == "1.23457e-010");
  };

  SECTION("Rounding")
  {
    CHECK(StringFormat::Fmt("%.4e", 0.12345222) == "1.2345e-001");
    CHECK(StringFormat::Fmt("%.5f", 0.12345222) == "0.12345");
    CHECK(StringFormat::Fmt("%.5g", 0.12345222) == "0.12345");

    CHECK(StringFormat::Fmt("%.4e", 0.12345888) == "1.2346e-001");
    CHECK(StringFormat::Fmt("%.5f", 0.12345888) == "0.12346");
    CHECK(StringFormat::Fmt("%.5g", 0.12345888) == "0.12346");

    CHECK(StringFormat::Fmt("%.5f", 0.00000222) == "0.00000");
    CHECK(StringFormat::Fmt("%.5f", 0.00000888) == "0.00001");
    CHECK(StringFormat::Fmt("%.5f", 0.00000000222) == "0.00000");

    CHECK(StringFormat::Fmt("%.5f", 0.000888) == "0.00089");
    CHECK(StringFormat::Fmt("%.5f", 0.0000888) == "0.00009");
    CHECK(StringFormat::Fmt("%.5f", 0.00000888) == "0.00001");
    CHECK(StringFormat::Fmt("%.5f", 0.000000888) == "0.00000");
    CHECK(StringFormat::Fmt("%.10f", 0.000000888) == "0.0000008880");
  };

  SECTION("Always decimal flag")
  {
    CHECK(StringFormat::Fmt("%@g", 0.0) == "0.0");
    CHECK(StringFormat::Fmt("%@g", 1.0) == "1.0");
    CHECK(StringFormat::Fmt("%@g", 2.0) == "2.0");
    CHECK(StringFormat::Fmt("%@g", 3.0) == "3.0");
    CHECK(StringFormat::Fmt("%@g", 5.0) == "5.0");
  };

  SECTION("Precision")
  {
    CHECK(StringFormat::Fmt("%.3f", 0.0) == "0.000");
    CHECK(StringFormat::Fmt("%.3f", 1.0) == "1.000");
    CHECK(StringFormat::Fmt("%.3f", 2.0) == "2.000");
    CHECK(StringFormat::Fmt("%.3f", 0.1) == "0.100");
    CHECK(StringFormat::Fmt("%.3f", 0.2) == "0.200");
    CHECK(StringFormat::Fmt("%.3f", 1.2313) == "1.231");
  };

  SECTION("Padding")
  {
    CHECK(StringFormat::Fmt("%5.3f", 1.0) == "1.000");
    CHECK(StringFormat::Fmt("%10.3f", 1.0) == "     1.000");
    CHECK(StringFormat::Fmt("%-10.3f", 1.0) == "1.000     ");
    CHECK(StringFormat::Fmt("%010.3f", 1.0) == "000001.000");
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
