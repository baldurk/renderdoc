/*
  Copyright (c) 2009, Hideyuki Tanaka
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  * Neither the name of the <organization> nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY <copyright holder> ''AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL <copyright holder> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <stdexcept>
#include <typeinfo>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <cstdint>

#ifdef max
#undef max
#endif

namespace cmdline{

namespace detail{

template <typename Target, typename Source, bool Same>
class lexical_cast_t{
public:
  static Target cast(const Source &arg){
    Target ret;
    std::stringstream ss;
    if (!(ss<<arg && ss>>ret && ss.eof()))
      throw std::bad_cast();
    
    return ret;
  }
};

template <typename Target, typename Source>
class lexical_cast_t<Target, Source, true>{
public:
  static Target cast(const Source &arg){
    return arg;
  }  
};

template <typename Source>
class lexical_cast_t<std::string, Source, false>{
public:
  static std::string cast(const Source &arg){
    std::ostringstream ss;
    ss<<arg;
    return ss.str();
  }
};

template <typename Target>
class lexical_cast_t<Target, std::string, false>{
public:
  static Target cast(const std::string &arg){
    Target ret;
    std::istringstream ss(arg);
    if (!(ss>>ret && ss.eof()))
      throw std::bad_cast();
    return ret;
  }
};

template <typename T1, typename T2>
struct is_same {
  static const bool value = false;
};

template <typename T>
struct is_same<T, T>{
  static const bool value = true;
};

template<typename Target, typename Source>
Target lexical_cast(const Source &arg)
{
  return lexical_cast_t<Target, Source, detail::is_same<Target, Source>::value>::cast(arg);
}

template <class T>
std::string readable_typename();

template <class T>
std::string default_value(T def)
{
  return detail::lexical_cast<std::string>(def);
}

template <>
inline std::string readable_typename<std::string>()
{
  return "string";
}

template <>
inline std::string readable_typename<int>()
{
  return "int";
}

template <>
inline std::string readable_typename<uint32_t>()
{
  return "uint";
}

} // detail

//-----

class cmdline_error : public std::exception {
public:
  cmdline_error(const std::string &msg): msg(msg){}
  ~cmdline_error() throw() {}
  const char *what() const throw() { return msg.c_str(); }
private:
  std::string msg;
};

template <class T>
struct default_reader{
  T operator()(const std::string &str){
    return detail::lexical_cast<T>(str);
  }
  std::string description() const { return ""; }
};

template <class T>
struct range_reader{
  range_reader(const T &low, const T &high): low(low), high(high) {}
  T operator()(const std::string &s) const {
    T ret=default_reader<T>()(s);
    if (!(ret>=low && ret<=high))
      throw cmdline::cmdline_error(description());
    return ret;
  }
  std::string description() const { return "Must be within [" + detail::lexical_cast<std::string>(low) + ", " + detail::lexical_cast<std::string>(high) + "]"; }
private:
  T low, high;
};

template <class T>
range_reader<T> range(const T &low, const T &high)
{
  return range_reader<T>(low, high);
}

template <class T>
struct oneof_reader{
  T operator()(const std::string &s){
    T ret=default_reader<T>()(s);
    if (std::find(alt.begin(), alt.end(), ret)==alt.end())
      throw cmdline::cmdline_error("'" + s + "' is not one of the accepted values");
    return ret;
  }
  void add(const T &v){ alt.push_back(v); }
  std::string description() const
  {
    std::string ret = "Options are:";
    for(size_t i=0; i < alt.size(); i++)
      ret += "\n  * " + detail::lexical_cast<std::string>(alt[i]);
    return ret;
  }
private:
  std::vector<T> alt;
};

template <class T>
oneof_reader<T> oneof(T a1)
{
  oneof_reader<T> ret;
  ret.add(a1);
  return ret;
}

template <class T>
oneof_reader<T> oneof(T a1, T a2)
{
  oneof_reader<T> ret;
  ret.add(a1);
  ret.add(a2);
  return ret;
}

template <class T>
oneof_reader<T> oneof(T a1, T a2, T a3)
{
  oneof_reader<T> ret;
  ret.add(a1);
  ret.add(a2);
  ret.add(a3);
  return ret;
}

template <class T>
oneof_reader<T> oneof(T a1, T a2, T a3, T a4)
{
  oneof_reader<T> ret;
  ret.add(a1);
  ret.add(a2);
  ret.add(a3);
  ret.add(a4);
  return ret;
}

template <class T>
oneof_reader<T> oneof(T a1, T a2, T a3, T a4, T a5)
{
  oneof_reader<T> ret;
  ret.add(a1);
  ret.add(a2);
  ret.add(a3);
  ret.add(a4);
  ret.add(a5);
  return ret;
}

template <class T>
oneof_reader<T> oneof(T a1, T a2, T a3, T a4, T a5, T a6)
{
  oneof_reader<T> ret;
  ret.add(a1);
  ret.add(a2);
  ret.add(a3);
  ret.add(a4);
  ret.add(a5);
  ret.add(a6);
  return ret;
}

template <class T>
oneof_reader<T> oneof(T a1, T a2, T a3, T a4, T a5, T a6, T a7)
{
  oneof_reader<T> ret;
  ret.add(a1);
  ret.add(a2);
  ret.add(a3);
  ret.add(a4);
  ret.add(a5);
  ret.add(a6);
  ret.add(a7);
  return ret;
}

template <class T>
oneof_reader<T> oneof(T a1, T a2, T a3, T a4, T a5, T a6, T a7, T a8)
{
  oneof_reader<T> ret;
  ret.add(a1);
  ret.add(a2);
  ret.add(a3);
  ret.add(a4);
  ret.add(a5);
  ret.add(a6);
  ret.add(a7);
  ret.add(a8);
  return ret;
}

template <class T>
oneof_reader<T> oneof(T a1, T a2, T a3, T a4, T a5, T a6, T a7, T a8, T a9)
{
  oneof_reader<T> ret;
  ret.add(a1);
  ret.add(a2);
  ret.add(a3);
  ret.add(a4);
  ret.add(a5);
  ret.add(a6);
  ret.add(a7);
  ret.add(a8);
  ret.add(a9);
  return ret;
}

template <class T>
oneof_reader<T> oneof(T a1, T a2, T a3, T a4, T a5, T a6, T a7, T a8, T a9, T a10)
{
  oneof_reader<T> ret;
  ret.add(a1);
  ret.add(a2);
  ret.add(a3);
  ret.add(a4);
  ret.add(a5);
  ret.add(a6);
  ret.add(a7);
  ret.add(a8);
  ret.add(a9);
  ret.add(a10);
  return ret;
}

//-----

class parser{
public:
  parser(){
    stop = false;
  }
  ~parser(){
    for (std::map<std::string, option_base*>::iterator p=options.begin();
         p!=options.end(); p++)
      delete p->second;
  }

  void add(const std::string &name,
           char short_name=0,
           const std::string &desc=""){
    if (options.count(name)) throw cmdline_error("multiple definition: "+name);
    options[name]=new option_without_value(name, short_name, desc);
    ordered.push_back(options[name]);
  }

  template <class T>
  void add(const std::string &name,
           char short_name=0,
           const std::string &desc="",
           bool need=true,
           const T def=T()){
    add(name, short_name, desc, need, def, default_reader<T>());
  }

  template <class T, class F>
  void add(const std::string &name,
           char short_name=0,
           const std::string &desc="",
           bool need=true,
           const T def=T(),
           F reader=F()){
    if (options.count(name)) throw cmdline_error("multiple definition: "+name);
    options[name]=new option_with_value_with_reader<T, F>(name, short_name, need, def, desc, reader);
    ordered.push_back(options[name]);
  }

  void set_header(const std::string &f){
    hdr=f;
  }

  void set_footer(const std::string &f){
    ftr=f;
  }

  void stop_at_rest(bool s){
    stop=s;
  }

  void set_program_name(const std::string &name){
    prog_name=name;
  }

  bool exist(const std::string &name) const {
    if (options.count(name)==0) throw cmdline_error("there is no flag: --"+name);
    return options.find(name)->second->has_set();
  }

  template <class T>
  const T &get(const std::string &name) const {
    if (options.count(name)==0) throw cmdline_error("there is no flag: --"+name);
    const option_with_value<T> *p=dynamic_cast<const option_with_value<T>*>(options.find(name)->second);
    if (p==NULL) throw cmdline_error("type mismatch flag '"+name+"'");
    return p->get();
  }

  const std::vector<std::string> &rest() const {
    return others;
  }

  bool parse(const std::vector<std::string> &args, bool processed_arg0 = false){
    int argc=static_cast<int>(args.size());
    std::vector<const char*> argv(argc);

    for (int i=0; i<argc; i++)
      argv[i]=args[i].c_str();

    return parse(argc, argv.data(), processed_arg0);
  }

  bool parse(int argc, const char * const argv[], bool processed_arg0 = false){
    errors.clear();
    others.clear();

    if (argc<1 && !processed_arg0){
      errors.push_back("Missing program name as argv[0]");
      return false;
    }
    if (prog_name=="" && !processed_arg0)
      prog_name=argv[0];

    std::map<char, std::string> lookup;
    for (std::map<std::string, option_base*>::iterator p=options.begin();
         p!=options.end(); p++){
      if (p->first.length()==0) continue;
      char initial=p->second->short_name();
      if (initial){
        if (lookup.count(initial)>0){
          lookup[initial]="";
          errors.push_back(std::string("short option '")+initial+"' is ambiguous");
          return false;
        }
        else lookup[initial]=p->first;
      }
    }

    bool found_others = false;

    for (int i=processed_arg0 ? 0 : 1; i<argc; i++){
      if (!found_others && strncmp(argv[i], "--", 2)==0){
        const char *p=strchr(argv[i]+2, '=');
        if (p){
          std::string name(argv[i]+2, p);
          std::string val(p+1);
          set_option(name, val);
        }
        else{
          std::string name(argv[i]+2);
          if (options.count(name)==0){
            if (stop){
              found_others=true;
              others.push_back(argv[i]);
            }
            else{
              errors.push_back("undefined option: --"+name);
            }
            continue;
          }
          if (options[name]->has_value()){
            if (i+1>=argc){
              errors.push_back("option needs value: --"+name);
              continue;
            }
            else{
              i++;
              set_option(name, argv[i]);
            }
          }
          else{
            set_option(name);
          }
        }
      }
      else if (!found_others && strncmp(argv[i], "-", 1)==0){
        if (!argv[i][1]) continue;
        char last=argv[i][1];
        for (int j=2; argv[i][j]; j++){
          last=argv[i][j];
          if (lookup.count(argv[i][j-1])==0){
            if (stop){
              found_others=true;
              others.push_back(argv[i]);
            }
            else{
              errors.push_back(std::string("undefined short option: -")+argv[i][j-1]);
            }
            continue;
          }
          if (lookup[argv[i][j-1]]==""){
            errors.push_back(std::string("ambiguous short option: -")+argv[i][j-1]);
            continue;
          }
          set_option(lookup[argv[i][j-1]]);
        }

        if (lookup.count(last)==0){
          if (stop){
            found_others=true;
            others.push_back(argv[i]);
          }
          else{
            errors.push_back(std::string("undefined short option: -")+last);
          }
          continue;
        }
        if (lookup[last]==""){
          errors.push_back(std::string("ambiguous short option: -")+last);
          continue;
        }

        if (i+1<argc && options[lookup[last]]->has_value()){
          set_option(lookup[last], argv[i+1]);
          i++;
        }
        else{
          set_option(lookup[last]);
        }
      }
      else{
        others.push_back(argv[i]);

        if(stop)
          found_others = true;
      }
    }

    for (std::map<std::string, option_base*>::iterator p=options.begin();
         p!=options.end(); p++)
      if (!p->second->valid())
        errors.push_back("need option: --"+std::string(p->first));

    return errors.size()==0;
  }

  void parse_check(const std::vector<std::string> &args, bool processed_arg0 = false){
    if (!options.count("help"))
      add("help", '?', "print this message");
    check((int)args.size(), parse(args, processed_arg0));
  }

  std::string error_full() const{
    std::ostringstream oss;
    for (size_t i=0; i<errors.size(); i++)
      oss<<errors[i]<<std::endl;
    return oss.str();
  }

  std::string usage() const {
    std::ostringstream oss;
    oss<<"usage: "<<prog_name<<" "<<hdr<<(hdr.empty() ? "" : " ");
    for (size_t i=0; i<ordered.size(); i++){
      if (ordered[i]->must())
        oss<<ordered[i]->short_description()<<" ";
    }
    
    oss<<"[options ...] "<<ftr<<std::endl<<std::endl;
    oss<<"options:"<<std::endl;

    size_t max_width=0;
    for (size_t i=0; i<ordered.size(); i++){
      max_width=std::max(max_width, ordered[i]->name().length());
    }
    for (size_t i=0; i<ordered.size(); i++){
      if (ordered[i]->short_name()){
        oss<<"  -"<<ordered[i]->short_name()<<", ";
      }
      else{
        oss<<"      ";
      }

      oss<<"--"<<ordered[i]->name();
      for (size_t j=ordered[i]->name().length(); j<max_width+4; j++)
        oss<<' ';

      std::string desc = ordered[i]->description();

      // allow multiline descriptions, align them properly
      size_t nl = desc.find('\n');

      while(nl != std::string::npos)
      {
        std::string firstline = desc.substr(0, nl);
        desc = desc.substr(nl+1);
        
        // print the first line
        oss<<firstline<<std::endl;

        // align for the next line
        for (size_t j=0; j<max_width+12; j++)
          oss<<' ';

        nl = desc.find('\n');
      }

      // print the remainder
      oss<<desc<<std::endl;
    }
    return oss.str();
  }

private:

  void check(int argc, bool ok){
    if (!ok){
      if(errors.size() == 1)
        std::cerr<<"Error: "<<errors[0]<<std::endl<<std::endl<<usage();
      else if(!errors.empty())
        std::cerr<<"Errors:"<<std::endl<<error_full()<<std::endl<<usage();
      else
        std::cerr<<usage();
      exit(1);
    }

    if (argc<=1 && !ok){
      std::cerr<<usage();
      exit(0);
    }
  }

  void set_option(const std::string &name){
    if (options.count(name)==0){
      errors.push_back("undefined option: --"+name);
      return;
    }
    if (!options[name]->set()){
      errors.push_back("option needs value: --"+name);
      return;
    }
  }

  void set_option(const std::string &name, const std::string &value){
    if (options.count(name)==0){
      errors.push_back("undefined option: --"+name);
      return;
    }
    if (!options[name]->set(value)){
      std::string err_details = options[name]->error_details();
      if(err_details.empty())
        errors.push_back("option value is invalid: --"+name+"="+value);
      else
        errors.push_back("option value is invalid: --"+name+"="+value+" ("+err_details+")");
      return;
    }
  }

  class option_base{
  public:
    virtual ~option_base(){}

    virtual bool has_value() const=0;
    virtual bool set()=0;
    virtual bool set(const std::string &value)=0;
    virtual bool has_set() const=0;
    virtual bool valid() const=0;
    virtual bool must() const=0;

    virtual const std::string error_details() { return ""; }

    virtual const std::string &name() const=0;
    virtual char short_name() const=0;
    virtual const std::string &description() const=0;
    virtual std::string short_description() const=0;
  };

  class option_without_value : public option_base {
  public:
    option_without_value(const std::string &name,
                         char short_name,
                         const std::string &desc)
      :nam(name), snam(short_name), desc(desc), has(false){
    }
    ~option_without_value(){}

    bool has_value() const { return false; }

    bool set(){
      has=true;
      return true;
    }

    bool set(const std::string &){
      return false;
    }

    bool has_set() const {
      return has;
    }

    bool valid() const{
      return true;
    }

    bool must() const{
      return false;
    }

    const std::string &name() const{
      return nam;
    }

    char short_name() const{
      return snam;
    }

    const std::string &description() const {
      return desc;
    }

    std::string short_description() const{
      return "--"+nam;
    }

    virtual const std::string error_details() { return nam+" can't have parameter"; }

  private:
    std::string nam;
    char snam;
    std::string desc;
    bool has;
  };

  template <class T>
  class option_with_value : public option_base {
  public:
    option_with_value(const std::string &name,
                      char short_name,
                      bool need,
                      const T &def,
                      const std::string &desc)
      : nam(name), snam(short_name), need(need), has(false)
      , def(def), actual(def) {
      this->desc=full_description(desc);
      this->error = " (Unknown error)";
    }
    ~option_with_value(){}

    const T &get() const {
      return actual;
    }

    bool has_value() const { return true; }

    bool set(){
      return false;
    }

    bool set(const std::string &value){
      try{
        actual=read(value);
        has=true;
      }
      catch(const std::exception &e){
        error = e.what();
        return false;
      }
      return true;
    }

    virtual const std::string error_details() { return error; }

    bool has_set() const{
      return has;
    }

    bool valid() const{
      if (need && !has) return false;
      return true;
    }

    bool must() const{
      return need;
    }

    const std::string &name() const{
      return nam;
    }

    char short_name() const{
      return snam;
    }

    const std::string &description() const {
      return desc;
    }

    std::string short_description() const{
      return "--"+nam+"=<"+detail::readable_typename<T>() + ">";
    }

  protected:
    std::string full_description(const std::string &desc){
      std::string defval = detail::default_value<T>(def);

      return
        desc+" ("+
        (need?"":"optional ")+
        detail::readable_typename<T>()+
        (!need && !defval.empty() ? "="+defval : "")+
        ")";
    }

    virtual T read(const std::string &s)=0;

    std::string nam;
    char snam;
    bool need;
    std::string desc;
    std::string error;

    bool has;
    T def;
    T actual;
  };

  template <class T, class F>
  class option_with_value_with_reader : public option_with_value<T> {
  public:
    option_with_value_with_reader(const std::string &name,
                                  char short_name,
                                  bool need,
                                  const T def,
                                  const std::string &desc,
                                  F reader)
      : option_with_value<T>(name, short_name, need, def, desc), reader(reader){
        std::string reader_description = this->reader.description();

        if(!reader_description.empty())
          this->desc = this->desc + " " + reader_description;
    }

  private:
    T read(const std::string &s){
      return reader(s);
    }

    F reader;
  };

  std::map<std::string, option_base*> options;
  std::vector<option_base*> ordered;
  std::string hdr;
  std::string ftr;
  bool stop;

  std::string prog_name;
  std::vector<std::string> others;

  std::vector<std::string> errors;
};

} // cmdline
