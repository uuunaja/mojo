// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/local_data_pipe_impl.h"

#include <string.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/edk/system/data_pipe.h"
#include "mojo/edk/system/waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

const uint32_t kSizeOfOptions =
    static_cast<uint32_t>(sizeof(MojoCreateDataPipeOptions));

// Validate options.
TEST(LocalDataPipeImplTest, Creation) {
  // Create using default options.
  {
    // Get default options.
    MojoCreateDataPipeOptions default_options = {0};
    EXPECT_EQ(MOJO_RESULT_OK, DataPipe::ValidateCreateOptions(
                                  NullUserPointer(), &default_options));
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(default_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }

  // Create using non-default options.
  {
    const MojoCreateDataPipeOptions options = {
        kSizeOfOptions,                           // |struct_size|.
        MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
        1,                                        // |element_num_bytes|.
        1000                                      // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions validated_options = {0};
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateCreateOptions(MakeUserPointer(&options),
                                              &validated_options));
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }
  {
    const MojoCreateDataPipeOptions options = {
        kSizeOfOptions,                           // |struct_size|.
        MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
        4,                                        // |element_num_bytes|.
        4000                                      // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions validated_options = {0};
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateCreateOptions(MakeUserPointer(&options),
                                              &validated_options));
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }
  // Default capacity.
  {
    const MojoCreateDataPipeOptions options = {
        kSizeOfOptions,                           // |struct_size|.
        MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
        100,                                      // |element_num_bytes|.
        0                                         // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions validated_options = {0};
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateCreateOptions(MakeUserPointer(&options),
                                              &validated_options));
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }
}

// Note: The "basic" waiting tests test that the "wait states" are correct in
// various situations; they don't test that waiters are properly awoken on state
// changes. (For that, we need to use multiple threads.)
TEST(LocalDataPipeImplTest, BasicProducerWaiting) {
  // Note: We take advantage of the fact that for |LocalDataPipeImpl|,
  // capacities are strict maximums. This is not guaranteed by the API.

  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                           // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),   // |element_num_bytes|.
      2 * sizeof(int32_t)                       // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = {0};
  EXPECT_EQ(MOJO_RESULT_OK, DataPipe::ValidateCreateOptions(
                                MakeUserPointer(&options), &validated_options));

  scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));
  Waiter waiter;
  uint32_t context = 0;
  HandleSignalsState hss;

  // Never readable.
  waiter.Init();
  hss = HandleSignalsState();
  EXPECT_EQ(
      MOJO_RESULT_FAILED_PRECONDITION,
      dp->ProducerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 12, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // Already writable.
  waiter.Init();
  hss = HandleSignalsState();
  EXPECT_EQ(
      MOJO_RESULT_ALREADY_EXISTS,
      dp->ProducerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 34, &hss));

  // Write two elements.
  int32_t elements[2] = {123, 456};
  uint32_t num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(UserPointer<const void>(elements),
                                  MakeUserPointer(&num_bytes), true));
  EXPECT_EQ(static_cast<uint32_t>(2u * sizeof(elements[0])), num_bytes);

  // Adding a waiter should now succeed.
  waiter.Init();
  ASSERT_EQ(MOJO_RESULT_OK,
            dp->ProducerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 56,
                                    nullptr));
  // And it shouldn't be writable yet.
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0, nullptr));
  hss = HandleSignalsState();
  dp->ProducerRemoveAwakable(&waiter, &hss);
  EXPECT_EQ(0u, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // Peek one element.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerReadData(UserPointer<void>(elements),
                                 MakeUserPointer(&num_bytes), true, true));
  EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
  EXPECT_EQ(123, elements[0]);
  EXPECT_EQ(-1, elements[1]);

  // Add a waiter.
  waiter.Init();
  ASSERT_EQ(MOJO_RESULT_OK,
            dp->ProducerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 56,
                                    nullptr));
  // And it still shouldn't be writable yet.
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0, nullptr));
  hss = HandleSignalsState();
  dp->ProducerRemoveAwakable(&waiter, &hss);
  EXPECT_EQ(0u, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // Do it again.
  waiter.Init();
  ASSERT_EQ(MOJO_RESULT_OK,
            dp->ProducerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 78,
                                    nullptr));

  // Read one element.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerReadData(UserPointer<void>(elements),
                                 MakeUserPointer(&num_bytes), true, false));
  EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
  EXPECT_EQ(123, elements[0]);
  EXPECT_EQ(-1, elements[1]);

  // Waiting should now succeed.
  EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(1000, &context));
  EXPECT_EQ(78u, context);
  hss = HandleSignalsState();
  dp->ProducerRemoveAwakable(&waiter, &hss);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // Try writing, using a two-phase write.
  void* buffer = nullptr;
  num_bytes = static_cast<uint32_t>(3u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(MakeUserPointer(&buffer),
                                       MakeUserPointer(&num_bytes), false));
  EXPECT_TRUE(buffer);
  EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);

  static_cast<int32_t*>(buffer)[0] = 789;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(static_cast<uint32_t>(
                                1u * sizeof(elements[0]))));

  // Add a waiter.
  waiter.Init();
  ASSERT_EQ(MOJO_RESULT_OK,
            dp->ProducerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 90,
                                    nullptr));

  // Read one element, using a two-phase read.
  const void* read_buffer = nullptr;
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(MakeUserPointer(&read_buffer),
                                      MakeUserPointer(&num_bytes), false));
  EXPECT_TRUE(read_buffer);
  // Since we only read one element (after having written three in all), the
  // two-phase read should only allow us to read one. This checks an
  // implementation detail!
  EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
  EXPECT_EQ(456, static_cast<const int32_t*>(read_buffer)[0]);
  EXPECT_EQ(
      MOJO_RESULT_OK,
      dp->ConsumerEndReadData(static_cast<uint32_t>(1u * sizeof(elements[0]))));

  // Waiting should succeed.
  EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(1000, &context));
  EXPECT_EQ(90u, context);
  hss = HandleSignalsState();
  dp->ProducerRemoveAwakable(&waiter, &hss);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // Write one element.
  elements[0] = 123;
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(UserPointer<const void>(elements),
                                  MakeUserPointer(&num_bytes), false));
  EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);

  // Add a waiter.
  waiter.Init();
  ASSERT_EQ(MOJO_RESULT_OK,
            dp->ProducerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 12,
                                    nullptr));

  // Close the consumer.
  dp->ConsumerClose();

  // It should now be never-writable.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, waiter.Wait(1000, &context));
  EXPECT_EQ(12u, context);
  hss = HandleSignalsState();
  dp->ProducerRemoveAwakable(&waiter, &hss);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);

  dp->ProducerClose();
}

TEST(LocalDataPipeImplTest, PeerClosedWaiting) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                           // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),   // |element_num_bytes|.
      2 * sizeof(int32_t)                       // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = {0};
  EXPECT_EQ(MOJO_RESULT_OK, DataPipe::ValidateCreateOptions(
                                MakeUserPointer(&options), &validated_options));

  Waiter waiter;
  HandleSignalsState hss;

  // Check MOJO_HANDLE_SIGNAL_PEER_CLOSED on producer.
  {
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));
    // Add a waiter.
    waiter.Init();
    ASSERT_EQ(MOJO_RESULT_OK,
              dp->ProducerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                                      12, nullptr));

    // Close the consumer.
    dp->ConsumerClose();

    // It should be signaled.
    uint32_t context = 0;
    EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(1000, &context));
    EXPECT_EQ(12u, context);
    hss = HandleSignalsState();
    dp->ProducerRemoveAwakable(&waiter, &hss);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);

    dp->ProducerClose();
  }

  // Check MOJO_HANDLE_SIGNAL_PEER_CLOSED on consumer.
  {
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));
    // Add a waiter.
    waiter.Init();
    ASSERT_EQ(MOJO_RESULT_OK,
              dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                                      12, nullptr));

    // Close the producer.
    dp->ProducerClose();

    // It should be signaled.
    uint32_t context = 0;
    EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(1000, &context));
    EXPECT_EQ(12u, context);
    hss = HandleSignalsState();
    dp->ConsumerRemoveAwakable(&waiter, &hss);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);

    dp->ConsumerClose();
  }
}

TEST(LocalDataPipeImplTest, BasicConsumerWaiting) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                           // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),   // |element_num_bytes|.
      1000 * sizeof(int32_t)                    // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = {0};
  EXPECT_EQ(MOJO_RESULT_OK, DataPipe::ValidateCreateOptions(
                                MakeUserPointer(&options), &validated_options));

  {
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));
    Waiter waiter;
    uint32_t context = 0;
    HandleSignalsState hss;

    // Never writable.
    waiter.Init();
    hss = HandleSignalsState();
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 12,
                                      &hss));
    EXPECT_EQ(0u, hss.satisfied_signals);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
              hss.satisfiable_signals);

    // Not yet readable.
    waiter.Init();
    ASSERT_EQ(MOJO_RESULT_OK,
              dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 34,
                                      nullptr));
    EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0, nullptr));
    hss = HandleSignalsState();
    dp->ConsumerRemoveAwakable(&waiter, &hss);
    EXPECT_EQ(0u, hss.satisfied_signals);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
              hss.satisfiable_signals);

    // Write two elements.
    int32_t elements[2] = {123, 456};
    uint32_t num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(UserPointer<const void>(elements),
                                    MakeUserPointer(&num_bytes), true));

    // Should already be readable.
    waiter.Init();
    hss = HandleSignalsState();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 56,
                                      &hss));
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
              hss.satisfiable_signals);

    // Discard one element.
    num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerDiscardData(MakeUserPointer(&num_bytes), true));
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);

    // Should still be readable.
    waiter.Init();
    hss = HandleSignalsState();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 78,
                                      &hss));
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
              hss.satisfiable_signals);

    // Peek one element.
    elements[0] = -1;
    elements[1] = -1;
    num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerReadData(UserPointer<void>(elements),
                                   MakeUserPointer(&num_bytes), true, true));
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
    EXPECT_EQ(456, elements[0]);
    EXPECT_EQ(-1, elements[1]);

    // Should still be readable.
    waiter.Init();
    hss = HandleSignalsState();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 78,
                                      &hss));
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
              hss.satisfiable_signals);

    // Read one element.
    elements[0] = -1;
    elements[1] = -1;
    num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerReadData(UserPointer<void>(elements),
                                   MakeUserPointer(&num_bytes), true, false));
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
    EXPECT_EQ(456, elements[0]);
    EXPECT_EQ(-1, elements[1]);

    // Adding a waiter should now succeed.
    waiter.Init();
    ASSERT_EQ(MOJO_RESULT_OK,
              dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 90,
                                      nullptr));

    // Write one element.
    elements[0] = 789;
    elements[1] = -1;
    num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(UserPointer<const void>(elements),
                                    MakeUserPointer(&num_bytes), true));

    // Waiting should now succeed.
    EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(1000, &context));
    EXPECT_EQ(90u, context);
    hss = HandleSignalsState();
    dp->ConsumerRemoveAwakable(&waiter, &hss);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
              hss.satisfiable_signals);

    // Close the producer.
    dp->ProducerClose();

    // Should still be readable.
    waiter.Init();
    hss = HandleSignalsState();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 12,
                                      &hss));
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
              hss.satisfied_signals);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
              hss.satisfiable_signals);

    // Read one element.
    elements[0] = -1;
    elements[1] = -1;
    num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerReadData(UserPointer<void>(elements),
                                   MakeUserPointer(&num_bytes), true, false));
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
    EXPECT_EQ(789, elements[0]);
    EXPECT_EQ(-1, elements[1]);

    // Should be never-readable.
    waiter.Init();
    hss = HandleSignalsState();
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 34,
                                      &hss));
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);

    dp->ConsumerClose();
  }

  // Test with two-phase APIs and closing the producer with an active consumer
  // waiter.
  {
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));
    Waiter waiter;
    uint32_t context = 0;
    HandleSignalsState hss;

    // Write two elements.
    int32_t* elements = nullptr;
    void* buffer = nullptr;
    // Request room for three (but we'll only write two).
    uint32_t num_bytes = static_cast<uint32_t>(3u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerBeginWriteData(MakeUserPointer(&buffer),
                                         MakeUserPointer(&num_bytes), true));
    EXPECT_TRUE(buffer);
    EXPECT_GE(num_bytes, static_cast<uint32_t>(3u * sizeof(elements[0])));
    elements = static_cast<int32_t*>(buffer);
    elements[0] = 123;
    elements[1] = 456;
    EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(static_cast<uint32_t>(
                                  2u * sizeof(elements[0]))));

    // Should already be readable.
    waiter.Init();
    hss = HandleSignalsState();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 12,
                                      &hss));
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
              hss.satisfiable_signals);

    // Read one element.
    // Request two in all-or-none mode, but only read one.
    const void* read_buffer = nullptr;
    num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(MakeUserPointer(&read_buffer),
                                        MakeUserPointer(&num_bytes), true));
    EXPECT_TRUE(read_buffer);
    EXPECT_EQ(static_cast<uint32_t>(2u * sizeof(elements[0])), num_bytes);
    const int32_t* read_elements = static_cast<const int32_t*>(read_buffer);
    EXPECT_EQ(123, read_elements[0]);
    EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(static_cast<uint32_t>(
                                  1u * sizeof(elements[0]))));

    // Should still be readable.
    waiter.Init();
    hss = HandleSignalsState();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 34,
                                      &hss));
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
              hss.satisfiable_signals);

    // Read one element.
    // Request three, but not in all-or-none mode.
    read_buffer = nullptr;
    num_bytes = static_cast<uint32_t>(3u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(MakeUserPointer(&read_buffer),
                                        MakeUserPointer(&num_bytes), false));
    EXPECT_TRUE(read_buffer);
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
    read_elements = static_cast<const int32_t*>(read_buffer);
    EXPECT_EQ(456, read_elements[0]);
    EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(static_cast<uint32_t>(
                                  1u * sizeof(elements[0]))));

    // Adding a waiter should now succeed.
    waiter.Init();
    ASSERT_EQ(MOJO_RESULT_OK,
              dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 56,
                                      nullptr));

    // Close the producer.
    dp->ProducerClose();

    // Should be never-readable.
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, waiter.Wait(1000, &context));
    EXPECT_EQ(56u, context);
    hss = HandleSignalsState();
    dp->ConsumerRemoveAwakable(&waiter, &hss);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);

    dp->ConsumerClose();
  }
}

// Tests that data pipes aren't writable/readable during two-phase writes/reads.
TEST(LocalDataPipeImplTest, BasicTwoPhaseWaiting) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                           // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),   // |element_num_bytes|.
      1000 * sizeof(int32_t)                    // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = {0};
  EXPECT_EQ(MOJO_RESULT_OK, DataPipe::ValidateCreateOptions(
                                MakeUserPointer(&options), &validated_options));

  scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));
  Waiter waiter;
  HandleSignalsState hss;

  // It should be writable.
  waiter.Init();
  hss = HandleSignalsState();
  EXPECT_EQ(
      MOJO_RESULT_ALREADY_EXISTS,
      dp->ProducerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 0, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  uint32_t num_bytes = static_cast<uint32_t>(1u * sizeof(int32_t));
  void* write_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(MakeUserPointer(&write_ptr),
                                       MakeUserPointer(&num_bytes), false));
  EXPECT_TRUE(write_ptr);
  EXPECT_GE(num_bytes, static_cast<uint32_t>(1u * sizeof(int32_t)));

  // At this point, it shouldn't be writable.
  waiter.Init();
  ASSERT_EQ(MOJO_RESULT_OK,
            dp->ProducerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 1,
                                    nullptr));
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0, nullptr));
  hss = HandleSignalsState();
  dp->ProducerRemoveAwakable(&waiter, &hss);
  EXPECT_EQ(0u, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // It shouldn't be readable yet either.
  waiter.Init();
  ASSERT_EQ(MOJO_RESULT_OK,
            dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 2,
                                    nullptr));
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0, nullptr));
  hss = HandleSignalsState();
  dp->ConsumerRemoveAwakable(&waiter, &hss);
  EXPECT_EQ(0u, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  static_cast<int32_t*>(write_ptr)[0] = 123;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(
                                static_cast<uint32_t>(1u * sizeof(int32_t))));

  // It should be writable again.
  waiter.Init();
  hss = HandleSignalsState();
  EXPECT_EQ(
      MOJO_RESULT_ALREADY_EXISTS,
      dp->ProducerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 3, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // And readable.
  waiter.Init();
  hss = HandleSignalsState();
  EXPECT_EQ(
      MOJO_RESULT_ALREADY_EXISTS,
      dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 4, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // Start another two-phase write and check that it's readable even in the
  // middle of it.
  num_bytes = static_cast<uint32_t>(1u * sizeof(int32_t));
  write_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(MakeUserPointer(&write_ptr),
                                       MakeUserPointer(&num_bytes), false));
  EXPECT_TRUE(write_ptr);
  EXPECT_GE(num_bytes, static_cast<uint32_t>(1u * sizeof(int32_t)));

  // It should be readable.
  waiter.Init();
  hss = HandleSignalsState();
  EXPECT_EQ(
      MOJO_RESULT_ALREADY_EXISTS,
      dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 5, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // End the two-phase write without writing anything.
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(0u));

  // Start a two-phase read.
  num_bytes = static_cast<uint32_t>(1u * sizeof(int32_t));
  const void* read_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(MakeUserPointer(&read_ptr),
                                      MakeUserPointer(&num_bytes), false));
  EXPECT_TRUE(read_ptr);
  EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(int32_t)), num_bytes);

  // At this point, it should still be writable.
  waiter.Init();
  hss = HandleSignalsState();
  EXPECT_EQ(
      MOJO_RESULT_ALREADY_EXISTS,
      dp->ProducerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 6, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // But not readable.
  waiter.Init();
  ASSERT_EQ(MOJO_RESULT_OK,
            dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 7,
                                    nullptr));
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0, nullptr));
  hss = HandleSignalsState();
  dp->ConsumerRemoveAwakable(&waiter, &hss);
  EXPECT_EQ(0u, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  // End the two-phase read without reading anything.
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(0u));

  // It should be readable again.
  waiter.Init();
  hss = HandleSignalsState();
  EXPECT_EQ(
      MOJO_RESULT_ALREADY_EXISTS,
      dp->ConsumerAddAwakable(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 8, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfiable_signals);

  dp->ProducerClose();
  dp->ConsumerClose();
}

void Seq(int32_t start, size_t count, int32_t* out) {
  for (size_t i = 0; i < count; i++)
    out[i] = start + static_cast<int32_t>(i);
}

TEST(LocalDataPipeImplTest, AllOrNone) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                           // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),   // |element_num_bytes|.
      10 * sizeof(int32_t)                      // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = {0};
  EXPECT_EQ(MOJO_RESULT_OK, DataPipe::ValidateCreateOptions(
                                MakeUserPointer(&options), &validated_options));

  scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));

  // Try writing way too much.
  uint32_t num_bytes = 20u * sizeof(int32_t);
  int32_t buffer[100];
  Seq(0, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ProducerWriteData(UserPointer<const void>(buffer),
                                  MakeUserPointer(&num_bytes), true));

  // Should still be empty.
  num_bytes = ~0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(0u, num_bytes);

  // Write some data.
  num_bytes = 5u * sizeof(int32_t);
  Seq(100, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(UserPointer<const void>(buffer),
                                  MakeUserPointer(&num_bytes), true));
  EXPECT_EQ(5u * sizeof(int32_t), num_bytes);

  // Half full.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(5u * sizeof(int32_t), num_bytes);

  // Too much.
  num_bytes = 6u * sizeof(int32_t);
  Seq(200, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ProducerWriteData(UserPointer<const void>(buffer),
                                  MakeUserPointer(&num_bytes), true));

  // Try reading too much.
  num_bytes = 11u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ConsumerReadData(UserPointer<void>(buffer),
                                 MakeUserPointer(&num_bytes), true, false));
  int32_t expected_buffer[100];
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Try discarding too much.
  num_bytes = 11u * sizeof(int32_t);
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ConsumerDiscardData(MakeUserPointer(&num_bytes), true));

  // Just a little.
  num_bytes = 2u * sizeof(int32_t);
  Seq(300, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(UserPointer<const void>(buffer),
                                  MakeUserPointer(&num_bytes), true));
  EXPECT_EQ(2u * sizeof(int32_t), num_bytes);

  // Just right.
  num_bytes = 3u * sizeof(int32_t);
  Seq(400, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(UserPointer<const void>(buffer),
                                  MakeUserPointer(&num_bytes), true));
  EXPECT_EQ(3u * sizeof(int32_t), num_bytes);

  // Exactly full.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(10u * sizeof(int32_t), num_bytes);

  // Read half.
  num_bytes = 5u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerReadData(UserPointer<void>(buffer),
                                 MakeUserPointer(&num_bytes), true, false));
  EXPECT_EQ(5u * sizeof(int32_t), num_bytes);
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  Seq(100, 5, expected_buffer);
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Try reading too much again.
  num_bytes = 6u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ConsumerReadData(UserPointer<void>(buffer),
                                 MakeUserPointer(&num_bytes), true, false));
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Try discarding too much again.
  num_bytes = 6u * sizeof(int32_t);
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ConsumerDiscardData(MakeUserPointer(&num_bytes), true));

  // Discard a little.
  num_bytes = 2u * sizeof(int32_t);
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerDiscardData(MakeUserPointer(&num_bytes), true));
  EXPECT_EQ(2u * sizeof(int32_t), num_bytes);

  // Three left.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(3u * sizeof(int32_t), num_bytes);

  // Close the producer, then test producer-closed cases.
  dp->ProducerClose();

  // Try reading too much; "failed precondition" since the producer is closed.
  num_bytes = 4u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            dp->ConsumerReadData(UserPointer<void>(buffer),
                                 MakeUserPointer(&num_bytes), true, false));
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Try discarding too much; "failed precondition" again.
  num_bytes = 4u * sizeof(int32_t);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            dp->ConsumerDiscardData(MakeUserPointer(&num_bytes), true));

  // Read a little.
  num_bytes = 2u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerReadData(UserPointer<void>(buffer),
                                 MakeUserPointer(&num_bytes), true, false));
  EXPECT_EQ(2u * sizeof(int32_t), num_bytes);
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  Seq(400, 2, expected_buffer);
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Discard the remaining element.
  num_bytes = 1u * sizeof(int32_t);
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerDiscardData(MakeUserPointer(&num_bytes), true));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);

  // Empty again.
  num_bytes = ~0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(0u, num_bytes);

  dp->ConsumerClose();
}

TEST(LocalDataPipeImplTest, TwoPhaseAllOrNone) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                           // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),   // |element_num_bytes|.
      10 * sizeof(int32_t)                      // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = {0};
  EXPECT_EQ(MOJO_RESULT_OK, DataPipe::ValidateCreateOptions(
                                MakeUserPointer(&options), &validated_options));

  scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));

  // Try writing way too much (two-phase).
  uint32_t num_bytes = 20u * sizeof(int32_t);
  void* write_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ProducerBeginWriteData(MakeUserPointer(&write_ptr),
                                       MakeUserPointer(&num_bytes), true));

  // Try writing an amount which isn't a multiple of the element size
  // (two-phase).
  static_assert(sizeof(int32_t) > 1u, "Wow! int32_t's have size 1");
  num_bytes = 1u;
  write_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dp->ProducerBeginWriteData(MakeUserPointer(&write_ptr),
                                       MakeUserPointer(&num_bytes), true));

  // Try reading way too much (two-phase).
  num_bytes = 20u * sizeof(int32_t);
  const void* read_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ConsumerBeginReadData(MakeUserPointer(&read_ptr),
                                      MakeUserPointer(&num_bytes), true));

  // Write half (two-phase).
  num_bytes = 5u * sizeof(int32_t);
  write_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(MakeUserPointer(&write_ptr),
                                       MakeUserPointer(&num_bytes), true));
  // May provide more space than requested.
  EXPECT_GE(num_bytes, 5u * sizeof(int32_t));
  EXPECT_TRUE(write_ptr);
  Seq(0, 5, static_cast<int32_t*>(write_ptr));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(5u * sizeof(int32_t)));

  // Try reading an amount which isn't a multiple of the element size
  // (two-phase).
  num_bytes = 1u;
  read_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dp->ConsumerBeginReadData(MakeUserPointer(&read_ptr),
                                      MakeUserPointer(&num_bytes), true));

  // Read one (two-phase).
  num_bytes = 1u * sizeof(int32_t);
  read_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(MakeUserPointer(&read_ptr),
                                      MakeUserPointer(&num_bytes), true));
  EXPECT_GE(num_bytes, 1u * sizeof(int32_t));
  EXPECT_EQ(0, static_cast<const int32_t*>(read_ptr)[0]);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(1u * sizeof(int32_t)));

  // We should have four left, leaving room for six.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(4u * sizeof(int32_t), num_bytes);

  // Assuming a tight circular buffer of the specified capacity, we can't do a
  // two-phase write of six now.
  num_bytes = 6u * sizeof(int32_t);
  write_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ProducerBeginWriteData(MakeUserPointer(&write_ptr),
                                       MakeUserPointer(&num_bytes), true));

  // Write six elements (simple), filling the buffer.
  num_bytes = 6u * sizeof(int32_t);
  int32_t buffer[100];
  Seq(100, 6, buffer);
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(UserPointer<const void>(buffer),
                                  MakeUserPointer(&num_bytes), true));
  EXPECT_EQ(6u * sizeof(int32_t), num_bytes);

  // We have ten.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(10u * sizeof(int32_t), num_bytes);

  // But a two-phase read of ten should fail.
  num_bytes = 10u * sizeof(int32_t);
  read_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ConsumerBeginReadData(MakeUserPointer(&read_ptr),
                                      MakeUserPointer(&num_bytes), true));

  // Close the producer.
  dp->ProducerClose();

  // A two-phase read of nine should work.
  num_bytes = 9u * sizeof(int32_t);
  read_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(MakeUserPointer(&read_ptr),
                                      MakeUserPointer(&num_bytes), true));
  EXPECT_GE(num_bytes, 9u * sizeof(int32_t));
  EXPECT_EQ(1, static_cast<const int32_t*>(read_ptr)[0]);
  EXPECT_EQ(2, static_cast<const int32_t*>(read_ptr)[1]);
  EXPECT_EQ(3, static_cast<const int32_t*>(read_ptr)[2]);
  EXPECT_EQ(4, static_cast<const int32_t*>(read_ptr)[3]);
  EXPECT_EQ(100, static_cast<const int32_t*>(read_ptr)[4]);
  EXPECT_EQ(101, static_cast<const int32_t*>(read_ptr)[5]);
  EXPECT_EQ(102, static_cast<const int32_t*>(read_ptr)[6]);
  EXPECT_EQ(103, static_cast<const int32_t*>(read_ptr)[7]);
  EXPECT_EQ(104, static_cast<const int32_t*>(read_ptr)[8]);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(9u * sizeof(int32_t)));

  // A two-phase read of two should fail, with "failed precondition".
  num_bytes = 2u * sizeof(int32_t);
  read_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            dp->ConsumerBeginReadData(MakeUserPointer(&read_ptr),
                                      MakeUserPointer(&num_bytes), true));

  dp->ConsumerClose();
}

// Tests that |ProducerWriteData()| and |ConsumerReadData()| writes and reads,
// respectively, as much as possible, even if it has to "wrap around" the
// internal circular buffer. (Note that the two-phase write and read do not do
// this.)
TEST(LocalDataPipeImplTest, WrapAround) {
  unsigned char test_data[1000];
  for (size_t i = 0; i < arraysize(test_data); i++)
    test_data[i] = static_cast<unsigned char>(i);

  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                           // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      1u,                                       // |element_num_bytes|.
      100u                                      // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = {0};
  EXPECT_EQ(MOJO_RESULT_OK, DataPipe::ValidateCreateOptions(
                                MakeUserPointer(&options), &validated_options));
  // This test won't be valid if |ValidateCreateOptions()| decides to give the
  // pipe more space.
  ASSERT_EQ(100u, validated_options.capacity_num_bytes);

  scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));

  // Write 20 bytes.
  uint32_t num_bytes = 20u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(UserPointer<const void>(&test_data[0]),
                                  MakeUserPointer(&num_bytes), false));
  EXPECT_EQ(20u, num_bytes);

  // Read 10 bytes.
  unsigned char read_buffer[1000] = {0};
  num_bytes = 10u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerReadData(UserPointer<void>(read_buffer),
                                 MakeUserPointer(&num_bytes), false, false));
  EXPECT_EQ(10u, num_bytes);
  EXPECT_EQ(0, memcmp(read_buffer, &test_data[0], 10u));

  // Check that a two-phase write can now only write (at most) 80 bytes. (This
  // checks an implementation detail; this behavior is not guaranteed, but we
  // need it for this test.)
  void* write_buffer_ptr = nullptr;
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(MakeUserPointer(&write_buffer_ptr),
                                       MakeUserPointer(&num_bytes), false));
  EXPECT_TRUE(write_buffer_ptr);
  EXPECT_EQ(80u, num_bytes);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(0u));

  // Write as much data as we can (using |ProducerWriteData()|). We should write
  // 90 bytes.
  num_bytes = 200u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(UserPointer<const void>(&test_data[20]),
                                  MakeUserPointer(&num_bytes), false));
  EXPECT_EQ(90u, num_bytes);

  // Check that a two-phase read can now only read (at most) 90 bytes. (This
  // checks an implementation detail; this behavior is not guaranteed, but we
  // need it for this test.)
  const void* read_buffer_ptr = nullptr;
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(MakeUserPointer(&read_buffer_ptr),
                                      MakeUserPointer(&num_bytes), false));
  EXPECT_TRUE(read_buffer_ptr);
  EXPECT_EQ(90u, num_bytes);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(0u));

  // Read as much as possible (using |ConsumerReadData()|). We should read 100
  // bytes.
  num_bytes =
      static_cast<uint32_t>(arraysize(read_buffer) * sizeof(read_buffer[0]));
  memset(read_buffer, 0, num_bytes);
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerReadData(UserPointer<void>(read_buffer),
                                 MakeUserPointer(&num_bytes), false, false));
  EXPECT_EQ(100u, num_bytes);
  EXPECT_EQ(0, memcmp(read_buffer, &test_data[10], 100u));

  dp->ProducerClose();
  dp->ConsumerClose();
}

// Tests the behavior of closing the producer or consumer with respect to
// writes and reads (simple and two-phase).
TEST(LocalDataPipeImplTest, CloseWriteRead) {
  const char kTestData[] = "hello world";
  const uint32_t kTestDataSize = static_cast<uint32_t>(sizeof(kTestData));

  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                           // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      1u,                                       // |element_num_bytes|.
      1000u                                     // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = {0};
  EXPECT_EQ(MOJO_RESULT_OK, DataPipe::ValidateCreateOptions(
                                MakeUserPointer(&options), &validated_options));

  // Close producer first, then consumer.
  {
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));

    // Write some data, so we'll have something to read.
    uint32_t num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(UserPointer<const void>(kTestData),
                                    MakeUserPointer(&num_bytes), false));
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Write it again, so we'll have something left over.
    num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(UserPointer<const void>(kTestData),
                                    MakeUserPointer(&num_bytes), false));
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Start two-phase write.
    void* write_buffer_ptr = nullptr;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerBeginWriteData(MakeUserPointer(&write_buffer_ptr),
                                         MakeUserPointer(&num_bytes), false));
    EXPECT_TRUE(write_buffer_ptr);
    EXPECT_GT(num_bytes, 0u);

    // Start two-phase read.
    const void* read_buffer_ptr = nullptr;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(MakeUserPointer(&read_buffer_ptr),
                                        MakeUserPointer(&num_bytes), false));
    EXPECT_TRUE(read_buffer_ptr);
    EXPECT_EQ(2u * kTestDataSize, num_bytes);

    // Close the producer.
    dp->ProducerClose();

    // The consumer can finish its two-phase read.
    EXPECT_EQ(0, memcmp(read_buffer_ptr, kTestData, kTestDataSize));
    EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(kTestDataSize));

    // And start another.
    read_buffer_ptr = nullptr;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(MakeUserPointer(&read_buffer_ptr),
                                        MakeUserPointer(&num_bytes), false));
    EXPECT_TRUE(read_buffer_ptr);
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Close the consumer, which cancels the two-phase read.
    dp->ConsumerClose();
  }

  // Close consumer first, then producer.
  {
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));

    // Write some data, so we'll have something to read.
    uint32_t num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(UserPointer<const void>(kTestData),
                                    MakeUserPointer(&num_bytes), false));
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Start two-phase write.
    void* write_buffer_ptr = nullptr;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerBeginWriteData(MakeUserPointer(&write_buffer_ptr),
                                         MakeUserPointer(&num_bytes), false));
    EXPECT_TRUE(write_buffer_ptr);
    ASSERT_GT(num_bytes, kTestDataSize);

    // Start two-phase read.
    const void* read_buffer_ptr = nullptr;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(MakeUserPointer(&read_buffer_ptr),
                                        MakeUserPointer(&num_bytes), false));
    EXPECT_TRUE(read_buffer_ptr);
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Close the consumer.
    dp->ConsumerClose();

    // Actually write some data. (Note: Premature freeing of the buffer would
    // probably only be detected under ASAN or similar.)
    memcpy(write_buffer_ptr, kTestData, kTestDataSize);
    // Note: Even though the consumer has been closed, ending the two-phase
    // write will report success.
    EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(kTestDataSize));

    // But trying to write should result in failure.
    num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ProducerWriteData(UserPointer<const void>(kTestData),
                                    MakeUserPointer(&num_bytes), false));

    // As will trying to start another two-phase write.
    write_buffer_ptr = nullptr;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ProducerBeginWriteData(MakeUserPointer(&write_buffer_ptr),
                                         MakeUserPointer(&num_bytes), false));

    dp->ProducerClose();
  }

  // Test closing the consumer first, then the producer, with an active
  // two-phase write.
  {
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));

    // Start two-phase write.
    void* write_buffer_ptr = nullptr;
    uint32_t num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerBeginWriteData(MakeUserPointer(&write_buffer_ptr),
                                         MakeUserPointer(&num_bytes), false));
    EXPECT_TRUE(write_buffer_ptr);
    ASSERT_GT(num_bytes, kTestDataSize);

    dp->ConsumerClose();
    dp->ProducerClose();
  }

  // Test closing the producer and then trying to read (with no data).
  {
    scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));

    // Write some data, so we'll have something to read.
    uint32_t num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(UserPointer<const void>(kTestData),
                                    MakeUserPointer(&num_bytes), false));
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Close the producer.
    dp->ProducerClose();

    // Peek that data.
    char buffer[1000];
    num_bytes = static_cast<uint32_t>(sizeof(buffer));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerReadData(UserPointer<void>(buffer),
                                   MakeUserPointer(&num_bytes), false, true));
    EXPECT_EQ(kTestDataSize, num_bytes);
    EXPECT_EQ(0, memcmp(buffer, kTestData, kTestDataSize));

    // Read that data.
    memset(buffer, 0, 1000);
    num_bytes = static_cast<uint32_t>(sizeof(buffer));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerReadData(UserPointer<void>(buffer),
                                   MakeUserPointer(&num_bytes), false, false));
    EXPECT_EQ(kTestDataSize, num_bytes);
    EXPECT_EQ(0, memcmp(buffer, kTestData, kTestDataSize));

    // A second read should fail.
    num_bytes = static_cast<uint32_t>(sizeof(buffer));
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerReadData(UserPointer<void>(buffer),
                                   MakeUserPointer(&num_bytes), false, false));

    // A two-phase read should also fail.
    const void* read_buffer_ptr = nullptr;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerBeginReadData(MakeUserPointer(&read_buffer_ptr),
                                        MakeUserPointer(&num_bytes), false));

    // Ditto for discard.
    num_bytes = 10u;
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerDiscardData(MakeUserPointer(&num_bytes), false));

    dp->ConsumerClose();
  }
}

TEST(LocalDataPipeImplTest, TwoPhaseMoreInvalidArguments) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                           // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),   // |element_num_bytes|.
      10 * sizeof(int32_t)                      // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = {0};
  EXPECT_EQ(MOJO_RESULT_OK, DataPipe::ValidateCreateOptions(
                                MakeUserPointer(&options), &validated_options));

  scoped_refptr<DataPipe> dp(DataPipe::CreateLocal(validated_options));

  // No data.
  uint32_t num_bytes = 1000u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(0u, num_bytes);

  // Try "ending" a two-phase write when one isn't active.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            dp->ProducerEndWriteData(1u * sizeof(int32_t)));

  // Still no data.
  num_bytes = 1000u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(0u, num_bytes);

  // Try ending a two-phase write with an invalid amount (too much).
  num_bytes = 0u;
  void* write_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(MakeUserPointer(&write_ptr),
                                       MakeUserPointer(&num_bytes), false));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dp->ProducerEndWriteData(num_bytes +
                                     static_cast<uint32_t>(sizeof(int32_t))));

  // But the two-phase write still ended.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, dp->ProducerEndWriteData(0u));

  // Still no data.
  num_bytes = 1000u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(0u, num_bytes);

  // Try ending a two-phase write with an invalid amount (not a multiple of the
  // element size).
  num_bytes = 0u;
  write_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(MakeUserPointer(&write_ptr),
                                       MakeUserPointer(&num_bytes), false));
  EXPECT_GE(num_bytes, 1u);
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, dp->ProducerEndWriteData(1u));

  // But the two-phase write still ended.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, dp->ProducerEndWriteData(0u));

  // Still no data.
  num_bytes = 1000u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(0u, num_bytes);

  // Now write some data, so we'll be able to try reading.
  int32_t element = 123;
  num_bytes = 1u * sizeof(int32_t);
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(UserPointer<const void>(&element),
                                  MakeUserPointer(&num_bytes), false));

  // One element available.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);

  // Try "ending" a two-phase read when one isn't active.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            dp->ConsumerEndReadData(1u * sizeof(int32_t)));

  // Still one element available.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);

  // Try ending a two-phase read with an invalid amount (too much).
  num_bytes = 0u;
  const void* read_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(MakeUserPointer(&read_ptr),
                                      MakeUserPointer(&num_bytes), false));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dp->ConsumerEndReadData(num_bytes +
                                    static_cast<uint32_t>(sizeof(int32_t))));

  // Still one element available.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);

  // Try ending a two-phase read with an invalid amount (not a multiple of the
  // element size).
  num_bytes = 0u;
  read_ptr = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(MakeUserPointer(&read_ptr),
                                      MakeUserPointer(&num_bytes), false));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);
  EXPECT_EQ(123, static_cast<const int32_t*>(read_ptr)[0]);
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, dp->ConsumerEndReadData(1u));

  // Still one element available.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(MakeUserPointer(&num_bytes)));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);

  dp->ProducerClose();
  dp->ConsumerClose();
}

}  // namespace
}  // namespace system
}  // namespace mojo
