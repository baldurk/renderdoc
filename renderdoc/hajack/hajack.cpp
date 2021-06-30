#include "hajack.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>

#include "android/android_utils.h"
#include "aosp/android_manifest.h"
#include "common/formatting.h"
#include "common/threading.h"
#include "core/core.h"
#include "replay/replay_driver.h"
#include "strings/string_utils.h"

// RDOC;ADRD;
namespace Android {
bool CheckPatchingRequirements();
bool RealignAPK(const rdcstr& apk, rdcstr& alignedAPK, const rdcstr& tmpDir);
bool DebugSignAPK(const rdcstr& apk, const rdcstr& workDir);
}  // namespace Android

static std::shared_ptr<Hajack> hajack_inst_ = nullptr;
std::mutex hajack_mutex_;
Hajack& Hajack::GetInst() {
  if (!hajack_inst_) {
    std::lock_guard<std::mutex> guard(hajack_mutex_);
    if (!hajack_inst_) {
      hajack_inst_ = std::make_shared<Hajack>();
    }
  }
  return *(hajack_inst_.get());
}

Hajack::Hajack() {
  RDCLOG("hajack constructor");
  renderdoc_packages_[Android::ABI::armeabi_v7a] = "com.android.rdcarm32";
  renderdoc_packages_[Android::ABI::arm64_v8a] = "com.android.rdcarm64";
  renderdoc_packages_[Android::ABI::x86] = "com.android.rdcx86";
  renderdoc_packages_[Android::ABI::x86_64] = "com.android.rdcx64";
  Init();
}

Hajack::~Hajack() {
  RDCLOG("hajack deconstructor");
  renderdoc_packages_.clear();
}

char renderdoc_cfgpath[256] = "/systemdd/etcddddddddddddddddddddddddddddddddddddd/";
char renderdoc_cfgname[100] = "aaaaaaaaaaaaaaaaarenderdoc.cfg";

void Hajack::Init() {
  RDCLOG("-------- init --------");
  rdcstr libpath;
  FileIO::GetLibraryFilename(libpath);
  rdcstr libdir = get_dirname(FileIO::GetFullPathname(libpath));
#if defined(ANDROID)
  patch_path_ = renderdoc_cfgpath;
  rdcstr config_path = patch_path_ + "/" + renderdoc_cfgname;
#else
  patch_path_ = libdir + "/patch/";
  rdcstr config_path = patch_path_ + local_config_name;
#endif
  RDCLOG("patch config path:%s", patch_path_.c_str());

  std::ifstream ifs(config_path.c_str(), std::ios::binary);
  if (!ifs.is_open()) {
    RDCERR("read config %s fail!", config_path.c_str());
    return;
  }

  Json::CharReaderBuilder reader_builder;
  std::unique_ptr<Json::CharReader> json_reader(reader_builder.newCharReader());
  Json::String errs;
  Json::Value json_value;
  if (!parseFromStream(reader_builder, ifs, &json_value, &errs)) {
    RDCERR("json parse failure!%s", errs.c_str());
    return;
  }
  if (!json_value.isObject()) {
    RDCERR("json is not object");
    return;
  }
  if (json_value["type"].isString()) {
    std::string str_type = json_value["type"].asString();
    if (str_type == "none") {
      type_ = Type::None;
    } else if (str_type == "normal") {
      type_ = Type::Normal;
    } else if (str_type == "inject") {
      type_ = Type::Inject;
    } else if (str_type == "depend") {
      type_ = Type::Depend;
    } else {
      RDCWARN("unkown type %s", str_type.c_str());
    }
  }
  if (type_ != Type::None) {
    if (json_value["self_compiled_apk"].isBool()) {
      is_self_compiled_apk_ = json_value["self_compiled_apk"].asBool();
    }
  }
  if (json_value["ignore_ticklog"].isBool()) {
    ignore_tick_log_ = json_value["ignore_ticklog"].asBool();
  }
  if (json_value["tick_interval"].isInt()) {
    tick_interval_ = json_value["tick_interval"].asUInt64();
  }
  if (json_value["use_game_dir"].isBool()) {
    use_game_dir_ = json_value["use_game_dir"].asBool();
  }

  if (json_value["remote_cfgpath"].isString()) {
    remote_cfgpath_ = json_value["remote_cfgpath"].asString().c_str();
  }
  if (json_value["remote_cfgname"].isString()) {
    remote_cfgname_ = json_value["remote_cfgname"].asString().c_str();
  }
  if (json_value["remote_tmppath"].isString()) {
    remote_tmppath_ = json_value["remote_tmppath"].asString().c_str();
  }
  if (json_value["remote_rdcconf"].isString()) {
    remote_rdcconf_ = json_value["remote_rdcconf"].asString().c_str();
  }
  if (json_value["su_format"]) {
    android_su_fmt_ = json_value["su_format"].asString().c_str();
  }

  if (json_value["remote_binpath"].isString()) {
    remote_binpath32_ = json_value["remote_binpath"].asString().c_str();
    remote_binpath64_ = json_value["remote_binpath"].asString().c_str();
  }
  if (json_value["remote_binpath"].isObject()) {
    remote_binpath32_ = json_value["remote_binpath"]["32"].asString().c_str();
    remote_binpath64_ = json_value["remote_binpath"]["64"].asString().c_str();
  }

  if (json_value["injecter_name"].isString()) {
    injecter_name32_ = json_value["injecter_name"].asString().c_str();
    injecter_name64_ = json_value["injecter_name"].asString().c_str();
  }
  if (json_value["injecter_name"].isObject()) {
    injecter_name32_ = json_value["injecter_name"]["32"].asString().c_str();
    injecter_name64_ = json_value["injecter_name"]["64"].asString().c_str();
  }

  if (json_value["zygote_name"].isObject()) {
    zygote_name32_ = json_value["zygote_name"]["32"].asString().c_str();
    zygote_name64_ = json_value["zygote_name"]["64"].asString().c_str();
  }

  if (json_value["remote_libpath"].isString()) {
    remote_libpath32_ = json_value["remote_libpath"].asString().c_str();
    remote_libpath64_ = json_value["remote_libpath"].asString().c_str();
  }
  if (json_value["remote_libpath"].isObject()) {
    remote_libpath32_ = json_value["remote_libpath"]["32"].asString().c_str();
    remote_libpath64_ = json_value["remote_libpath"]["64"].asString().c_str();
  }

  if (json_value["loader_soname"].isString()) {
    loader_soname32_ = json_value["loader_soname"].asString().c_str();
    loader_soname64_ = json_value["loader_soname"].asString().c_str();
  }
  if (json_value["loader_soname"].isObject()) {
    loader_soname32_ = json_value["loader_soname"]["32"].asString().c_str();
    loader_soname64_ = json_value["loader_soname"]["64"].asString().c_str();
  }

  if (json_value["remote_rdc"].isString()) {
    remote_rdcname32_ = json_value["remote_rdc"].asString().c_str();
    remote_rdcname64_ = json_value["remote_rdc"].asString().c_str();
  }
  if (json_value["remote_rdc"].isObject()) {
    remote_rdcname32_ = json_value["remote_rdc"]["32"].asString().c_str();
    remote_rdcname64_ = json_value["remote_rdc"]["64"].asString().c_str();
  }
  if (json_value["remote_rdccmd"].isString()) {
    remote_rdccmd_ = json_value["remote_rdccmd"].asString().c_str();
  }

  if (json_value["first_target_control_port"].isInt()) {
    FirstTargetControlPort = static_cast<uint16_t>(json_value["first_target_control_port"].asInt());
  }
  if (json_value["last_target_control_port"].isInt()) {
    LastTargetControlPort = static_cast<uint16_t>(json_value["last_target_control_port"].asInt());
  } else {
    LastTargetControlPort = FirstTargetControlPort + 7;
  }
  if (json_value["forward_port_base"].isInt()) {
    ForwardPortBase = static_cast<uint16_t>(json_value["forward_port_base"].asInt());
  }
  if (json_value["remote_server_port"].isInt()) {
    RemoteServerPort = static_cast<uint16_t>(json_value["remote_server_port"].asInt());
  }
  if (json_value["forward_target_control_offset"].isInt()) {
    ForwardTargetControlOffset = static_cast<uint16_t>(json_value["forward_target_control_offset"].asInt());
  }
  if (json_value["forward_remote_server_offset"].isInt()) {
    ForwardRemoteServerOffset = static_cast<uint16_t>(json_value["forward_remote_server_offset"].asInt());
  }
  if (json_value["forward_port_stride"].isInt()) {
    ForwardPortStride = static_cast<uint16_t>(json_value["forward_port_stride"].asInt());
  }

  if (json_value["renderdoc_package"].isString()) {
    Json::Value& pakcage = json_value["renderdoc_package"];
    renderdoc_packages_[Android::ABI::armeabi_v7a] = pakcage.asString().c_str();
    renderdoc_packages_[Android::ABI::arm64_v8a] = pakcage.asString().c_str();
    renderdoc_packages_[Android::ABI::x86] = pakcage.asString().c_str();
    renderdoc_packages_[Android::ABI::x86_64] = pakcage.asString().c_str();
  }
  if (json_value["renderdoc_package"].isObject()) {
    // arm abi
    Json::Value& pakcage = json_value["renderdoc_package"];
    if (pakcage["arm32"].isString()) {
      renderdoc_packages_[Android::ABI::armeabi_v7a] = pakcage["arm32"].asString().c_str();
    }
    if (pakcage["arm64"].isString()) {
      renderdoc_packages_[Android::ABI::arm64_v8a] = pakcage["arm64"].asString().c_str();
    }
    if (pakcage["x86"].isString()) {
      renderdoc_packages_[Android::ABI::x86] = pakcage["x86"].asString().c_str();
    }
    if (pakcage["x64"].isString()) {
      renderdoc_packages_[Android::ABI::x86_64] = pakcage["x64"].asString().c_str();
    }
  }

  if (json_value["game_package"].isString()) {
    package_list_.push_back(json_value["game_package"].asString().c_str());
  }
  if (json_value["game_package"].isArray()) {
    Json::Value& pakcage = json_value["game_package"];
    for (Json::ArrayIndex i = 0; i < pakcage.size(); i++) {
      package_list_.push_back(pakcage[i].asString().c_str());
    }
  }
  if (json_value["depend_module"].isString()) {
    depend_modules_.push_back(json_value["depend_module"].asString().c_str());
  }
  if (json_value["depend_module"].isArray()) {
    Json::Value& moudle = json_value["depend_module"];
    for (Json::ArrayIndex i = 0; i < moudle.size(); i++) {
      depend_modules_.push_back(moudle[i].asString().c_str());
    }
  }
  RDCLOG("json ->");
  RDCLOG("  type:%d", static_cast<int32_t>(type_));
  RDCLOG("  self compiled apk %s", is_self_compiled_apk_ ? "true" : "false");
  RDCLOG("  ignore tick_log %s  interval:%llu", ignore_tick_log_ ? "true" : "false", tick_interval_);
  RDCLOG("  remote cfgpath:%s tmppath:%s", remote_cfgpath_.c_str(), remote_tmppath_.c_str());
  RDCLOG("  remote cfgname:%s conf:%s", remote_cfgname_.c_str(), remote_rdcconf_.c_str());
  RDCLOG("  remote su cmd fmt:%s", android_su_fmt_.c_str());
  RDCLOG("  zygote   32:%s 64:%s", zygote_name32_.c_str(), zygote_name64_.c_str());
  RDCLOG("  injecter 32:%s 64:%s", injecter_name32_.c_str(), injecter_name64_.c_str());
  RDCLOG("  loader   32:%s 64:%s", loader_soname32_.c_str(), loader_soname64_.c_str());
  RDCLOG("  renderdoc name 32:%s 64:%s", remote_rdcname32_.c_str(), remote_rdcname64_.c_str());
  RDCLOG("  renderdoc path 32:%s 64:%s", remote_libpath32_.c_str(), remote_libpath64_.c_str());
  RDCLOG("  renderdoc cmd name %s", remote_rdccmd_.c_str());
  RDCLOG("  renderdoc FirstTargetControlPort %u", FirstTargetControlPort);
  RDCLOG("  renderdoc LastTargetControlPort %u", LastTargetControlPort);
  RDCLOG("  renderdoc RemoteServerPort %u", RemoteServerPort);
  RDCLOG("  renderdoc ForwardPortBase %u", ForwardPortBase);
  RDCLOG("  renderdoc ForwardTargetControlOffset %u", ForwardTargetControlOffset);
  RDCLOG("  renderdoc ForwardRemoteServerOffset %u", ForwardRemoteServerOffset);
  RDCLOG("  renderdoc ForwardPortStride %u", ForwardPortStride);
  RDCLOG("  renderdoc packages");
  for (auto& item : renderdoc_packages_) {
    auto abi = item.first;
    RDCLOG("    abi:%s package:%s", Android::GetPlainABIName(abi).c_str(), item.second.c_str());
  }
  RDCLOG("  game packages");
  for (auto& item : package_list_) {
    RDCLOG("    package:%s", item.c_str());
  }
  RDCLOG("  depend modules");
  for (auto& item : depend_modules_) {
    RDCLOG("    module:%s", item.c_str());
  }
}

bool ModifyFileData(const rdcstr& oldpath, const rdcstr& newpath, bytebuf& orgbuf, bytebuf& newbuf, int flag = 0) {
  RDCLOG("path %s -> %s  size %zu -> %zu", oldpath.c_str(), newpath.c_str(), orgbuf.size(), newbuf.size());
  if (orgbuf.empty()) {
    RDCERR("need modify buf is empty");
    return false;
  }
  if (0 == flag) {
    if (newbuf.size() > orgbuf.size()) {
      RDCERR("at same size newbuf size > orgbuf size");
      return false;
    }
    for (std::size_t i = newbuf.size(); i < orgbuf.size(); i++) {
      newbuf.push_back(0);
    }
  }
  if (!FileIO::exists(oldpath)) {
    RDCERR("file %s not found!", oldpath.c_str());
    return false;
  }
  bytebuf file_buffer;
  if (!FileIO::ReadAll(oldpath, file_buffer)) {
    RDCERR("file %s read fail!", oldpath.c_str());
    return false;
  }
  if (orgbuf.size() >= file_buffer.size()) {
    RDCERR("%s is too small", oldpath.c_str());
    return false;
  }
  bytebuf file_buffer_new;
  std::size_t start = 0;
  for (std::size_t i = 0; i < file_buffer.size() - orgbuf.size();) {
    if (memcmp(file_buffer.data() + i, orgbuf.data(), orgbuf.size()) == 0) {
      RDCLOG("find at index 0x%08zx", i);
      if (i != start) {
        file_buffer_new.append(file_buffer.data() + start, i - start);
      }
      file_buffer_new.append(newbuf.data(), newbuf.size());
      if (flag <= 0) {
        start = i + orgbuf.size();
      } else {
        start = i + newbuf.size();
      }
      i = start;
    } else {
      i++;
    }
  }
  if (start < file_buffer.size()) {
    file_buffer_new.append(file_buffer.data() + start, file_buffer.size() - start);
  }
  if (newpath == oldpath) {
    FileIO::Delete(oldpath);
  }
  if (!FileIO::WriteAll(newpath, file_buffer_new)) {
    RDCERR("lib %s write fail!", newpath.c_str());
    return false;
  }
  return true;
}

bool ModifyFileData(const rdcstr& oldpath, const rdcstr& newpath, const rdcstr& orgstr, const rdcstr& newstr,
                    int flag = 0) {
  bytebuf orgbuf, newbuf;
  for (std::size_t i = 0; i < orgstr.length(); i++) {
    orgbuf.push_back(orgstr[i]);
  }
  for (std::size_t i = 0; i < newstr.length(); i++) {
    newbuf.push_back(newstr[i]);
  }
  return ModifyFileData(oldpath, newpath, orgbuf, newbuf, flag);
}

bool Hajack::ModifyPakcage(Android::ABI abi, const rdcstr& org_apkdir, const rdcstr& out_dir, const rdcstr& org_package,
                           const rdcstr& new_package) {
  rdcstr new_apk(out_dir + new_package + ".apk");
  std::size_t renderdoc_length = renderdoc_android_library.length();
  if ((renderdoc_length < remote_rdcname32_.length()) || (renderdoc_length < remote_rdcname64_.length())) {
    RDCERR("renderdoc so 32/64 %s/%s length not compitable", remote_rdcname32_.c_str(), remote_rdcname64_.c_str());
    return false;
  }
  rdcstr renderdoc_newsoname;
  rdcstr apklib_subdir;
  switch (abi) {
    case Android::ABI::armeabi_v7a: {
      apklib_subdir = "/armeabi-v7a/";
      renderdoc_newsoname = remote_rdcname32_.c_str();
      break;
    }
    case Android::ABI::arm64_v8a: {
      apklib_subdir = "/arm64-v8a/";
      renderdoc_newsoname = remote_rdcname64_.c_str();
      break;
    }
    case Android::ABI::x86: {
      apklib_subdir = "/x86/";
      renderdoc_newsoname = remote_rdcname32_.c_str();
      break;
    }
    case Android::ABI::x86_64: {
      apklib_subdir = "/x86_64/";
      renderdoc_newsoname = remote_rdcname64_.c_str();
      break;
    }
    default: {
      RDCERR("lib abi fail!");
      return false;
    }
  };
  RDCLOG("org_apkdir:%s out_dir:%s abi:%s", org_apkdir.c_str(), out_dir.c_str(), Android::GetPlainABIName(abi).c_str());
  RDCLOG("package %s -> %s so %s -> %s", org_package.c_str(), new_package.c_str(), renderdoc_android_library.c_str(),
         renderdoc_newsoname.c_str());
  if ((new_package == org_package) && (renderdoc_newsoname == renderdoc_android_library)) {
    RDCLOG("not modify");
    return true;
  }
  if (!Android::CheckPatchingRequirements()) {
    RDCERR("check patching requirements fail");
    return false;
  }
  rdcstr tmp_dir = FileIO::GetTempFolderFilename();
  rdcstr org_apk(org_apkdir + org_package + ".apk");

  rdcstr apktool_path(patch_path_ + "apktool.bat");
  if (!FileIO::exists(apktool_path)) {
    RDCWARN("apktool %s is not exist", apktool_path.c_str());
    apktool_path = out_dir + "apktool.bat";
  }
  if (!FileIO::exists(apktool_path)) {
    RDCWARN("apktool %s is not exist", apktool_path.c_str());
    apktool_path = Android::getToolPath(Android::ToolDir::BuildTools, "apktool.bat", false);
  }
  if (!FileIO::exists(apktool_path)) {
    RDCERR("apktool %s is not exist", apktool_path.c_str());
    return false;
  }
  apktool_path = "\"" + apktool_path + "\"";
  // rdcstr apkout_dir(FileIO::GetTempFolderFilename() + "/out/");
  rdcstr apkout_dir(out_dir + "/out/");
  FileIO::Delete(apkout_dir);
  Android::execScript("rmdir", " /s/q \"" + apkout_dir + "\"");
  auto apktool_res = Android::execScript(apktool_path, "d -f -o \"" + apkout_dir + "\" \"" + org_apk + "\"");
  if (apktool_res.strStdout.trimmed().empty()) {
    RDCERR("use apktool %s decode apk fail, err: %d %s", apktool_path.c_str(), apktool_res.retCode,
           apktool_res.strStderror.trimmed().c_str());
    return false;
  }

  // modify manifest package
  if ((!new_package.empty()) && (new_package != org_package)) {
    RDCLOG("start modify AndroidManifest package");
    rdcstr manifest_path(apkout_dir + "AndroidManifest.xml");
    rdcstr oldpackage = "package=\"" + org_package + "\"";
    rdcstr newpakcage = "package=\"" + new_package + "\"";
    if (!ModifyFileData(manifest_path, manifest_path, oldpackage, newpakcage, -1)) {
      RDCERR("modify so name fail");
      return false;
    }
    rdcstr old_androidname = "android:name=\".Loader\"";
    rdcstr new_androidname = "android:name=\"" + org_package + ".Loader\"";
    if (!ModifyFileData(manifest_path, manifest_path, old_androidname, new_androidname, -1)) {
      RDCERR("modify so name fail");
      return false;
    }
  }

  // modif so name
  if ((!renderdoc_newsoname.empty()) && (renderdoc_newsoname != renderdoc_android_library)) {
    RDCLOG("start modify so name %s -> %s", renderdoc_android_library.c_str(), renderdoc_newsoname.c_str());
    rdcstr renderdoc_savepath(patch_path_ + renderdoc_android_library + "_" + Android::GetPlainABIName(abi));
    rdcstr renderdoc_path(apkout_dir + "lib/" + apklib_subdir + renderdoc_android_library);
    rdcstr renderdoc_newpath(apkout_dir + "lib/" + apklib_subdir + renderdoc_newsoname);
    // oldpath, rdcstr& newpath, bytebuf& orgbuf, bytebuf& newbuf
    if (!ModifyFileData(renderdoc_path, renderdoc_newpath, renderdoc_android_library, renderdoc_newsoname)) {
      RDCERR("modify so name fail");
      return false;
    }
    if (IsSelfCompiledApk()) {
      RDCLOG("change renderdoc cfg path");
      if (!ModifyFileData(renderdoc_newpath, renderdoc_newpath, renderdoc_cfgpath, remote_cfgpath_, 1)) {
        RDCERR("modify so config path fail");
        return false;
      }
      if (!ModifyFileData(renderdoc_newpath, renderdoc_newpath, renderdoc_cfgname, remote_cfgname_, 1)) {
        RDCERR("modify so config path fail");
        return false;
      }
    }
    if (!FileIO::Copy(renderdoc_newpath, renderdoc_savepath, true)) {
      RDCERR("copy file %s fail!", renderdoc_savepath.c_str());
      return false;
    }

    // repaire librendercmd.so
    rdcstr renderdoccmd_path(apkout_dir + "lib/" + apklib_subdir + renderdoccmd_library);
    if (!ModifyFileData(renderdoccmd_path, renderdoccmd_path, renderdoc_android_library, renderdoc_newsoname)) {
      RDCERR("modify so name fail");
      return false;
    }
  }
  if ((!remote_rdccmd_.empty()) && (remote_rdccmd_ != renderdoccmd_library)) {
    rdcstr renderdoccmd_path(apkout_dir + "lib/" + apklib_subdir + renderdoccmd_library);
    rdcstr renderdoccmd_newpath(apkout_dir + "lib/" + apklib_subdir + remote_rdccmd_);
    if (!ModifyFileData(renderdoccmd_path, renderdoccmd_newpath, renderdoccmd_library, remote_rdccmd_)) {
      RDCERR("modify so name fail");
      return false;
    }

    rdcstr loader_smail(apkout_dir + "smali/org/renderdoc/renderdoccmd/" + Android::GetPlainABIName(abi) +
                        "/Loader.smali");
    rdcstr remote_rdccmd_tmp = remote_rdccmd_;
    remote_rdccmd_tmp[remote_rdccmd_tmp.length() - 3] = 0;
    rdcstr remote_rdccmd_loadname = remote_rdccmd_tmp.c_str() + 3;
    RDCLOG("renderdoc cmdso name:%s", remote_rdccmd_loadname.c_str());
    if (!ModifyFileData(loader_smail, loader_smail, "\"renderdoccmd\"", "\"" + remote_rdccmd_loadname + "\"", -1)) {
      RDCERR("modify so name fail");
      return false;
    }

    rdcstr manifest_path(apkout_dir + "AndroidManifest.xml");
    rdcstr oldlibname = "android:value=\"renderdoccmd\"";
    rdcstr newlibname = "android:value=\"" + remote_rdccmd_loadname + "\"";
    if (!ModifyFileData(manifest_path, manifest_path, oldlibname, newlibname, -1)) {
      RDCERR("modify so name fail");
      return false;
    }
  }

  // rebuild apk
  rdcstr org_tmp_apk(out_dir + org_package + ".temp.apk");
  FileIO::Delete(org_tmp_apk);
  apktool_res = Android::execScript(apktool_path, "b \"" + apkout_dir + "\" -o \"" + org_tmp_apk + "\"");
  if (apktool_res.strStdout.trimmed().empty()) {
    RDCERR("use apktool %s build apk fail err:%s", apktool_path.c_str(), apktool_res.strStderror.trimmed().c_str());
    return false;
  }

  // realign apk
  rdcstr aligned_apk(out_dir + org_package + ".aligned.apk");
  RDCLOG("---- realign apk");
  if (!Android::RealignAPK(org_tmp_apk, aligned_apk, tmp_dir)) {
    RDCLOG("realign apk fail");
    return false;
  }

  RDCLOG("---- debug sign apk");
  if (!Android::DebugSignAPK(aligned_apk, tmp_dir)) {
    RDCLOG("debug sign apk fail");
    return false;
  }
  FileIO::Delete(new_apk);
  RDCLOG("---- rename apk");
  if (0 != std::rename(aligned_apk.c_str(), new_apk.c_str())) {
    RDCLOG("rename apk fail");
    return false;
  }
  FileIO::Delete(org_tmp_apk);
  FileIO::Delete(apkout_dir);
  Android::execScript("rmdir", " /s/q \"" + apkout_dir + "\"");
  RDCLOG("---- succ");
  return true;
}

bool Hajack::IsChangePackage(Android::ABI abi) { return renderdoc_packages_.find(abi) != renderdoc_packages_.end(); }

Android::ABI Hajack::GetPackageABI(const rdcstr& device_id, const rdcstr& package) {
  Android::ABI abi = Android::ABI::unknown;
  rdcstr installedABI = Android::DetermineInstalledABI(device_id, package);
  if (installedABI == "null" || installedABI.empty()) {
    RDCLOG("Can't determine installed ABI, falling back to device preferred ABI");
    // pick the last ABI
    rdcarray<Android::ABI> abis = Android::GetSupportedABIs(device_id);
    if (abis.empty())
      RDCWARN("No ABIs listed as supported");
    else
      abi = abis.back();
  } else {
    abi = Android::GetABI(installedABI);
  }
  return abi;
}

void Hajack::PushRenderGLESLayersToPckage(const rdcstr& device_id, const rdcstr& installed_path) {
  rdcstr res = Android::adbExecCommand(device_id, "shell ls " + installed_path + "/lib/").strStdout.trimmed();
  rdcarray<rdcstr> libpaths;
  split(res, libpaths, '\n');
  for (rdcstr& libpath : libpaths) {
    libpath.trim();
    RDCLOG("lib path %s", libpath.c_str());
    Android::ABI abi = Android::ABI::unknown;
    if (libpath.endsWith("arm")) {
      abi = Android::ABI::armeabi_v7a;
    }
    if (libpath.endsWith("arm64")) {
      abi = Android::ABI::arm64_v8a;
    }
    if (Android::ABI::unknown == abi) {
      RDCERR("unkown abi");
      continue;
    }

    rdcstr render_gles_layers = GetRenderDoc(abi);
    // rdcstr cmd = GetPushCommand(patch_path_ + "/" + renderdoc_android_library + "_" + Android::GetPlainABIName(abi),
    //                             installed_path + "/lib/" + libpath + "/" + render_gles_layers);
    if (!PushFile(device_id, patch_path_, renderdoc_android_library + "_" + Android::GetPlainABIName(abi),
                  installed_path + "/lib/" + libpath + "/" + render_gles_layers)) {
      RDCERR("push file fail");
      return;
    }
  }
}

bool Hajack::CheckerIsDepend(Android::ABI abi, const rdcstr& path) {
  rdcarray<PathEntry> subfiles;
  FileIO::GetFilesInDirectory(path, subfiles);
  for (PathEntry& file : subfiles) {
    rdcstr file_path = path + "/" + file.filename;
    if (!FileIO::exists(file_path)) {
      RDCWARN("file %s is not exists", file_path.c_str());
      continue;
    }
    rdcstr tool = patch_path_ + "/depends.exe";
    rdcstr so = GetRenderDoc(abi);
    auto args = StringFormat::Fmt("check \"%s\" \"%s\"", file_path.c_str(), so.c_str());
    auto res = Android::execCommand(tool, args);
    if (res.retCode == 0) {
      RDCLOG("patch %s has depends!", file_path.c_str());
      return true;
    }
  }
  return false;
}
rdcstr Hajack::GetAllowDependModulePath(Android::ABI abi, const rdcstr& path) {
  rdcarray<PathEntry> subfiles;
  FileIO::GetFilesInDirectory(path, subfiles);
  for (PathEntry& file : subfiles) {
    if ((depend_modules_.size() != 0) && (depend_modules_.indexOf(file.filename) == -1)) {
      RDCLOG("file %s is not in depend modules", file.filename.c_str());
      continue;
    }
    rdcstr file_path = path + "/" + file.filename;
    if (!FileIO::exists(file_path)) {
      RDCWARN("file %s is not exists", file_path.c_str());
      continue;
    }
    RDCLOG("%s", file_path.c_str());
    rdcstr tool = patch_path_ + "/depends.exe";
    rdcstr so = GetRenderDoc(abi);
    auto args = StringFormat::Fmt("add \"%s\" \"%s\"", file_path.c_str(), so.c_str());
    auto res = Android::execCommand(tool, args);
    RDCLOG(res.strStdout.trimmed().c_str());
    if (res.retCode == 0) {
      RDCLOG("patch %s succ!", file_path.c_str());
      return file.filename;
    }
    RDCERR("patch %s fail", file_path.c_str());
  }
  return "";
}

bool Hajack::SetPackageDepends(const rdcstr& device_id, const rdcstr& installed_path) {
  rdcstr dst = patch_path_ + "/tmp";
  rdcstr cmd = "pull \"" + installed_path + "/lib/\" \"" + dst + "\"";
  auto func = [&]() {
    auto res = Android::adbExecCommand(device_id, cmd);
    if (res.retCode != 0) {
      RDCERR("pull %s libs fail!%s", installed_path.c_str(), res.strStderror.trimmed().c_str());
      return false;
    }
    RDCLOG("pull %s libs succ!%s", installed_path.c_str(), res.strStdout.trimmed().c_str());

    rdcarray<PathEntry> libpaths;
    FileIO::GetFilesInDirectory(dst, libpaths);
    for (PathEntry& libpath : libpaths) {
      Android::ABI abi = Android::ABI::unknown;
      if (libpath.filename == "arm") {
        abi = Android::ABI::armeabi_v7a;
      }
      if (libpath.filename == "arm64") {
        abi = Android::ABI::arm64_v8a;
      }
      if (Android::ABI::unknown == abi) {
        RDCERR("unkown abi");
        return false;
      }
      rdcstr local = dst + "/" + libpath.filename;
      if (CheckerIsDepend(abi, local)) {
        RDCERR("%s has depends", local.c_str());
        continue;
      }
      auto depend_module = GetAllowDependModulePath(abi, local);
      if (depend_module.empty()) {
        RDCERR("Get Depend Module fail!%s", local.c_str());
        return false;
      }
      rdcstr remote = installed_path + "/lib/" + libpath.filename;
      if (!PushFile(device_id, local, depend_module, remote)) {
        RDCERR("push file fail!local:%s file:%s remote:%s", local.c_str(), depend_module.c_str(), remote.c_str());
        return false;
      }
      rdcstr remote_rdc_path;
      if (!use_game_dir_) {
        if ((abi == Android::ABI::x86) || (abi == Android::ABI::armeabi_v7a)) {
          remote_rdc_path = remote_libpath32_;
        }
        if ((abi == Android::ABI::x86_64) || (abi == Android::ABI::arm64_v8a)) {
          remote_rdc_path = remote_libpath64_;
        }
      }
      if (remote_rdc_path.empty()) {
        remote_rdc_path = remote;
      }
      if (!PushFile(device_id, patch_path_, renderdoc_android_library + "_" + Android::GetPlainABIName(abi),
                    remote_rdc_path + "/" + GetRenderDoc(abi))) {
        RDCERR("push file fail!local:%s file:%s remote:%s", local.c_str(), GetRenderDoc(abi).c_str(),
               remote_rdc_path.c_str());
        return false;
      }
    }
    RDCLOG("set depends succ!");
    return true;
  };
  auto res = func();
  Android::execScript("rmdir", " /s/q \"" + dst + "\"");
  return res;
}

void Hajack::InitApks(const rdcstr& apks_folder, const rdcstr& suff) {
  RDCLOG("apks folder %s", apks_folder.c_str());
  for (auto& item : renderdoc_packages_) {
    auto abi = item.first;
    rdcstr apk_path = apks_folder;

    int abi_suffix = apk_path.find(suff);
    if (abi_suffix >= 0) {
      apk_path.replace(abi_suffix, suff.size(), Android::GetPlainABIName(abi));
    }
    rdcstr org_apk = apk_path + RENDERDOC_ANDROID_PACKAGE_BASE "." + Android::GetPlainABIName(abi) + ".apk";
    if (!FileIO::exists(org_apk)) {
      RDCWARN("%s missing - ensure you build all ABIs your device can support for full compatibility", org_apk.c_str());
      continue;
    }
    rdcstr org_apk_path = apk_path;
    if (IsSelfCompiledApk()) {
      RDCLOG("use self compiled apk");
      org_apk_path = patch_path_;
    }
    rdcstr org_package = RENDERDOC_ANDROID_PACKAGE_BASE "." + Android::GetPlainABIName(abi);
    if (!ModifyPakcage(abi, org_apk_path, apk_path, org_package, item.second)) {
      RDCERR("%s modify to %s fail", org_apk.c_str(), item.second.c_str());
    }
  }
}

rdcstr Hajack::GetRenderDoc(Android::ABI abi) {
  if ((abi == Android::ABI::x86) || (abi == Android::ABI::armeabi_v7a)) {
    return remote_rdcname32_.c_str();
  }
  if ((abi == Android::ABI::x86_64) || (abi == Android::ABI::arm64_v8a)) {
    return remote_rdcname64_.c_str();
  }
  RDCERR("Can't find abi %s layers", Android::GetPlainABIName(abi).c_str());
  return "";
}

#if defined(ANDROID)
rdcstr Hajack::GetAndroidRenderDoc() {
  if (sizeof(void*) == sizeof(uint32_t)) {
    return remote_rdcname32_.c_str();
  }
  return remote_rdcname64_.c_str();
}
#endif

bool Hajack::IsHajack() {
  bool res = type_ != Type::None;
  RDCLOG("check hajack %s", res ? "true" : "false");
  return res;
}

bool Hajack::IsInject() {
  auto res = type_ == Type::Inject;
  RDCLOG("check inject %s", res ? "true" : "false");
  return res;
}

bool Hajack::IsDepend() {
  auto res = type_ == Type::Depend;
  RDCLOG("check depend %s", res ? "true" : "false");
  return res;
}

bool Hajack::IsSelfCompiledApk() {
  if (type_ == Type::None) {
    return false;
  }
  return is_self_compiled_apk_;
}

bool Hajack::IsIgnoreTickLog() { return ignore_tick_log_; }

uint64_t Hajack::GetTickLogInterval() { return tick_interval_; }

rdcstr Hajack::GetPackageName(Android::ABI abi) {
  auto iter = renderdoc_packages_.find(abi);
  if (iter == renderdoc_packages_.end()) {
    RDCERR("Can't find abi %s for package", Android::GetPlainABIName(abi).c_str());
    return "";
  }
  return iter->second;
}

bool Hajack::CheckInstallPakcages(const rdcstr& device_id, rdcarray<Android::ABI>& abis) {
  RDCLOG("check install packages");
  for (Android::ABI abi : abis) {
    rdcstr command = "shell pm list packages " + GetPackageName(abi);
    RDCLOG("  -- %s", command.c_str());
    Process::ProcessResult adbCheck = Android::adbExecCommand(device_id, command);
    if (adbCheck.strStdout.empty()) {
      RDCERR("Couldn't find installed APK %s. stderr: %s", command.c_str(), adbCheck.strStderror.c_str());
      return false;
    }
  }
  RDCLOG("success");
  return true;
}

rdcstr Hajack::GetInstallPakcages(const rdcstr& device_id) {
  RDCLOG("get install packages");
  rdcarray<rdcstr> packages;
  rdcarray<Android::ABI> abis = Android::GetSupportedABIs(device_id);
  for (Android::ABI abi : abis) {
    rdcstr command = "shell pm list packages " + GetPackageName(abi);
    RDCLOG("  -- %s", command.c_str());
    Process::ProcessResult adbCheck = Android::adbExecCommand(device_id, command);
    if (adbCheck.strStdout.empty()) {
      RDCERR("Couldn't find installed APK %s. stderr: %s", command.c_str(), adbCheck.strStderror.c_str());
      continue;
    }
    packages.push_back(adbCheck.strStdout.trimmed());
  }
  rdcstr res;
  merge(packages, res, '\n');
  RDCLOG("installed APKs %s.", res.c_str());
  return res;
}

rdcstr Hajack::GetPushCommand(const rdcstr& src, const rdcstr& dst) {
  if (!FileIO::exists(src)) {
    RDCWARN("push src:%s is not exist", src.c_str());
  }
  return "push \"" + src + "\" \"" + dst + "\"";
}

rdcstr Hajack::GetRootCommand(const rdcstr& src) {
  if (android_su_fmt_.empty()) {
    return src;
  }
  rdcstr src_tmp = src.trimmed();
  if (src_tmp.beginsWith("shell")) {
    rdcstr src2 = src_tmp.c_str() + 5;
    return "shell " + StringFormat::Fmt(android_su_fmt_.c_str(), src2.c_str());
  }
  RDCLOG("cmd:%s is not shell cmd", src.c_str());
  return src;
}

rdcstr Hajack::GetShellCommand(const rdcstr& src) {
  if (src.trimmed().beginsWith("shell")) {
    return GetRootCommand(src);
  }
  return GetRootCommand("shell " + src);
}

bool Hajack::PushFile(const rdcstr& device_id, const rdcstr& local, const rdcstr& fname, const rdcstr& dst) {
  return PushFileEx(device_id, local + "/" + fname, fname, dst);
}

bool Hajack::PushFileEx(const rdcstr& device_id, const rdcstr& local, const rdcstr& fname, const rdcstr& dst) {
  rdcstr cmd = GetPushCommand(local, remote_tmppath_ + "/" + fname);
  auto res = Android::adbExecCommand(device_id, cmd);
  if (res.retCode != 0) {
    RDCERR("push fail!%s", res.strStderror.trimmed().c_str());
    return false;
  }
  RDCLOG(res.strStdout.trimmed().c_str());

  cmd = GetShellCommand("cp \"" + remote_tmppath_ + "/" + fname + "\" \"" + dst + "\"");
  res = Android::adbExecCommand(device_id, cmd);
  if (res.retCode != 0) {
    RDCERR("copy fail!%s", res.strStderror.trimmed().c_str());
    return false;
  }
  RDCLOG(res.strStdout.trimmed().c_str());

  cmd = GetShellCommand("rm -rf \"" + remote_tmppath_ + "/" + fname + "\"");
  res = Android::adbExecCommand(device_id, cmd);
  if (res.retCode != 0) {
    RDCERR("rm tmp fail fail!%s", res.strStderror.trimmed().c_str());
    return false;
  }
  RDCLOG("push file success!%s", res.strStdout.trimmed().c_str());
  return true;
}

rdcstr Hajack::GetRenderDocConf() {
  rdcstr conf = remote_cfgpath_ + "/" + remote_rdcconf_;
  RDCLOG("conf:%s", conf.c_str());
  return conf;
}
void Hajack::PushRenderDocConf(const rdcstr& device_id) {
  rdcstr conf_name = "renderdoc.conf";
  rdcstr conf_path = FileIO::GetAppFolderFilename(conf_name);
  rdcstr dst_conf = remote_cfgpath_ + "/" + remote_rdcconf_;
  if (!PushFileEx(device_id, conf_path, conf_name, dst_conf)) {
    RDCERR("push conf fail");
  }
}

void Hajack::UnInject(const rdcstr& device_id, rdcarray<Android::ABI> abis) {
  // uninject
  if (type_ != Type::Inject) {
    RDCWARN("cur type is not inject");
    return;
  }
  RDCLOG("-------- uninject --------");
  for (Android::ABI abi : abis) {
    rdcstr uninjecter_cmd;
    if ((Android::ABI::x86 == abi) || (Android::ABI::armeabi_v7a == abi)) {
      uninjecter_cmd = remote_binpath32_ + "/" + injecter_name32_ + " uninject \"" + zygote_name32_ + "\" \"" +
                       remote_libpath32_ + "/" + loader_soname32_ + "\"";
    }
    if ((Android::ABI::x86_64 == abi) || (Android::ABI::arm64_v8a == abi)) {
      uninjecter_cmd = remote_binpath64_ + "/" + injecter_name64_ + " uninject \"" + zygote_name64_ + "\" \"" +
                       remote_libpath64_ + "/" + loader_soname64_ + "\"";
    }
    if (!uninjecter_cmd.empty()) {
      auto res = Android::adbExecCommand(device_id, GetShellCommand(uninjecter_cmd));
      if (res.retCode != 0) {
        RDCWARN("uninject fail!%s %s", res.strStderror.trimmed().c_str(), res.strStdout.trimmed().c_str());
      } else {
        RDCLOG("uninject succ!%s", res.strStdout.trimmed().c_str());
      }
    }
  }
}

bool Hajack::Injecter(const rdcstr& device_id, rdcarray<Android::ABI> abis) {
  RDCLOG("start inject");
  Process::ProcessResult res =
      Android::adbExecCommand(device_id, GetShellCommand("mkdir -p \"" + remote_cfgpath_ + "\""));
  if (res.retCode != 0) {
    RDCERR("mkdir %s fail!%s", remote_cfgpath_.c_str(), res.strStderror.trimmed().c_str());
    return false;
  }
  if (!PushFile(device_id, patch_path_, local_config_name, remote_cfgpath_ + "/" + remote_cfgname_)) {
    RDCERR("push config file fail");
    return false;
  }
  for (Android::ABI abi : abis) {
    // modify loader config path
    RDCLOG("--- abi:%s", Android::GetPlainABIName(abi).c_str());
    rdcstr loader = patch_path_ + "/libloader.so_" + Android::GetPlainABIName(abi);
    rdcstr loader_newname = "/libloader.so_" + Android::GetPlainABIName(abi) + "_" + "tmp";
    if (!ModifyFileData(loader, patch_path_ + loader_newname, "/system/etc/renderdoc.cfg",
                        remote_cfgpath_ + "/" + remote_cfgname_, 1)) {
      RDCERR("loader %s modify fail", loader.c_str());
      return false;
    }
    rdcstr injecter_cmd;
    if ((Android::ABI::x86 == abi) || (Android::ABI::armeabi_v7a == abi)) {
      if (!PushFile(device_id, patch_path_, loader_newname, remote_libpath32_ + "/" + loader_soname32_)) {
        RDCERR("push loader fail");
        return false;
      }
      if (!PushFile(device_id, patch_path_, renderdoc_android_library + "_" + Android::GetPlainABIName(abi),
                    remote_libpath32_ + "/" + remote_rdcname32_)) {
        RDCERR("push renderdoc so fail");
        return false;
      }
      if (!PushFile(device_id, patch_path_, "/injecter_" + Android::GetPlainABIName(abi),
                    remote_binpath32_ + "/" + injecter_name32_)) {
        RDCERR("push injecter fail");
        return false;
      }
      res = Android::adbExecCommand(
          device_id, GetShellCommand("chmod 755 \"" + remote_binpath32_ + "/" + injecter_name32_ + "\""));
      if (res.retCode != 0) {
        RDCERR("chmod %s fail!%s", injecter_name32_.c_str(), res.strStderror.trimmed().c_str());
        return false;
      }
      injecter_cmd = remote_binpath32_ + "/" + injecter_name32_ + " inject \"" + zygote_name32_ + "\" \"" +
                     remote_libpath32_ + "/" + loader_soname32_ + "\"";
    }
    if ((Android::ABI::x86_64 == abi) || (Android::ABI::arm64_v8a == abi)) {
      if (!PushFile(device_id, patch_path_, loader_newname, remote_libpath64_ + "/" + loader_soname64_)) {
        RDCERR("push loader fail");
        return false;
      }
      if (!PushFile(device_id, patch_path_, renderdoc_android_library + "_" + Android::GetPlainABIName(abi),
                    remote_libpath64_ + "/" + remote_rdcname64_)) {
        RDCERR("push renderdoc so fail");
        return false;
      }
      if (!PushFile(device_id, patch_path_, "/injecter_" + Android::GetPlainABIName(abi),
                    remote_binpath64_ + "/" + injecter_name64_)) {
        RDCERR("push injecter fail");
        return false;
      }
      res = Android::adbExecCommand(
          device_id, GetShellCommand("chmod 755 \"" + remote_binpath64_ + "/" + injecter_name64_ + "\""));
      if (res.retCode != 0) {
        RDCERR("chmod %s fail!%s", injecter_name64_.c_str(), res.strStderror.trimmed().c_str());
        return false;
      }
      injecter_cmd = remote_binpath64_ + "/" + injecter_name64_ + " inject \"" + zygote_name64_ + "\" \"" +
                     remote_libpath64_ + "/" + loader_soname64_ + "\"";
    }
    if (!injecter_cmd.empty()) {
      res = Android::adbExecCommand(device_id, GetShellCommand(injecter_cmd));
      if (res.retCode != 0) {
        RDCERR("inject fail!%s %s", res.strStderror.trimmed().c_str(), res.strStdout.trimmed().c_str());
        return false;
      } else {
        RDCLOG("inject succ!%s", res.strStdout.trimmed().c_str());
      }
    }
  };
  RDCLOG("inject success");
  return true;
}

extern "C" RENDERDOC_API bool RENDERDOC_CC ExChangePackageName(Android::ABI abi, const std::string& dir,
                                                               const std::string& org_package,
                                                               const std::string& new_package) {
  return Hajack::GetInst().ModifyPakcage(abi, dir.c_str(), dir.c_str(), org_package.c_str(), new_package.c_str());
}
