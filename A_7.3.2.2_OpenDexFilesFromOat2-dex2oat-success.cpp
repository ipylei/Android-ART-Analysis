//http://aospxref.com/android-8.0.0_r36/xref/art/runtime/oat_file_assistant.cc?fi=LoadDexFiles#LoadDexFiles
std::vector<std::unique_ptr<const DexFile>> OatFileAssistant::LoadDexFiles(
                                                    const OatFile& oat_file,  //source_oat_file 即oatfile路径
                                                    const char* dex_location  //dexfile路径
                                                    ){
  std::vector<std::unique_ptr<const DexFile>> dex_files;

  // Load the main dex file.
  std::string error_msg;
  const OatFile::OatDexFile* oat_dex_file = oat_file.GetOatDexFile(dex_location, nullptr, &error_msg);
  if (oat_dex_file == nullptr) {
    LOG(WARNING) << error_msg;
    return std::vector<std::unique_ptr<const DexFile>>();
  }
  
  //【1.】第一次加载
  //OatDexFile对象调用.OpenDexFile()得到一个 DexFile 对象!
  //http://aospxref.com/android-8.0.0_r36/xref/art/runtime/oat_file.cc
  std::unique_ptr<const DexFile> dex_file = oat_dex_file->OpenDexFile(&error_msg);
  if (dex_file.get() == nullptr) {
    LOG(WARNING) << "Failed to open dex file from oat dex file: " << error_msg;
    return std::vector<std::unique_ptr<const DexFile>>();
  }
  dex_files.push_back(std::move(dex_file));
  
  //一个循环
  // Load the rest of the multidex entries
  for (size_t i = 1; ; i++) {
    std::string multidex_dex_location = DexFile::GetMultiDexLocation(i, dex_location);
    oat_dex_file = oat_file.GetOatDexFile(multidex_dex_location.c_str(), nullptr);
    if (oat_dex_file == nullptr) {
      // There are no more multidex entries to load.
      break;
    }
    
    //【2.】其他次加载
    dex_file = oat_dex_file->OpenDexFile(&error_msg);
    if (dex_file.get() == nullptr) {
      LOG(WARNING) << "Failed to open dex file from oat dex file: " << error_msg;
      return std::vector<std::unique_ptr<const DexFile>>();
    }
    dex_files.push_back(std::move(dex_file));
  }
  return dex_files;
}


//http://aospxref.com/android-8.0.0_r36/xref/art/runtime/oat_file.cc
std::unique_ptr<const DexFile> OatFile::OatDexFile::OpenDexFile(std::string* error_msg) const {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  static constexpr bool kVerify = false;
  static constexpr bool kVerifyChecksum = false;
  return DexFile::Open(dex_file_pointer_,
                       FileSize(),
                       dex_file_location_,
                       dex_file_location_checksum_,
                       this,
                       kVerify,
                       kVerifyChecksum,
                       error_msg);
}


//http://aospxref.com/android-8.1.0_r81/xref/art/runtime/dex_file.cc
std::unique_ptr<const DexFile> DexFile::Open(const uint8_t* base,
                                             size_t size,
                                             const std::string& location,
                                             uint32_t location_checksum,
                                             const OatDexFile* oat_dex_file,
                                             bool verify,
                                             bool verify_checksum,
                                             std::string* error_msg) {
  ScopedTrace trace(std::string("Open dex file from RAM ") + location);
  return OpenCommon(base,
                    size,
                    location,
                    location_checksum,
                    oat_dex_file,
                    verify,
                    verify_checksum,
                    error_msg);
                    
                    
//http://aospxref.com/android-8.1.0_r81/xref/art/runtime/dex_file.cc#OpenCommon
std::unique_ptr<DexFile> DexFile::OpenCommon(const uint8_t* base,
                                             size_t size,
                                             const std::string& location,
                                             uint32_t location_checksum,
                                             const OatDexFile* oat_dex_file,
                                             bool verify,
                                             bool verify_checksum,
                                             std::string* error_msg,
                                             VerifyResult* verify_result) {
  if (verify_result != nullptr) {
    *verify_result = VerifyResult::kVerifyNotAttempted;
  }
  std::unique_ptr<DexFile> dex_file(new DexFile(base,
                                                size,
                                                location,
                                                location_checksum,
                                                oat_dex_file));
  if (dex_file == nullptr) {
    *error_msg = StringPrintf("Failed to open dex file '%s' from memory: %s", location.c_str(),
                              error_msg->c_str());
    return nullptr;
  }
  if (!dex_file->Init(error_msg)) {
    dex_file.reset();
    return nullptr;
  }
  if (verify && !DexFileVerifier::Verify(dex_file.get(),
                                         dex_file->Begin(),
                                         dex_file->Size(),
                                         location.c_str(),
                                         verify_checksum,
                                         error_msg)) {
    if (verify_result != nullptr) {
      *verify_result = VerifyResult::kVerifyFailed;
    }
    return nullptr;
  }
  if (verify_result != nullptr) {
    *verify_result = VerifyResult::kVerifySucceeded;
  }
  return dex_file;
}                    