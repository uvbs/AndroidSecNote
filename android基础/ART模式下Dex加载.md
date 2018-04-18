## 概述
通过学习ClassLoader，可以知道无论在art模式下，还是Dalvik模式下，类加载的核心都是根据输入的Dex文件生成DexFile对象实例。DexFile对象由DexPathList.loadDexFile创建，下面就具体分析这个函数。

## DexPathList.loadDexFile
``` java
private static DexFile loadDexFile(File file, File optimizedDirectory)
            throws IOException {
        if (optimizedDirectory == null) {
            return new DexFile(file);
        } else {
            String optimizedPath = optimizedPathFor(file, optimizedDirectory);
            return DexFile.loadDex(file.getPath(), optimizedPath, 0);
        }
    }
```
直接调用了DexFile的loadDex

## DexFile.loadDex
``` java
static public DexFile loadDex(String sourcePathName, String outputPathName,
        int flags) throws IOException {
        return new DexFile(sourcePathName, outputPathName, flags);
    }
private DexFile(String sourceName, String outputName, int flags) throws IOException {
        if (outputName != null) {
            try {
                String parent = new File(outputName).getParent();
                if (Libcore.os.getuid() != Libcore.os.stat(parent).st_uid) {
                    throw new IllegalArgumentException("Optimized data directory " + parent
                            + " is not owned by the current user. Shared storage cannot protect"
                            + " your application from code injection attacks.");
                }
            } catch (ErrnoException ignored) {
                // assume we'll fail with a more contextual error later
            }
        }

        mCookie = openDexFile(sourceName, outputName, flags);
        mFileName = sourceName;
        guard.open("close");
        //System.out.println("DEX FILE cookie is " + mCookie + " sourceName=" + sourceName + " outputName=" + outputName);
    }
private static native long openDexFileNative(String sourceName, String outputName, int flags);
```
可以看到最终进入到了native层的openDexFileNative函数,该函数在native层对应的函数为DexFile_openDexFileNative

<http://androidxref.com/5.1.0_r1/xref/art/runtime/native/dalvik_system_DexFile.cc>


## DexFile_openDexFileNative
``` cpp
static jlong DexFile_openDexFileNative(JNIEnv* env, jclass, jstring javaSourceName, jstring javaOutputName, jint) {
  
  ScopedUtfChars sourceName(env, javaSourceName);
  if (sourceName.c_str() == NULL) {
    return 0;
  }
  NullableScopedUtfChars outputName(env, javaOutputName);
  if (env->ExceptionCheck()) {
    return 0;
  }

  ClassLinker* linker = Runtime::Current()->GetClassLinker();
  //保存生成的DexFile
  std::unique_ptr<std::vector<const DexFile*>> dex_files(new std::vector<const DexFile*>());
  std::vector<std::string> error_msgs;
  
  bool success = linker->OpenDexFilesFromOat(sourceName.c_str(), outputName.c_str(), &error_msgs,
                                             dex_files.get());

  if (success || !dex_files->empty()) {
    // In the case of non-success, we have not found or could not generate the oat file.
    // But we may still have found a dex file that we can use.
    return static_cast<jlong>(reinterpret_cast<uintptr_t>(dex_files.release()));
  } else {
    // The vector should be empty after a failed loading attempt.
    DCHECK_EQ(0U, dex_files->size());

    ScopedObjectAccess soa(env);
    CHECK(!error_msgs.empty());
    // The most important message is at the end. So set up nesting by going forward, which will
    // wrap the existing exception as a cause for the following one.
    auto it = error_msgs.begin();
    auto itEnd = error_msgs.end();
    for ( ; it != itEnd; ++it) {
      ThrowWrappedIOException("%s", it->c_str());
    }

    return 0;
  }
}
```
因为在art模式下，app安装时会将dex转为oat文件，只是安装了的app，必然存在oat文件。所以这里调用OpenDexFilesFromOat。

## ClassLinker::OpenDexFilesFromOat

``` cpp
bool ClassLinker::OpenDexFilesFromOat(const char* dex_location, const char* oat_location,
                                      std::vector<std::string>* error_msgs,
                                      std::vector<const DexFile*>* dex_files) {
  // 1) Check whether we have an open oat file.
  // This requires a dex checksum, use the "primary" one.
  uint32_t dex_location_checksum;
  uint32_t* dex_location_checksum_pointer = &dex_location_checksum;
  bool have_checksum = true;
  std::string checksum_error_msg;
  //获取checksum，保存在dex_location_checksum
  if (!DexFile::GetChecksum(dex_location, dex_location_checksum_pointer, &checksum_error_msg)) {
    // This happens for pre-opted files since the corresponding dex files are no longer on disk.
    dex_location_checksum_pointer = nullptr;
    have_checksum = false;
  }

  bool needs_registering = false;
  //查看该dex所对应的oat文件，是否已经打开，即内存中是否存在
  const OatFile::OatDexFile* oat_dex_file = FindOpenedOatDexFile(oat_location, dex_location,
                                                                 dex_location_checksum_pointer);
  std::unique_ptr<const OatFile> open_oat_file(
      oat_dex_file != nullptr ? oat_dex_file->GetOatFile() : nullptr);

  // 2) If we do not have an open one, maybe there's one on disk already.

  ScopedFlock scoped_flock;

  if (open_oat_file.get() == nullptr) {
    //存在oat文件
    if (oat_location != nullptr) {
      // Can only do this if we have a checksum, else error.
      if (!have_checksum) {
        error_msgs->push_back(checksum_error_msg);
        return false;
      }

      std::string error_msg;

      // We are loading or creating one in the future. Time to set up the file lock.
      if (!scoped_flock.Init(oat_location, &error_msg)) {
        error_msgs->push_back(error_msg);
        return false;
      }

      // TODO Caller specifically asks for this oat_location. We should honor it. Probably?
      //内存中不存在从磁盘中查找，并加载oat文件
      open_oat_file.reset(FindOatFileInOatLocationForDexFile(dex_location, dex_location_checksum,
                                                             oat_location, &error_msg));

      if (open_oat_file.get() == nullptr) {
        std::string compound_msg = StringPrintf("Failed to find dex file '%s' in oat location '%s': %s",
                                                dex_location, oat_location, error_msg.c_str());
        VLOG(class_linker) << compound_msg;
        error_msgs->push_back(compound_msg);
      }
    }  
    else { //不存在oat文件，从磁盘加载dex创建。
      // TODO: What to lock here?
      bool obsolete_file_cleanup_failed;
      open_oat_file.reset(FindOatFileContainingDexFileFromDexLocation(dex_location,
                                                                      dex_location_checksum_pointer,
                                                                      kRuntimeISA, error_msgs,
                                                                      &obsolete_file_cleanup_failed));
      // There's no point in going forward and eventually try to regenerate the
      // file if we couldn't remove the obsolete one. Mostly likely we will fail
      // with the same error when trying to write the new file.
      // TODO: should we maybe do this only when we get permission issues? (i.e. EACCESS).
      if (obsolete_file_cleanup_failed) {
        return false;
      }
    }
    needs_registering = true;
  }
  // 3) If we have an oat file, check all contained multidex files for our dex_location.
  // Note: LoadMultiDexFilesFromOatFile will check for nullptr in the first argument.
  bool success = LoadMultiDexFilesFromOatFile(open_oat_file.get(), dex_location,
                                              dex_location_checksum_pointer,
                                              false, error_msgs, dex_files);
  if (success) {
    const OatFile* oat_file = open_oat_file.release();  // Avoid deleting it.
    if (needs_registering) {
      // We opened the oat file, so we must register it.
      RegisterOatFile(oat_file);
    }
    // If the file isn't executable we failed patchoat but did manage to get the dex files.
    return oat_file->IsExecutable();
  } else {
    if (needs_registering) {
      // We opened it, delete it.
      open_oat_file.reset();
    } else {
      open_oat_file.release();  // Do not delete open oat files.
    }
  }

  // 4) If it's not the case (either no oat file or mismatches), regenerate and load.

  // Need a checksum, fail else.
  if (!have_checksum) {
    error_msgs->push_back(checksum_error_msg);
    return false;
  }

  // Look in cache location if no oat_location is given.
  std::string cache_location;
  if (oat_location == nullptr) {
    // Use the dalvik cache.
    const std::string dalvik_cache(GetDalvikCacheOrDie(GetInstructionSetString(kRuntimeISA)));
    cache_location = GetDalvikCacheFilenameOrDie(dex_location, dalvik_cache.c_str());
    oat_location = cache_location.c_str();
  }

  bool has_flock = true;
  // Definitely need to lock now.
  if (!scoped_flock.HasFile()) {
    std::string error_msg;
    if (!scoped_flock.Init(oat_location, &error_msg)) {
      error_msgs->push_back(error_msg);
      has_flock = false;
    }
  }

  if (Runtime::Current()->IsDex2OatEnabled() && has_flock && scoped_flock.HasFile()) {
    // Create the oat file.
    open_oat_file.reset(CreateOatFileForDexLocation(dex_location, scoped_flock.GetFile()->Fd(),
                                                    oat_location, error_msgs));
  }

  // Failed, bail.
  if (open_oat_file.get() == nullptr) {
    std::string error_msg;
    // dex2oat was disabled or crashed. Add the dex file in the list of dex_files to make progress.
    DexFile::Open(dex_location, dex_location, &error_msg, dex_files);
    error_msgs->push_back(error_msg);
    return false;
  }

  // Try to load again, but stronger checks.
  success = LoadMultiDexFilesFromOatFile(open_oat_file.get(), dex_location,
                                         dex_location_checksum_pointer,
                                         true, error_msgs, dex_files);
  if (success) {
    RegisterOatFile(open_oat_file.release());
    return true;
  } else {
    return false;
  }
}
```
流程如下：
1.  首先获取dex文件的checksum
2.  检查该dex对应的oat文件是否已经打开了
3.  如果没有oat文件没有打开
    1. 磁盘上存在oat，有的话直接打开
    2. 磁盘上不存在oat，创建oat，并打开
3. 获取到打开的oat文件后，调用loadMultiDexFilesFromOatFile,生成DexFile对象。

因为我们关心的是动态加载，直接加载dex，所以不存在oat文件, art会调用CreateOatFileForDexLocation生成oat文件。

### ClassLinker::CreateOatFileForDexLocation

``` cpp
const OatFile* ClassLinker::CreateOatFileForDexLocation(const char* dex_location,
                                                        int fd, const char* oat_location,
                                                        std::vector<std::string>* error_msgs) {
  // Generate the output oat file for the dex file
  VLOG(class_linker) << "Generating oat file " << oat_location << " for " << dex_location;
  std::string error_msg;
  if (!GenerateOatFile(dex_location, fd, oat_location, &error_msg)) {
    CHECK(!error_msg.empty());
    error_msgs->push_back(error_msg);
    return nullptr;
  }
  std::unique_ptr<OatFile> oat_file(OatFile::Open(oat_location, oat_location, nullptr, nullptr,
                                            !Runtime::Current()->IsCompiler(),
                                            &error_msg));
  if (oat_file.get() == nullptr) {
    std::string compound_msg = StringPrintf("\nFailed to open generated oat file '%s': %s",
                                            oat_location, error_msg.c_str());
    error_msgs->push_back(compound_msg);
    return nullptr;
  }

  return oat_file.release();
}

```

## ClassLinker::GenerateOatFile
``` cpp
bool ClassLinker::GenerateOatFile(const char* dex_filename,
                                  int oat_fd,
                                  const char* oat_cache_filename,
                                  std::string* error_msg) {
  Locks::mutator_lock_->AssertNotHeld(Thread::Current());  // Avoid starving GC.
  std::string dex2oat(Runtime::Current()->GetCompilerExecutable());

  gc::Heap* heap = Runtime::Current()->GetHeap();
  std::string boot_image_option("--boot-image=");
  if (heap->GetImageSpace() == nullptr) {
    // TODO If we get a dex2dex compiler working we could maybe use that, OTOH since we are likely
    // out of space anyway it might not matter.
    *error_msg = StringPrintf("Cannot create oat file for '%s' because we are running "
                              "without an image.", dex_filename);
    return false;
  }
  boot_image_option += heap->GetImageSpace()->GetImageLocation();

  std::string dex_file_option("--dex-file=");
  dex_file_option += dex_filename;

  std::string oat_fd_option("--oat-fd=");
  StringAppendF(&oat_fd_option, "%d", oat_fd);

  std::string oat_location_option("--oat-location=");
  oat_location_option += oat_cache_filename;
  //调用dex2oat生成oat
  std::vector<std::string> argv;
  argv.push_back(dex2oat);
  argv.push_back("--runtime-arg");
  argv.push_back("-classpath");
  argv.push_back("--runtime-arg");
  argv.push_back(Runtime::Current()->GetClassPathString());

  Runtime::Current()->AddCurrentRuntimeFeaturesAsDex2OatArguments(&argv);

  if (!Runtime::Current()->IsVerificationEnabled()) {
    argv.push_back("--compiler-filter=verify-none");
  }

  if (Runtime::Current()->MustRelocateIfPossible()) {
    argv.push_back("--runtime-arg");
    argv.push_back("-Xrelocate");
  } else {
    argv.push_back("--runtime-arg");
    argv.push_back("-Xnorelocate");
  }

  if (!kIsTargetBuild) {
    argv.push_back("--host");
  }

  argv.push_back(boot_image_option);
  argv.push_back(dex_file_option);
  argv.push_back(oat_fd_option);
  argv.push_back(oat_location_option);
  const std::vector<std::string>& compiler_options = Runtime::Current()->GetCompilerOptions();
  for (size_t i = 0; i < compiler_options.size(); ++i) {
    argv.push_back(compiler_options[i].c_str());
  }

  if (!Exec(argv, error_msg)) {
    // Manually delete the file. Ensures there is no garbage left over if the process unexpectedly
    // died. Ignore unlink failure, propagate the original error.
    TEMP_FAILURE_RETRY(unlink(oat_cache_filename));
    return false;
  }

  return true;
}
```
暂时就分析到dex2oat这里。