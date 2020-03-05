/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <android-base/strings.h>
#include <android-base/unique_fd.h>
#include <android/content/pm/DataLoaderParamsParcel.h>
#include <android/content/pm/FileSystemControlParcel.h>
#include <android/content/pm/IDataLoaderStatusListener.h>
#include <android/os/IVold.h>
#include <android/os/incremental/IIncrementalManager.h>
#include <binder/IServiceManager.h>
#include <incfs.h>

#include <memory>
#include <string>
#include <string_view>

using namespace android::incfs;
using namespace android::content::pm;

namespace android::os::incremental {

// --- Wrapper interfaces ---

using MountId = int32_t;

class VoldServiceWrapper {
public:
    virtual ~VoldServiceWrapper() = default;
    virtual binder::Status mountIncFs(const std::string& backingPath, const std::string& targetDir,
                                      int32_t flags,
                                      IncrementalFileSystemControlParcel* _aidl_return) const = 0;
    virtual binder::Status unmountIncFs(const std::string& dir) const = 0;
    virtual binder::Status bindMount(const std::string& sourceDir,
                                     const std::string& targetDir) const = 0;
};

class IncrementalManagerWrapper {
public:
    virtual ~IncrementalManagerWrapper() = default;
    virtual binder::Status prepareDataLoader(MountId mountId,
                                             const FileSystemControlParcel& control,
                                             const DataLoaderParamsParcel& params,
                                             const sp<IDataLoaderStatusListener>& listener,
                                             bool* _aidl_return) const = 0;
    virtual binder::Status startDataLoader(MountId mountId, bool* _aidl_return) const = 0;
    virtual binder::Status destroyDataLoader(MountId mountId) const = 0;
    virtual binder::Status showHealthBlockedUI(MountId mountId) const = 0;
};

class IncFsWrapper {
public:
    virtual ~IncFsWrapper() = default;
    virtual ErrorCode makeFile(Control control, std::string_view path, int mode, FileId id,
                               NewFileParams params) const = 0;
    virtual ErrorCode makeDir(Control control, std::string_view path, int mode) const = 0;
    virtual RawMetadata getMetadata(Control control, FileId fileid) const = 0;
    virtual RawMetadata getMetadata(Control control, std::string_view path) const = 0;
    virtual FileId getFileId(Control control, std::string_view path) const = 0;
    virtual ErrorCode link(Control control, std::string_view from, std::string_view to) const = 0;
    virtual ErrorCode unlink(Control control, std::string_view path) const = 0;
    virtual base::unique_fd openWrite(Control control, FileId id) const = 0;
    virtual ErrorCode writeBlocks(Span<const DataBlock> blocks) const = 0;
};

class ServiceManagerWrapper {
public:
    virtual ~ServiceManagerWrapper() = default;
    virtual std::unique_ptr<VoldServiceWrapper> getVoldService() = 0;
    virtual std::unique_ptr<IncrementalManagerWrapper> getIncrementalManager() = 0;
    virtual std::unique_ptr<IncFsWrapper> getIncFs() = 0;
};

// --- Real stuff ---

class RealVoldService : public VoldServiceWrapper {
public:
    RealVoldService(const sp<os::IVold> vold) : mInterface(std::move(vold)) {}
    ~RealVoldService() = default;
    binder::Status mountIncFs(const std::string& backingPath, const std::string& targetDir,
                              int32_t flags,
                              IncrementalFileSystemControlParcel* _aidl_return) const override {
        return mInterface->mountIncFs(backingPath, targetDir, flags, _aidl_return);
    }
    binder::Status unmountIncFs(const std::string& dir) const override {
        return mInterface->unmountIncFs(dir);
    }
    binder::Status bindMount(const std::string& sourceDir,
                             const std::string& targetDir) const override {
        return mInterface->bindMount(sourceDir, targetDir);
    }

private:
    sp<os::IVold> mInterface;
};

class RealIncrementalManager : public IncrementalManagerWrapper {
public:
    RealIncrementalManager(const sp<os::incremental::IIncrementalManager> manager)
          : mInterface(manager) {}
    ~RealIncrementalManager() = default;
    binder::Status prepareDataLoader(MountId mountId, const FileSystemControlParcel& control,
                                     const DataLoaderParamsParcel& params,
                                     const sp<IDataLoaderStatusListener>& listener,
                                     bool* _aidl_return) const override {
        return mInterface->prepareDataLoader(mountId, control, params, listener, _aidl_return);
    }
    binder::Status startDataLoader(MountId mountId, bool* _aidl_return) const override {
        return mInterface->startDataLoader(mountId, _aidl_return);
    }
    binder::Status destroyDataLoader(MountId mountId) const override {
        return mInterface->destroyDataLoader(mountId);
    }
    binder::Status showHealthBlockedUI(MountId mountId) const override {
        return mInterface->showHealthBlockedUI(mountId);
    }

private:
    sp<os::incremental::IIncrementalManager> mInterface;
};

class RealServiceManager : public ServiceManagerWrapper {
public:
    RealServiceManager(sp<IServiceManager> serviceManager);
    ~RealServiceManager() = default;
    std::unique_ptr<VoldServiceWrapper> getVoldService() override;
    std::unique_ptr<IncrementalManagerWrapper> getIncrementalManager() override;
    std::unique_ptr<IncFsWrapper> getIncFs() override;

private:
    template <class INTERFACE>
    sp<INTERFACE> getRealService(std::string_view serviceName) const;
    sp<android::IServiceManager> mServiceManager;
};

class RealIncFs : public IncFsWrapper {
public:
    RealIncFs() = default;
    ~RealIncFs() = default;
    ErrorCode makeFile(Control control, std::string_view path, int mode, FileId id,
                       NewFileParams params) const override {
        return incfs::makeFile(control, path, mode, id, params);
    }
    ErrorCode makeDir(Control control, std::string_view path, int mode) const override {
        return incfs::makeDir(control, path, mode);
    }
    RawMetadata getMetadata(Control control, FileId fileid) const override {
        return incfs::getMetadata(control, fileid);
    }
    RawMetadata getMetadata(Control control, std::string_view path) const override {
        return incfs::getMetadata(control, path);
    }
    FileId getFileId(Control control, std::string_view path) const override {
        return incfs::getFileId(control, path);
    }
    ErrorCode link(Control control, std::string_view from, std::string_view to) const override {
        return incfs::link(control, from, to);
    }
    ErrorCode unlink(Control control, std::string_view path) const override {
        return incfs::unlink(control, path);
    }
    base::unique_fd openWrite(Control control, FileId id) const override {
        return base::unique_fd{incfs::openWrite(control, id)};
    }
    ErrorCode writeBlocks(Span<const DataBlock> blocks) const override {
        return incfs::writeBlocks(blocks);
    }
};

} // namespace android::os::incremental
