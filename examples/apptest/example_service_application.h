// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_TEST_EXAMPLE_SERVICE_APPLICATION_H_
#define MOJO_EXAMPLES_TEST_EXAMPLE_SERVICE_APPLICATION_H_

#include "examples/apptest/example_service_impl.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {

class ApplicationConnection;

class ExampleServiceApplication : public ApplicationDelegate,
                                  public InterfaceFactory<ExampleService> {
 public:
  ExampleServiceApplication();
  ~ExampleServiceApplication() override;

 private:
  // ApplicationDelegate implementation.
  bool ConfigureIncomingConnection(
      ApplicationConnection* connection) override;

  // InterfaceFactory<ExampleService> implementation.
  void Create(ApplicationConnection* connection,
              InterfaceRequest<ExampleService> request) override;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ExampleServiceApplication);
};

}  // namespace mojo

#endif  // MOJO_EXAMPLES_TEST_EXAMPLE_SERVICE_APPLICATION_H_
