// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_ANDROID_CONTENT_HANDLER_H_
#define MOJO_SHELL_ANDROID_CONTENT_HANDLER_H_

#include <jni.h>

#include "mojo/application/content_handler_factory.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/content_handler/public/interfaces/content_handler.mojom.h"
#include "shell/android/intent_receiver_manager_factory.h"

namespace base {
class FilePath;
}

namespace shell {

class AndroidHandler : public mojo::ApplicationDelegate,
                       public mojo::ContentHandlerFactory::Delegate {
 public:
  AndroidHandler();
  ~AndroidHandler();

 private:
  // mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // mojo::ContentHandlerFactory::Delegate:
  void RunApplication(
      mojo::InterfaceRequest<mojo::Application> application_request,
      mojo::URLResponsePtr response) override;

  mojo::ContentHandlerFactory content_handler_factory_;
  IntentReceiverManagerFactory intent_receiver_manager_factory_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(AndroidHandler);
};

bool RegisterAndroidHandlerJni(JNIEnv* env);

}  // namespace shell

#endif  // MOJO_SHELL_ANDROID_CONTENT_HANDLER_H_
