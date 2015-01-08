// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application/application_runner_chromium.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "services/icu_data/data.h"
#include "services/icu_data/icu_data.mojom.h"

namespace icu_data {

class ICUDataImpl : public mojo::ApplicationDelegate,
                    public ICUData,
                    public mojo::InterfaceFactory<ICUData> {
 public:
  ICUDataImpl() {}
  ~ICUDataImpl() override {}

  // mojo::ApplicationDelegate implementation.
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService(this);
    return true;
  }

  // mojo::InterfaceFactory<mojo::ICUData> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<ICUData> request) override {
    bindings_.AddBinding(this, request.Pass());
  }

  void Map(const mojo::String& sha1hash,
           const mojo::Callback<void(mojo::ScopedSharedBufferHandle)>& callback)
      override {
    if (std::string(sha1hash) != std::string(kICUDataTableHash)) {
      callback.Run(mojo::ScopedSharedBufferHandle());
      return;
    }
    EnsureBuffer();
    mojo::ScopedSharedBufferHandle handle;
    // FIXME: We should create a read-only duplicate of the handle.
    mojo::DuplicateBuffer(buffer_->handle.get(), nullptr, &handle);
    callback.Run(handle.Pass());
  }

 private:
  void EnsureBuffer() {
    if (buffer_)
      return;
    buffer_.reset(new mojo::SharedBuffer(kICUDataTableSize));
    void* ptr = nullptr;
    MojoResult rv = mojo::MapBuffer(buffer_->handle.get(), 0, kICUDataTableSize,
                                    &ptr, MOJO_MAP_BUFFER_FLAG_NONE);
    CHECK_EQ(rv, MOJO_RESULT_OK);
    memcpy(ptr, kICUDataTable, kICUDataTableSize);
    rv = mojo::UnmapBuffer(ptr);
    CHECK_EQ(rv, MOJO_RESULT_OK);
  }

  scoped_ptr<mojo::SharedBuffer> buffer_;
  mojo::WeakBindingSet<ICUData> bindings_;
};
}

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new icu_data::ICUDataImpl);
  return runner.Run(shell_handle);
}
