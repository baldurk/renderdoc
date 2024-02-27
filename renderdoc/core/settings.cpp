/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "settings.h"
#include "api/replay/structured_data.h"
#include "common/formatting.h"
#include "serialise/streamio.h"
#include "core.h"

#include "3rdparty/pugixml/pugixml.hpp"

static const rdcliteral debugOnlyString = "DEBUG VARIABLE: Read-only in stable builds."_lit;

static rdcstr valueString(const SDObject *o)
{
  if(o->type.basetype == SDBasic::String)
    return o->data.str;

  if(o->type.basetype == SDBasic::UnsignedInteger)
    return StringFormat::Fmt("%llu", o->data.basic.u);

  if(o->type.basetype == SDBasic::SignedInteger)
    return StringFormat::Fmt("%lld", o->data.basic.i);

  if(o->type.basetype == SDBasic::Float)
    return StringFormat::Fmt("%lf", o->data.basic.d);

  if(o->type.basetype == SDBasic::Boolean)
    return o->data.basic.b ? "True" : "False";

  if(o->type.basetype == SDBasic::Array)
    return StringFormat::Fmt("[%zu]", o->NumChildren());

  return "{}";
}

struct xml_stream_writer : pugi::xml_writer
{
  StreamWriter &stream;

  xml_stream_writer(StreamWriter &writer) : stream(writer) {}
  void write(const void *data, size_t size) { stream.Write(data, size); }
};

static SDObject *makeSDObject(const rdcinflexiblestr &name, SDBasic type, pugi::xml_node &value)
{
  switch(type)
  {
    case SDBasic::UnsignedInteger: return makeSDObject(name, (uint64_t)value.text().as_ullong());
    case SDBasic::SignedInteger: return makeSDObject(name, (int64_t)value.text().as_llong());
    case SDBasic::String: return makeSDObject(name, value.text().as_string());
    case SDBasic::Float: return makeSDObject(name, value.text().as_float());
    case SDBasic::Boolean: return makeSDObject(name, value.text().as_bool());
    case SDBasic::Character: return makeSDObject(name, value.text().as_string()[0]);
    default: break;
  }

  return NULL;
}

static void saveSDObject(const SDObject &value, pugi::xml_node obj)
{
  switch(value.type.basetype)
  {
    case SDBasic::Resource:
    case SDBasic::Enum:
    case SDBasic::UnsignedInteger: obj.text() = value.data.basic.u; break;
    case SDBasic::SignedInteger: obj.text() = value.data.basic.i; break;
    case SDBasic::String: obj.text() = value.data.str.c_str(); break;
    case SDBasic::Float: obj.text() = value.data.basic.d; break;
    case SDBasic::Boolean: obj.text() = value.data.basic.b; break;
    case SDBasic::Character:
    {
      char str[2] = {value.data.basic.c, '\0'};
      obj.text().set(str);
      break;
    }
    default: RDCERR("Unexpected case");
  }
}

static void Config2XML(pugi::xml_node &parent, const SDObject &child)
{
  pugi::xml_node obj = parent.append_child(child.name.c_str());

  if(child.type.name == "category"_lit)
  {
    for(size_t i = 0; i < child.NumChildren(); i++)
      Config2XML(obj, *child.GetChild(i));
  }
  else
  {
    const SDObject *value = child.FindChild("value");

    parent.insert_child_before(pugi::node_comment, obj)
        .set_value((" " + child.FindChild("description")->data.str + " ").c_str());

    obj.append_attribute("type") = ToStr(value->type.basetype).c_str();
    if(value->type.basetype == SDBasic::Array)
    {
      if(value->NumChildren() > 0)
        obj.append_attribute("elemtype") = ToStr(value->GetChild(0)->type.basetype).c_str();
      else
        obj.append_attribute("elemtype") = "";

      for(size_t o = 0; o < value->NumChildren(); o++)
        saveSDObject(*value->GetChild(o), obj.append_child("item"));
    }
    else
    {
      saveSDObject(*value, obj);
    }
  }
}

static SDBasic getType(const char *typeStr)
{
  if(!typeStr)
    return SDBasic::Chunk;

  const SDBasic types[] = {
      SDBasic::Array,         SDBasic::String, SDBasic::UnsignedInteger,
      SDBasic::SignedInteger, SDBasic::Float,  SDBasic::Boolean,
  };

  static rdcarray<rdcstr> basicTypeStrings;
  if(basicTypeStrings.empty())
  {
    for(SDBasic t : types)
      basicTypeStrings.push_back(ToStr(t));
  }

  int idx = basicTypeStrings.indexOf(typeStr);
  if(idx >= 0)
    return types[idx];

  return SDBasic::Chunk;
}

static SDObject *XML2Config(pugi::xml_node &obj)
{
  SDObject *ret =
      new SDObject(rdcstr(obj.name()), obj.attribute("type") ? "setting"_lit : "category"_lit);

  if(ret->type.name == "category"_lit)
  {
    uint32_t i = 0;
    for(pugi::xml_node child = obj.first_child(); child; child = child.next_sibling())
    {
      if(child.type() == pugi::node_comment)
        continue;

      SDObject *childObj = XML2Config(child);
      if(childObj)
      {
        ret->AddAndOwnChild(childObj);
      }
      else
      {
        RDCERR("Error converting child %u config option '%s'", i, ret->name.c_str());
        delete ret;
        return NULL;
      }

      i++;
    }
  }
  else
  {
    pugi::xml_node value = obj.first_child();
    rdcstr description = obj.previous_sibling().value();
    description.trim();

    ret->AddAndOwnChild(makeSDObject("description"_lit, description));

    SDObject *valueObj = NULL;

    SDBasic type = getType(obj.attribute("type").as_string());

    if(type == SDBasic::Array)
    {
      type = getType(obj.attribute("elemtype").as_string());
      valueObj = makeSDArray("value"_lit);

      uint32_t i = 0;
      for(pugi::xml_node el = value; el; el = el.next_sibling())
      {
        SDObject *childObj = makeSDObject("$el"_lit, type, el);

        if(childObj)
        {
          valueObj->AddAndOwnChild(childObj);
        }
        else
        {
          RDCERR("Error converting array value %u in config option '%s'", i, ret->name.c_str());
          delete valueObj;
          delete ret;
          return NULL;
        }

        i++;
      }
    }
    else
    {
      valueObj = makeSDObject("value"_lit, type, value);

      if(!valueObj)
      {
        RDCERR("Unexpected type %u of attribute %s", type, ret->name.c_str());
        delete ret;
        return NULL;
      }
    }

    ret->AddAndOwnChild(valueObj);
  }

  return ret;
}

static SDObject *importXMLConfig(StreamReader &stream)
{
  rdcstr buf;
  buf.resize((size_t)stream.GetSize());
  stream.Read(buf.data(), buf.size());

  pugi::xml_document doc;
  doc.load_string(buf.c_str(), pugi::parse_default | pugi::parse_comments);

  pugi::xml_node root = doc.child("config");

  SDObject *ret = new SDObject("config"_lit, "config"_lit);

  if(root)
  {
    for(pugi::xml_node child = root.first_child(); child; child = child.next_sibling())
    {
      SDObject *childObj = XML2Config(child);
      if(childObj)
        ret->AddAndOwnChild(XML2Config(child));
    }
  }

  return ret;
}

static void exportXMLConfig(StreamWriter &stream, const SDObject *obj)
{
  pugi::xml_document doc;

  pugi::xml_node xRoot = doc.append_child("config");
  xRoot.append_attribute("version") = (uint32_t)1;

  for(size_t o = 0; o < obj->NumChildren(); o++)
    Config2XML(xRoot, *obj->GetChild(o));

  xml_stream_writer writer(stream);
  doc.save(writer, "  ", pugi::format_default | pugi::format_no_empty_element_tags);
}

static bool MergeConfigValues(const rdcstr &prefix, SDObject *dstConfig, const SDObject *srcConfig,
                              bool updateDescs)
{
  bool ret = false;

  // for every child in the destination, see if it has a source node. If not, we're out of date
  for(size_t i = 0; i < dstConfig->NumChildren(); i++)
    ret |= (srcConfig->FindChild(dstConfig->GetChild(i)->name) == NULL);

  // for every child in the source
  for(size_t i = 0; i < srcConfig->NumChildren(); i++)
  {
    // see if it's present in the destination
    const SDObject *srcChild = srcConfig->GetChild(i);
    SDObject *dstChild = dstConfig->FindChild(srcChild->name);

    if(dstChild)
    {
      // if present, merge the values
      rdcstr prefixedChild = prefix + dstChild->name;

      if(dstChild->type.name == "category"_lit)
      {
        // recurse if this child is not a setting node
        ret |= MergeConfigValues(prefixedChild + ".", dstChild, srcChild, updateDescs);
      }
      else
      {
        SDObject *dstVal = dstChild->FindChild("value");
        const SDObject *srcVal = srcChild->FindChild("value");
        SDObject *dstDesc = dstChild->FindChild("description");
        const SDObject *srcDesc = srcChild->FindChild("description");

        bool customised = !srcVal->HasEqualValue(dstVal);

        // otherwise see if the value is customised, and if so log the change
        if(customised)
        {
          rdcstr oldVal = valueString(dstVal);
          rdcstr newVal = valueString(srcVal);

          RDCLOG("%s has been customised from %s to %s", (prefix + dstChild->name).c_str(),
                 oldVal.c_str(), newVal.c_str());

#if RENDERDOC_STABLE_BUILD
          if(rdcstr(dstDesc->data.str).contains(debugOnlyString))
          {
            RDCWARN("%s customisation will not apply - read only in this build",
                    (prefix + dstChild->name).c_str());
          }
#endif

          // always set the value. For a debug-only setting this will do nothing but we want to
          // update our config value with the user's in case we're going to write out some new
          // values/descriptions
          dstVal->data.str = srcVal->data.str;
          memcpy(&dstVal->data.basic, &srcVal->data.basic, sizeof(dstVal->data.basic));

          dstVal->DeleteChildren();

          for(size_t c = 0; c < srcVal->NumChildren(); c++)
            dstVal->DuplicateAndAddChild(srcVal->GetChild(c));
        }

        // if the description has changed from the loaded, need to write the new one
        if(dstDesc->data.str != srcDesc->data.str)
        {
          if(updateDescs)
            dstDesc->data.str = srcDesc->data.str;
          ret |= true;
        }
      }
    }
    else
    {
      // child wasn't in the destination config, out of date
      ret |= true;

      // if we're copying nodes, do that now
      dstConfig->DuplicateAndAddChild(srcChild);
    }
  }

  return ret;
}

const bool &ConfigVarRegistration<bool>::value()
{
  // avoid warnings on stupid compilers
  (void)tmp;
  return obj->data.basic.b;
}

const uint64_t &ConfigVarRegistration<uint64_t>::value()
{
  (void)tmp;
  return obj->data.basic.u;
}

const uint32_t &ConfigVarRegistration<uint32_t>::value()
{
  tmp = obj->data.basic.u & 0xFFFFFFFFU;
  return tmp;
}

const rdcstr &ConfigVarRegistration<rdcstr>::value()
{
  tmp = obj->data.str;
  return tmp;
}

template <typename T>
rdcstr DefValString(const T &el)
{
  return ToStr(el);
}

// this one needs a special implementation unfortunately to convert
const rdcarray<rdcstr> &ConfigVarRegistration<rdcarray<rdcstr>>::value()
{
  tmp.resize(obj->NumChildren());
  for(size_t i = 0; i < tmp.size(); i++)
    tmp[i] = obj->GetChild(i)->data.str;

  return tmp;
}

rdcstr DefValString(const rdcarray<rdcstr> &el)
{
  rdcstr ret = "[";
  for(size_t i = 0; i < el.size(); i++)
  {
    if(i != 0)
      ret += ", ";
    ret += el[i];
  }
  ret += "]";
  return ret;
}

inline SDObject *makeSDObject(const rdcinflexiblestr &name, const rdcarray<rdcstr> &vals)
{
  SDObject *ret = new SDObject(name, "array"_lit);
  ret->type.basetype = SDBasic::Array;
  for(const rdcstr &s : vals)
    ret->AddAndOwnChild(makeSDObject("$el"_lit, s));
  return ret;
}

#define CONFIG_SUPPORT_TYPE(T)                                                            \
  ConfigVarRegistration<T>::ConfigVarRegistration(rdcliteral name, const T &defaultValue, \
                                                  bool debugOnly, rdcliteral description) \
  {                                                                                       \
    rdcstr settingName = name;                                                            \
    settingName = settingName.substr(settingName.find_last_of("_") + 1);                  \
                                                                                          \
    rdcstr desc = name;                                                                   \
    desc += "\n\n";                                                                       \
    for(char &c : desc)                                                                   \
      if(c == '_')                                                                        \
        c = '.';                                                                          \
    desc += description;                                                                  \
                                                                                          \
    desc += "\n\nDefault value: '" + DefValString(defaultValue) + "'";                    \
    if(debugOnly)                                                                         \
    {                                                                                     \
      desc += "\n";                                                                       \
      desc += debugOnlyString;                                                            \
    }                                                                                     \
                                                                                          \
    SDObject *setting = new SDObject(settingName, "setting"_lit);                         \
    setting->AddAndOwnChild(makeSDObject("value"_lit, defaultValue));                     \
    setting->AddAndOwnChild(makeSDObject("key"_lit, name));                               \
    setting->AddAndOwnChild(makeSDObject("default"_lit, defaultValue));                   \
    setting->AddAndOwnChild(makeSDObject("description"_lit, desc));                       \
                                                                                          \
    obj = setting->GetChild(0);                                                           \
                                                                                          \
    RenderDoc::Inst().RegisterSetting(name, setting);                                     \
  }

CONFIG_SUPPORT_TYPE(bool)
CONFIG_SUPPORT_TYPE(uint64_t)
CONFIG_SUPPORT_TYPE(uint32_t)
CONFIG_SUPPORT_TYPE(rdcstr)
CONFIG_SUPPORT_TYPE(rdcarray<rdcstr>)

void RenderDoc::ProcessConfig()
{
  rdcstr confFile = FileIO::GetAppFolderFilename("renderdoc.conf");

  RDCLOG("Loading config from %s", confFile.c_str());

  SDObject *loadedConfig = NULL;
  {
    StreamReader reader(FileIO::fopen(confFile, FileIO::ReadBinary));

    loadedConfig = importXMLConfig(reader);
  }

  // iterate through the current config, and update any values that are found in the loaded config.
  // returns true if the loaded config is out of date (i.e. there's a value we have which isn't
  // present at all, or the descriptions in the loaded config are old).
  bool outofDate = ::MergeConfigValues(rdcstr(), m_Config, loadedConfig, false);

  // in the replay application, write it back out again if it's out of date. This
  // refreshes the config without changing any customised values and means the user can always edit
  // the files on disk
  if(IsReplayApp() && outofDate)
  {
    bool success = false;

    // merge the current config into the loaded config. Values that overlap will have been updated
    // with the user's values above, so all that's left is to add new values which aren't in the
    // config or update descriptions
    MergeConfigValues(rdcstr(), loadedConfig, m_Config, true);

    {
      StreamWriter writer(FileIO::fopen(confFile + ".tmp", FileIO::WriteBinary), Ownership::Stream);

      exportXMLConfig(writer, loadedConfig);

      // only overwrite the config if there were no errors here
      success = !writer.IsErrored();
    }

    // if we successfully wrote the file, move it over the original
    if(success)
      FileIO::Move(confFile + ".tmp", confFile, true);
  }

  // delete the loaded config if we have it
  delete loadedConfig;
}

void RenderDoc::SaveConfigSettings()
{
  if(IsReplayApp())
  {
    rdcstr confFile = FileIO::GetAppFolderFilename("renderdoc.conf");

    bool success = false;

    {
      StreamWriter writer(FileIO::fopen(confFile + ".tmp", FileIO::WriteBinary), Ownership::Stream);

      exportXMLConfig(writer, m_Config);

      // only overwrite the config if there were no errors here
      success = !writer.IsErrored();
    }

    // if we successfully wrote the file, move it over the original
    if(success)
      FileIO::Move(confFile + ".tmp", confFile, true);
  }
}

const SDObject *RenderDoc::GetConfigSetting(const rdcstr &settingPath)
{
  return FindConfigSetting(settingPath);
}

SDObject *RenderDoc::SetConfigSetting(const rdcstr &settingPath)
{
  return FindConfigSetting(settingPath);
}

SDObject *RenderDoc::FindConfigSetting(const rdcstr &settingPath)
{
  if(settingPath.empty())
    return m_Config;

  SDObject *cur = m_Config;

  rdcstr path = settingPath;
  int idx = path.find_first_of("_.");
  while(idx >= 0)
  {
    rdcstr node = path.substr(0, idx);
    path.erase(0, idx + 1);

    SDObject *child = cur->FindChild(node);
    if(!child)
      return NULL;

    cur = child;
    idx = path.find_first_of("_.");
  }

  SDObject *obj = cur->FindChild(path);
  if(obj)
    return obj->FindChild("value");

  return NULL;
}

void RenderDoc::RegisterSetting(const rdcstr &settingPath, SDObject *setting)
{
  SDObject *cur = m_Config;

  if(cur == NULL)
    cur = m_Config = new SDObject("config"_lit, "config"_lit);

  rdcstr path = settingPath;
  int idx = path.indexOf('_');
  while(idx >= 0)
  {
    rdcstr node = path.substr(0, idx);
    path.erase(0, idx + 1);

    SDObject *child = cur->FindChild(node);
    if(!child)
    {
      child = new SDObject(node, "category"_lit);
      auto it =
          std::lower_bound(cur->begin(), cur->end(), child,
                           [](const SDObject *a, const SDObject *b) { return a->name < b->name; });
      cur->InsertAndOwnChild(it - cur->begin(), child);
    }

    cur = child;

    idx = path.indexOf('_');
  }

  SDObject *obj = cur->FindChild(path);
  if(obj != NULL)
    RDCFATAL("Duplicate setting %s", settingPath.c_str());

  cur->AddAndOwnChild(setting);
}
