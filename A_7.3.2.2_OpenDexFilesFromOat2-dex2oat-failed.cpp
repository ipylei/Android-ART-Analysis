//aosp 8.1.0

//http://aospxref.com/android-8.1.0_r81/xref/art/runtime/dex_file.cc#213
bool DexFile::Open(const char* filename,
                   const std::string& location,
                   bool verify_checksum,
                   std::string* error_msg,
                   std::vector<std::unique_ptr<const DexFile>>* dex_files) {
  ScopedTrace trace(std::string("Open dex file ") + std::string(location));
  DCHECK(dex_files != nullptr) << "DexFile::Open: out-param is nullptr";
  uint32_t magic;
  
  //课时08.制作那些年顶级黑客大会上的通用脱壳机 [寒冰]
  //第一个脱壳点
  File fd = OpenAndReadMagic(filename, &magic, error_msg);
  if (fd.Fd() == -1) {
    DCHECK(!error_msg->empty());
    return false;
  }
  if (IsZipMagic(magic)) {
    return DexFile::OpenZip(fd.Release(), location, verify_checksum, error_msg, dex_files);
  }
  if (IsDexMagic(magic)) {
    std::unique_ptr<const DexFile> dex_file(DexFile::OpenFile(fd.Release(),
                                                              location,
                                                              /* verify */ true,
                                                              verify_checksum,
                                                              error_msg));
    if (dex_file.get() != nullptr) {
      dex_files->push_back(std::move(dex_file));
      return true;
    } else {
      return false;
    }
  }
  *error_msg = StringPrintf("Expected valid zip or dex file: '%s'", filename);
  return false;
}


//http://aospxref.com/android-8.1.0_r81/xref/art/runtime/dex_file.cc#273
std::unique_ptr<const DexFile> DexFile::OpenFile(int fd,
                                                 const std::string& location,
                                                 bool verify,
                                                 bool verify_checksum,
                                                 std::string* error_msg) {
  ScopedTrace trace(std::string("Open dex file ") + std::string(location));
  CHECK(!location.empty());
  std::unique_ptr<MemMap> map;
  {
    File delayed_close(fd, /* check_usage */ false);
    struct stat sbuf;
    memset(&sbuf, 0, sizeof(sbuf));
    if (fstat(fd, &sbuf) == -1) {
      *error_msg = StringPrintf("DexFile: fstat '%s' failed: %s", location.c_str(),
                                strerror(errno));
      return nullptr;
    }
    if (S_ISDIR(sbuf.st_mode)) {
      *error_msg = StringPrintf("Attempt to mmap directory '%s'", location.c_str());
      return nullptr;
    }
    size_t length = sbuf.st_size;
    //映射到内存？
    map.reset(MemMap::MapFile(length,
                              PROT_READ,
                              MAP_PRIVATE,
                              fd,
                              0,
                              /*low_4gb*/false,
                              location.c_str(),
                              error_msg));
    if (map == nullptr) {
      DCHECK(!error_msg->empty());
      return nullptr;
    }
  }

  if (map->Size() < sizeof(DexFile::Header)) {
    *error_msg = StringPrintf(
        "DexFile: failed to open dex file '%s' that is too short to have a header",
        location.c_str());
    return nullptr;
  }

  const Header* dex_header = reinterpret_cast<const Header*>(map->Begin());

  std::unique_ptr<DexFile> dex_file = OpenCommon(map->Begin(),
                                                 map->Size(),
                                                 location,
                                                 dex_header->checksum_,
                                                 kNoOatDexFile,
                                                 verify,
                                                 verify_checksum,
                                                 error_msg);
  if (dex_file != nullptr) {
    dex_file->mem_map_ = std::move(map);
  }

  return dex_file;
}


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