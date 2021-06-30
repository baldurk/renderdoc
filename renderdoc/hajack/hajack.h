#pragma once
#include <string>

#include "android/android_utils.h"
#include "common/globalconfig.h"
#include "jsoncpp/json/json.h"

class Hajack {
 public:
  Hajack();
  ~Hajack();
  void Init();
  static Hajack& GetInst();
  rdcstr GetRenderDoc(Android::ABI abi);
#if defined(ANDROID)
  rdcstr GetAndroidRenderDoc();
#endif
  bool IsHajack();
  bool IsInject();
  bool IsDepend();
  bool IsSelfCompiledApk();
  bool IsIgnoreTickLog();
  uint64_t GetTickLogInterval();
  rdcstr GetPackageName(Android::ABI abi);
  rdcstr GetInstallPakcages(const rdcstr& device_id);
  bool CheckInstallPakcages(const rdcstr& device_id, rdcarray<Android::ABI>& abis);
  void InitApks(const rdcstr& apks_folder, const rdcstr& suff);
  bool ModifyPakcage(Android::ABI abi, const rdcstr& org_apkdir, const rdcstr& out_dir, const rdcstr& org_package,
                     const rdcstr& new_package);
  void UnInject(const rdcstr& device_id, rdcarray<Android::ABI> abis);
  bool Injecter(const rdcstr& device_id, rdcarray<Android::ABI> abis);
  bool IsChangePackage(Android::ABI abi);
  Android::ABI GetPackageABI(const rdcstr& device_id, const rdcstr& package);
  void PushRenderGLESLayersToPckage(const rdcstr& device_id, const rdcstr& installed_path);
  bool SetPackageDepends(const rdcstr& device_id, const rdcstr& installed_path);
  rdcstr GetRenderDocConf();
  void PushRenderDocConf(const rdcstr& device_id);

 private:
  rdcstr GetPushCommand(const rdcstr& src, const rdcstr& des);
  rdcstr GetRootCommand(const rdcstr& src);
  rdcstr GetShellCommand(const rdcstr& src);
  bool PushFile(const rdcstr& device_id, const rdcstr& local, const rdcstr& fname, const rdcstr& dst);
  bool PushFileEx(const rdcstr& device_id, const rdcstr& local, const rdcstr& fname, const rdcstr& dst);
  rdcstr GetAllowDependModulePath(Android::ABI abi, const rdcstr& path);
  bool CheckerIsDepend(Android::ABI abi, const rdcstr& path);

  rdcstr remote_cfgpath_ = "/system/etc/";
  rdcstr remote_cfgname_ = "rdc.json";
  rdcstr remote_tmppath_ = "/data/local/tmp/";
  rdcstr remote_rdcconf_ = "rdc.conf";
  rdcstr android_su_fmt_ = "su -c '%s'";
  rdcstr injecter_name32_ = "injecter32";
  rdcstr injecter_name64_ = "injecter64";
  rdcstr remote_binpath32_ = "/system/bin/";
  rdcstr remote_binpath64_ = "/system/bin64/";
  rdcstr loader_soname32_ = "libloader.so";
  rdcstr loader_soname64_ = "libloader.so";
  rdcstr remote_libpath32_ = "/system/lib/";
  rdcstr remote_libpath64_ = "/system/lib64/";
  rdcstr remote_rdcname32_ = "libEGL.1.so";
  rdcstr remote_rdcname64_ = "libEGL.1.so";
  rdcstr remote_rdccmd_ = "libnaitve.so";
  rdcstr zygote_name32_ = "zygote";
  rdcstr zygote_name64_ = "zygote64";
  bool is_self_compiled_apk_ = false;
  bool ignore_tick_log_ = false;
  uint64_t tick_interval_ = 100;
  bool use_game_dir_ = true;
  std::map<Android::ABI, rdcstr> renderdoc_packages_;
  rdcarray<rdcstr> package_list_;
  rdcarray<rdcstr> depend_modules_;
  enum class Type {
    None,
    Normal,
    Inject,
    Depend,
  };
  Type type_ = Type::Normal;

  rdcstr patch_path_;
  rdcstr renderdoc_android_library = RENDERDOC_ANDROID_LIBRARY;
  rdcstr renderdoccmd_library = "librenderdoccmd.so";
  rdcstr local_config_name = "config.json";

 public:
  uint16_t FirstTargetControlPort = 38920;
  uint16_t LastTargetControlPort = FirstTargetControlPort + 7;
  uint16_t RemoteServerPort = 39920;
  uint16_t ForwardPortBase = 38950;
  uint16_t ForwardTargetControlOffset = 0;
  uint16_t ForwardRemoteServerOffset = 9;
  uint16_t ForwardPortStride = 10;
};
