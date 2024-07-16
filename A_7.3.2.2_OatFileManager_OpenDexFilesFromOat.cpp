//http://aospxref.com/android-8.0.0_r36/xref/art/runtime/oat_file_manager.cc?fi=OpenDexFilesFromOat#OpenDexFilesFromOat
std::vector<std::unique_ptr<const DexFile>> OatFileManager::OpenDexFilesFromOat(
                            const char* dex_location,     //dexfile路径
                            jobject class_loader,         //优化路径
                            jobjectArray dex_elements,
                            const OatFile** out_oat_file, //nullptr
                            std::vector<std::string>* error_msgs //std::vector<std::string> error_msgs;
                            ) {


  // Verify we aren't holding the mutator lock, which could starve GC if we have to generate or relocate an oat file.
  Thread* const self = Thread::Current();
  Locks::mutator_lock_->AssertNotHeld(self);
  Runtime* const runtime = Runtime::Current();
  
  //初始化一个 oat_file 助手对象
  OatFileAssistant oat_file_assistant(dex_location,
                                      kRuntimeISA,
                                      !runtime->IsAotCompiler());

  // Lock the target oat location to avoid races generating and loading the oat file.
  std::string error_msg;
  if (!oat_file_assistant.Lock(/*out*/&error_msg)) {
    // Don't worry too much if this fails. If it does fail, it's unlikely we can generate an oat file anyway.
    VLOG(class_linker) << "OatFileAssistant::Lock: " << error_msg;
  }

  const OatFile* source_oat_file = nullptr;
  
  
  //插件oat文件是否是最新的，即是否需要重新编译，如果需要的话就编译成最新的：MakeUpToDate()
  if (!oat_file_assistant.IsUpToDate()) {
    // Update the oat file on disk if we can, based on the --compiler-filter
    // option derived from the current runtime options.
    // This may fail, but that's okay. Best effort is all that matters here.
    switch (oat_file_assistant.MakeUpToDate(/*profile_changed*/false, /*out*/ &error_msg)) {
      case OatFileAssistant::kUpdateFailed:
        LOG(WARNING) << error_msg;
        break;

      case OatFileAssistant::kUpdateNotAttempted:
        // Avoid spamming the logs if we decided not to attempt making the oat file up to date.
        VLOG(oat) << error_msg;
        break;

      case OatFileAssistant::kUpdateSucceeded:
        // Nothing to do.
        break;
    }
  }

  // Get the oat file on disk.
  std::unique_ptr<const OatFile> oat_file(oat_file_assistant.GetBestOatFile().release());
  
  //当oat_file不为空，则代表dex2oat编译成功
  if (oat_file != nullptr) {
    bool accept_oat_file = !HasCollisions(oat_file.get(), class_loader, dex_elements, /*out*/ &error_msg);
    if (!accept_oat_file) {
      if (Runtime::Current()->IsDexFileFallbackEnabled()) {
        if (!oat_file_assistant.HasOriginalDexFiles()) {
          accept_oat_file = true;
          LOG(WARNING) << "Dex location " << dex_location << " does not seem to include dex file. " << "Allow oat file use. This is potentially dangerous.";
        } else {
          LOG(WARNING) << "Found duplicate classes, falling back to extracting from APK : " << dex_location;
          LOG(WARNING) << "NOTE: This wastes RAM and hurts startup performance.";
        }
      } else {
        if (!oat_file_assistant.HasOriginalDexFiles()) {
          accept_oat_file = true;
        }
        LOG(WARNING) << "Found duplicate classes, dex-file-fallback disabled, will be failing to  load classes for " << dex_location;
      }
    }

    if (accept_oat_file) {
      VLOG(class_linker) << "Registering " << oat_file->GetLocation();
      source_oat_file = RegisterOatFile(std::move(oat_file));
      *out_oat_file = source_oat_file;
    }
  }

  std::vector<std::unique_ptr<const DexFile>> dex_files;
  
  
  //情况1：从oat文件中加载dexfile信息
  // Load the dex files from the oat file.
  if (source_oat_file != nullptr) {
    bool added_image_space = false;
    
    if (source_oat_file->IsExecutable()) {
      std::unique_ptr<gc::space::ImageSpace> image_space =
          kEnableAppImage ? oat_file_assistant.OpenImageSpace(source_oat_file) : nullptr;
      if (image_space != nullptr) {
        ScopedObjectAccess soa(self);
        StackHandleScope<1> hs(self);
        Handle<mirror::ClassLoader> h_loader(
            hs.NewHandle(soa.Decode<mirror::ClassLoader>(class_loader)));
        // Can not load app image without class loader.
        if (h_loader != nullptr) {
          std::string temp_error_msg;
          // Add image space has a race condition since other threads could be reading from the
          // spaces array.
          {
            ScopedThreadSuspension sts(self, kSuspended);
            gc::ScopedGCCriticalSection gcs(self,
                                            gc::kGcCauseAddRemoveAppImageSpace,
                                            gc::kCollectorTypeAddRemoveAppImageSpace);
            ScopedSuspendAll ssa("Add image space");
            runtime->GetHeap()->AddSpace(image_space.get());
          }
          {
            ScopedTrace trace2(StringPrintf("Adding image space for location %s", dex_location)); 
            added_image_space = runtime->GetClassLinker()->AddImageSpace(image_space.get(),
                                                                         h_loader,
                                                                         dex_elements,
                                                                         dex_location,
                                                                         /*out*/&dex_files,
                                                                         /*out*/&temp_error_msg);
          }
          if (added_image_space) {
            // Successfully added image space to heap, release the map so that it does not get
            // freed.
            image_space.release();
          } else {
            LOG(INFO) << "Failed to add image file " << temp_error_msg;
            dex_files.clear();
            {
              ScopedThreadSuspension sts(self, kSuspended);
              gc::ScopedGCCriticalSection gcs(self,
                                              gc::kGcCauseAddRemoveAppImageSpace,
                                              gc::kCollectorTypeAddRemoveAppImageSpace);
              ScopedSuspendAll ssa("Remove image space");
              runtime->GetHeap()->RemoveSpace(image_space.get());
            }
            // Non-fatal, don't update error_msg.
          }
        }
      }
    }    
    //[*]goto here
    if (!added_image_space) {
      DCHECK(dex_files.empty());
      //http://aospxref.com/android-8.0.0_r36/xref/art/runtime/oat_file_assistant.cc?fi=LoadDexFiles#LoadDexFiles
      dex_files = oat_file_assistant.LoadDexFiles(*source_oat_file, dex_location);
    }
    if (dex_files.empty()) {
      error_msgs->push_back("Failed to open dex files from " + source_oat_file->GetLocation());
    }
  }

  // Fall back to running out of the original dex file if we couldn't load any dex_files from the oat file.
  //情况2：编译失败的情况！ 或者dex2oat被阻断的情况，会直接去打dexfile文件
  if (dex_files.empty()) {
    if (oat_file_assistant.HasOriginalDexFiles()) {
      if (Runtime::Current()->IsDexFileFallbackEnabled()) {
        static constexpr bool kVerifyChecksum = true;
        //[*]goto here -> dex2oat编译失败或被阻断的情况下，会直接去打dexfile文件
        //http://aospxref.com/android-8.0.0_r36/xref/art/runtime/dex_file.cc
        if (!DexFile::Open(dex_location, dex_location, kVerifyChecksum, /*out*/ &error_msg, &dex_files)) {
          LOG(WARNING) << error_msg;
          error_msgs->push_back("Failed to open dex files from " + std::string(dex_location)
                                + " because: " + error_msg);
        }
      } else {
        error_msgs->push_back("Fallback mode disabled, skipping dex files.");
      }
    } else {
      error_msgs->push_back("No original dex files found for dex location "
          + std::string(dex_location));
    }
  }

  return dex_files;
}


//以下分析aosp 8.0.0 代码!
//编译生成oat文件
//http://aospxref.com/android-8.1.0_r81/xref/art/runtime/oat_file_assistant.cc?fi=MakeUpToDate#MakeUpToDate
OatFileAssistant::ResultOfAttemptToUpdate
OatFileAssistant::MakeUpToDate(bool profile_changed,
                               ClassLoaderContext* class_loader_context,
                               std::string* error_msg) {
  CompilerFilter::Filter target;
  if (!GetRuntimeCompilerFilterOption(&target, error_msg)) {
    return kUpdateNotAttempted;
  }

  OatFileInfo& info = GetBestInfo();
  // TODO(calin, jeffhao): the context should really be passed to GetDexOptNeeded: b/62269291.
  // This is actually not trivial in the current logic as it will interact with the collision
  // check:
  //   - currently, if the context does not match but we have no collisions we still accept the
  //     oat file.
  //   - if GetDexOptNeeded would return kDex2OatFromScratch for a context mismatch and we make
  //     the oat code up to date the collision check becomes useless.
  //   - however, MakeUpToDate will not always succeed (e.g. for primary apks, or for dex files
  //     loaded in other processes). So it boils down to how far do we want to complicate
  //     the logic in order to enable the use of oat files. Maybe its time to try simplify it.
  switch (info.GetDexOptNeeded(
        target, profile_changed, /*downgrade*/ false, class_loader_context)) {
    case kNoDexOptNeeded:
      return kUpdateSucceeded;

    // TODO: For now, don't bother with all the different ways we can call
    // dex2oat to generate the oat file. Always generate the oat file as if it
    // were kDex2OatFromScratch.
    case kDex2OatFromScratch:
    case kDex2OatForBootImage:
    case kDex2OatForRelocation:
    case kDex2OatForFilter:
      return GenerateOatFileNoChecks(info, target, class_loader_context, error_msg);
  }
  UNREACHABLE();
}



//http://aospxref.com/android-8.0.0_r36/xref/art/runtime/oat_file_assistant.cc#616
OatFileAssistant::ResultOfAttemptToUpdate OatFileAssistant::GenerateOatFileNoChecks(
                      OatFileAssistant::OatFileInfo& info, 
                      CompilerFilter::Filter filter, 
                      std::string* error_msg) {
  CHECK(error_msg != nullptr);

  Runtime* runtime = Runtime::Current();
  if (!runtime->IsDex2OatEnabled()) {
    *error_msg = "Generation of oat file for dex location " + dex_location_
      + " not attempted because dex2oat is disabled.";
    return kUpdateNotAttempted;
  }

  if (info.Filename() == nullptr) {
    *error_msg = "Generation of oat file for dex location " + dex_location_
      + " not attempted because the oat file name could not be determined.";
    return kUpdateNotAttempted;
  }
  const std::string& oat_file_name = *info.Filename();
  const std::string& vdex_file_name = GetVdexFilename(oat_file_name);

  // dex2oat ignores missing dex files and doesn't report an error.
  // Check explicitly here so we can detect the error properly.
  // TODO: Why does dex2oat behave that way?
  struct stat dex_path_stat;
  if (TEMP_FAILURE_RETRY(stat(dex_location_.c_str(), &dex_path_stat)) != 0) {
    *error_msg = "Could not access dex location " + dex_location_ + ":" + strerror(errno);
    return kUpdateNotAttempted;
  }

  // If this is the odex location, we need to create the odex file layout (../oat/isa/..)
  if (!info.IsOatLocation()) {
    if (!PrepareOdexDirectories(dex_location_, oat_file_name, isa_, error_msg)) {
      return kUpdateNotAttempted;
    }
  }

  // Set the permissions for the oat and the vdex files.
  // The user always gets read and write while the group and others propagate
  // the reading access of the original dex file.
  mode_t file_mode = S_IRUSR | S_IWUSR |
      (dex_path_stat.st_mode & S_IRGRP) |
      (dex_path_stat.st_mode & S_IROTH);

  std::unique_ptr<File> vdex_file(OS::CreateEmptyFile(vdex_file_name.c_str()));
  if (vdex_file.get() == nullptr) {
    *error_msg = "Generation of oat file " + oat_file_name
      + " not attempted because the vdex file " + vdex_file_name
      + " could not be opened.";
    return kUpdateNotAttempted;
  }
  
  //判断权限
  if (fchmod(vdex_file->Fd(), file_mode) != 0) {
    *error_msg = "Generation of oat file " + oat_file_name
      + " not attempted because the vdex file " + vdex_file_name
      + " could not be made world readable.";
    return kUpdateNotAttempted;
  }

  std::unique_ptr<File> oat_file(OS::CreateEmptyFile(oat_file_name.c_str()));
  if (oat_file.get() == nullptr) {
    *error_msg = "Generation of oat file " + oat_file_name
      + " not attempted because the oat file could not be created.";
    return kUpdateNotAttempted;
  }

  if (fchmod(oat_file->Fd(), file_mode) != 0) {
    *error_msg = "Generation of oat file " + oat_file_name
      + " not attempted because the oat file could not be made world readable.";
    oat_file->Erase();
    return kUpdateNotAttempted;
  }

  std::vector<std::string> args;
  args.push_back("--dex-file=" + dex_location_);
  args.push_back("--output-vdex-fd=" + std::to_string(vdex_file->Fd()));
  args.push_back("--oat-fd=" + std::to_string(oat_file->Fd()));
  args.push_back("--oat-location=" + oat_file_name);
  args.push_back("--compiler-filter=" + CompilerFilter::NameOfFilter(filter));
    
    
  //调用dex2oat函数!
  if (!Dex2Oat(args, error_msg)) {
    // Manually delete the oat and vdex files. This ensures there is no garbage
    // left over if the process unexpectedly died.
    vdex_file->Erase();
    unlink(vdex_file_name.c_str());
    oat_file->Erase();
    unlink(oat_file_name.c_str());
    return kUpdateFailed;
  }

  if (vdex_file->FlushCloseOrErase() != 0) {
    *error_msg = "Unable to close vdex file " + vdex_file_name;
    unlink(vdex_file_name.c_str());
    return kUpdateFailed;
  }

  if (oat_file->FlushCloseOrErase() != 0) {
    *error_msg = "Unable to close oat file " + oat_file_name;
    unlink(oat_file_name.c_str());
    return kUpdateFailed;
  }

  // Mark that the odex file has changed and we should try to reload.
  info.Reset();
  return kUpdateSucceeded;
}


//http://aospxref.com/android-8.0.0_r36/xref/art/runtime/oat_file_assistant.cc#Dex2Oat
bool OatFileAssistant::Dex2Oat(const std::vector<std::string>& args,
                               std::string* error_msg) {
  Runtime* runtime = Runtime::Current();
  std::string image_location = ImageLocation();
  if (image_location.empty()) {
    *error_msg = "No image location found for Dex2Oat.";
    return false;
  }

  std::vector<std::string> argv;
  argv.push_back(runtime->GetCompilerExecutable());
  argv.push_back("--runtime-arg");
  argv.push_back("-classpath");
  argv.push_back("--runtime-arg");
  std::string class_path = runtime->GetClassPathString();
  if (class_path == "") {
    class_path = OatFile::kSpecialSharedLibrary;
  }
  argv.push_back(class_path);
  if (runtime->IsJavaDebuggable()) {
    argv.push_back("--debuggable");
  }
  runtime->AddCurrentRuntimeFeaturesAsDex2OatArguments(&argv);

  if (!runtime->IsVerificationEnabled()) {
    argv.push_back("--compiler-filter=verify-none");
  }

  if (runtime->MustRelocateIfPossible()) {
    argv.push_back("--runtime-arg");
    argv.push_back("-Xrelocate");
  } else {
    argv.push_back("--runtime-arg");
    argv.push_back("-Xnorelocate");
  }

  if (!kIsTargetBuild) {
    argv.push_back("--host");
  }

  argv.push_back("--boot-image=" + image_location);

  std::vector<std::string> compiler_options = runtime->GetCompilerOptions();
  argv.insert(argv.end(), compiler_options.begin(), compiler_options.end());

  argv.insert(argv.end(), args.begin(), args.end());

  std::string command_line(android::base::Join(argv, ' '));
  return Exec(argv, error_msg);
}



//http://aospxref.com/android-8.0.0_r36/xref/art/runtime/exec_utils.cc
bool Exec(std::vector<std::string>& arg_vector, std::string* error_msg) {
  int status = ExecAndReturnCode(arg_vector, error_msg);
  if (status != 0) {
    const std::string command_line(android::base::Join(arg_vector, ' '));
    *error_msg = StringPrintf("Failed execv(%s) because non-0 exit status",
                              command_line.c_str());
    return false;
  }
  return true;
}

}  // namespace art


//http://aospxref.com/android-8.0.0_r36/xref/art/runtime/exec_utils.cc#ExecAndReturnCode
int ExecAndReturnCode(std::vector<std::string>& arg_vector, std::string* error_msg) {
  const std::string command_line(android::base::Join(arg_vector, ' '));
  CHECK_GE(arg_vector.size(), 1U) << command_line;

  // Convert the args to char pointers.
  const char* program = arg_vector[0].c_str();
  std::vector<char*> args;
  for (size_t i = 0; i < arg_vector.size(); ++i) {
    const std::string& arg = arg_vector[i];
    char* arg_str = const_cast<char*>(arg.c_str());
    CHECK(arg_str != nullptr) << i;
    args.push_back(arg_str);
  }
  args.push_back(nullptr);
  
  //创建子进程
  // fork and exec
  pid_t pid = fork();   
  if (pid == 0) {
    // no allocation allowed between fork and exec

    // change process groups, so we don't get reaped by ProcessManager
    setpgid(0, 0);

    // (b/30160149): protect subprocesses from modifications to LD_LIBRARY_PATH, etc.
    // Use the snapshot of the environment from the time the runtime was created.
    char** envp = (Runtime::Current() == nullptr) ? nullptr : Runtime::Current()->GetEnvSnapshot();
    
    //调用dex2oat编译程序，对当前dex文件进行编译，等待编译过程结束！
    if (envp == nullptr) {
      execv(program, &args[0]);
    } else {
      execve(program, &args[0], envp);
    }
    PLOG(ERROR) << "Failed to execve(" << command_line << ")";
    // _exit to avoid atexit handlers in child.
    _exit(1);
  } 
  
  else {
    if (pid == -1) {
      *error_msg = StringPrintf("Failed to execv(%s) because fork failed: %s",
                                command_line.c_str(), strerror(errno));
      return -1;
    }

    // wait for subprocess to finish
    int status = -1;
    pid_t got_pid = TEMP_FAILURE_RETRY(waitpid(pid, &status, 0));
    if (got_pid != pid) {
      *error_msg = StringPrintf("Failed after fork for execv(%s) because waitpid failed: "
                                "wanted %d, got %d: %s",
                                command_line.c_str(), pid, got_pid, strerror(errno));
      return -1;
    }
    if (WIFEXITED(status)) {
      return WEXITSTATUS(status);
    }
    return -1;
  }
}