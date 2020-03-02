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

#include "lizard_counter.hpp"

#include <cstring>

namespace lizard
{
static std::vector<Value> s_empty;

LizardCounterData::LizardCounterData(LizardCounterId id, const int64_t *values, size_t length)
    : m_id(id), m_length(length), m_is_int(true)
{
  m_values = new Value[length];
  memcpy(m_values, (const void *)values, sizeof(int64_t) * length);
}

LizardCounterData::LizardCounterData(LizardCounterId id, const double *values, size_t length)
    : m_id(id), m_length(length), m_is_int(false)
{
  m_values = new Value[length];
  memcpy(m_values, (const void *)values, sizeof(double) * length);
}

LizardCounterData::~LizardCounterData()
{
  delete[] m_values;
}

LizardCounter::LizardCounter(LizardCounterId id, const char *key, const char *name, const char *title,
                             const char *description, const char *category, const double multiplier,
                             UnitType units, ClassType classType, SourceType sourceType)
    : m_id(id),
      m_key(key),
      m_name(name),
      m_title(title),
      m_description(description),
      m_category(category),
      m_multiplier(multiplier),
      m_units(units),
      m_classType(classType),
      m_sourceType(sourceType)
{
}

void LizardCounter::setInternalKey(uint64_t key)
{
  m_internalKey = key;
}

void LizardCounterDataStore::addValue(int64_t key, Value value)
{
  m_values[key].push_back(value);
}

const std::vector<Value> &LizardCounterDataStore::getValues(int64_t key) const
{
  const std::map<int64_t, std::vector<Value> >::const_iterator it = m_values.find(key);
  if(it != m_values.end())
    return it->second;
  else
    return s_empty;
}

void LizardCounterDataStore::clear()
{
  m_values.clear();
}

} /* namespace lizard */
