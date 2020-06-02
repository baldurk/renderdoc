/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Samsung Electronics (UK) Limited
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

#ifndef LIB_LIZARD_COUNTER_HPP
#define LIB_LIZARD_COUNTER_HPP

#include <map>
#include <string>
#include <vector>

namespace lizard
{
typedef uint32_t LizardCounterId;

typedef union
{
  int64_t as_int;
  double as_double;
} Value;

class LizardCounterData
{
public:
  LizardCounterData(LizardCounterId id, const int64_t *values, size_t length);
  LizardCounterData(LizardCounterId id, const double *values, size_t length);
  ~LizardCounterData(void);
  const int64_t *getIntValues() const { return reinterpret_cast<int64_t *>(m_values); }
  const double *getDoubleValues() const { return reinterpret_cast<double *>(m_values); }
  const Value *getValues() const { return m_values; }
  LizardCounterId getId() const { return m_id; }
  size_t getLength() const { return m_length; }
  bool isInt() const { return m_is_int; }
private:
  LizardCounterId m_id;
  Value *m_values;
  size_t m_length;
  bool m_is_int;
};

class LizardCounter
{
public:
  enum ClassType
  {
    CLASS_ABSOLUTE,
    CLASS_DELTA,
  };

  enum SourceType
  {
    SOURCE_GATORD,
    SOURCE_HWCPIPE_CPU,
    SOURCE_HWCPIPE_GPU,
  };

  enum UnitType
  {
    UNITS_UNKNOWN,
    UNITS_BYTE,
    UNITS_CELSIUS,
    UNITS_HZ,
    UNITS_MHZ,
    UNITS_PAGES,
    UNITS_RPM,
    UNITS_S,
    UNITS_V,
    UNITS_MV
  };

  LizardCounter() : m_id(0), m_multiplier(1) {}
  LizardCounter(LizardCounterId id, const char *key, const char *name, const char *title,
                const char *description, const char *category, const double multiplier,
                UnitType units, ClassType classType, SourceType sourceType);
  ~LizardCounter(void) {}
  LizardCounterId id(void) const { return m_id; }
  const char *key(void) const { return m_key.c_str(); }
  const char *name(void) const { return m_name.c_str(); }
  const char *title(void) const { return m_title.c_str(); }
  const char *description(void) const { return m_description.c_str(); }
  const char *category(void) const { return m_category.c_str(); }
  double multiplier(void) const { return m_multiplier; }
  UnitType units(void) const { return m_units; }
  ClassType classType(void) const { return m_classType; }
  SourceType sourceType(void) const { return m_sourceType; }
  uint64_t internalKey(void) const { return m_internalKey; }
  void setInternalKey(uint64_t key);

private:
  const LizardCounterId m_id;
  std::string m_key;
  std::string m_name;
  std::string m_title;
  std::string m_description;
  std::string m_category;
  double m_multiplier;
  UnitType m_units;
  ClassType m_classType;
  SourceType m_sourceType;
  uint64_t m_internalKey;
};

class LizardCounterDataStore
{
public:
  void addValue(int64_t key, Value value);
  const std::vector<Value> &getValues(int64_t key) const;
  void clear();

private:
  std::map<int64_t, std::vector<Value>> m_values;
};

}; /* namespace lizard */

#endif /* LIB_LIZARD_COUNTER_HPP */
