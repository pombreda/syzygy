// Copyright 2014 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "syzygy/agent/asan/error_info.h"

#include <windows.h>

#include "gtest/gtest.h"
#include "syzygy/agent/asan/unittest_util.h"
#include "syzygy/crashdata/json.h"

namespace agent {
namespace asan {

namespace {

typedef testing::TestWithAsanRuntime AsanErrorInfoTest;

}  // namespace

TEST_F(AsanErrorInfoTest, ErrorInfoAccessTypeToStr) {
  EXPECT_EQ(kHeapUseAfterFree, ErrorInfoAccessTypeToStr(USE_AFTER_FREE));
  EXPECT_EQ(kHeapBufferUnderFlow,
            ErrorInfoAccessTypeToStr(HEAP_BUFFER_UNDERFLOW));
  EXPECT_EQ(kHeapBufferOverFlow,
            ErrorInfoAccessTypeToStr(HEAP_BUFFER_OVERFLOW));
  EXPECT_EQ(kAttemptingDoubleFree, ErrorInfoAccessTypeToStr(DOUBLE_FREE));
  EXPECT_EQ(kInvalidAddress, ErrorInfoAccessTypeToStr(INVALID_ADDRESS));
  EXPECT_EQ(kWildAccess, ErrorInfoAccessTypeToStr(WILD_ACCESS));
  EXPECT_EQ(kHeapUnknownError, ErrorInfoAccessTypeToStr(UNKNOWN_BAD_ACCESS));
  EXPECT_EQ(kHeapCorruptBlock, ErrorInfoAccessTypeToStr(CORRUPT_BLOCK));
  EXPECT_EQ(kCorruptHeap, ErrorInfoAccessTypeToStr(CORRUPT_HEAP));
}

TEST_F(AsanErrorInfoTest, ErrorInfoGetBadAccessInformation) {
  testing::FakeAsanBlock fake_block(kShadowRatioLog, runtime_->stack_cache());
  const size_t kAllocSize = 100;
  EXPECT_TRUE(fake_block.InitializeBlock(kAllocSize));

  AsanErrorInfo error_info = {};
  error_info.location = fake_block.block_info.body +
      kAllocSize + 1;
  EXPECT_TRUE(ErrorInfoGetBadAccessInformation(runtime_->stack_cache(),
                                               &error_info));
  EXPECT_EQ(HEAP_BUFFER_OVERFLOW, error_info.error_type);
  EXPECT_EQ(kUnknownHeapType, error_info.block_info.heap_type);

  EXPECT_TRUE(fake_block.MarkBlockAsQuarantined());
  error_info.location = fake_block.block_info.body;
  EXPECT_TRUE(ErrorInfoGetBadAccessInformation(runtime_->stack_cache(),
                                               &error_info));
  EXPECT_EQ(USE_AFTER_FREE, error_info.error_type);
  EXPECT_EQ(kUnknownHeapType, error_info.block_info.heap_type);

  error_info.location = fake_block.buffer_align_begin - 1;
  EXPECT_FALSE(ErrorInfoGetBadAccessInformation(runtime_->stack_cache(),
                                                &error_info));
}

TEST_F(AsanErrorInfoTest, GetBadAccessInformationNestedBlock) {
  // Test a nested use after free. We allocate an outer block and an inner block
  // inside it, then we mark the outer block as quarantined and we test a bad
  // access inside the inner block.

  testing::FakeAsanBlock fake_block(kShadowRatioLog, runtime_->stack_cache());
  const size_t kInnerBlockAllocSize = 100;

  // Allocates the outer block.
  BlockLayout outer_block_layout = {};
  EXPECT_TRUE(BlockPlanLayout(kShadowRatio, kShadowRatio, kInnerBlockAllocSize,
                              0, 0, &outer_block_layout));
  EXPECT_TRUE(fake_block.InitializeBlock(outer_block_layout.block_size));

  common::StackCapture stack;
  stack.InitFromStack();

  // Initializes the inner block.
  BlockLayout inner_block_layout = {};
  EXPECT_TRUE(BlockPlanLayout(kShadowRatio,
                              kShadowRatio,
                              kInnerBlockAllocSize,
                              0,
                              0,
                              &inner_block_layout));
  BlockInfo inner_block_info = {};
  BlockInitialize(inner_block_layout, fake_block.block_info.body, true,
      &inner_block_info);
  ASSERT_NE(reinterpret_cast<void*>(NULL), inner_block_info.body);
  Shadow::PoisonAllocatedBlock(inner_block_info);
  inner_block_info.header->alloc_stack =
      runtime_->stack_cache()->SaveStackTrace(stack);
  BlockHeader* inner_header = inner_block_info.header;
  BlockHeader* outer_header = reinterpret_cast<BlockHeader*>(
      fake_block.buffer_align_begin);

  AsanErrorInfo error_info = {};

  // Mark the inner block as quarantined and check that we detect a use after
  // free when trying to access its data.
  inner_block_info.header->free_stack =
      runtime_->stack_cache()->SaveStackTrace(stack);
  EXPECT_NE(reinterpret_cast<void*>(NULL), inner_header->free_stack);
  inner_header->state = QUARANTINED_BLOCK;

  error_info.location = fake_block.block_info.body;
  EXPECT_TRUE(ErrorInfoGetBadAccessInformation(runtime_->stack_cache(),
                                               &error_info));
  EXPECT_EQ(USE_AFTER_FREE, error_info.error_type);
  EXPECT_NE(reinterpret_cast<void*>(NULL), error_info.block_info.free_stack);
  EXPECT_EQ(kUnknownHeapType, error_info.block_info.heap_type);

  EXPECT_EQ(inner_header->free_stack->num_frames(),
            error_info.block_info.free_stack_size);
  for (size_t i = 0; i < inner_header->free_stack->num_frames(); ++i) {
    EXPECT_EQ(inner_header->free_stack->frames()[i],
              error_info.block_info.free_stack[i]);
  }

  // Mark the outer block as quarantined, we should detect a use after free
  // when trying to access the data of the inner block, and the free stack
  // should be the one of the inner block.
  EXPECT_TRUE(fake_block.MarkBlockAsQuarantined());
  EXPECT_NE(ALLOCATED_BLOCK, static_cast<BlockState>(outer_header->state));
  EXPECT_NE(reinterpret_cast<void*>(NULL), outer_header->free_stack);

  // Tests an access in the inner block.
  error_info.location = inner_block_info.body;
  EXPECT_TRUE(ErrorInfoGetBadAccessInformation(runtime_->stack_cache(),
                                               &error_info));
  EXPECT_EQ(USE_AFTER_FREE, error_info.error_type);
  EXPECT_NE(reinterpret_cast<void*>(NULL), error_info.block_info.free_stack);
  EXPECT_EQ(kUnknownHeapType, error_info.block_info.heap_type);

  EXPECT_EQ(inner_header->free_stack->num_frames(),
            error_info.block_info.free_stack_size);
  for (size_t i = 0; i < inner_header->free_stack->num_frames(); ++i) {
    EXPECT_EQ(inner_header->free_stack->frames()[i],
              error_info.block_info.free_stack[i]);
  }
}

TEST_F(AsanErrorInfoTest, ErrorInfoGetBadAccessKind) {
  const size_t kAllocSize = 100;
  testing::FakeAsanBlock fake_block(kShadowRatioLog, runtime_->stack_cache());
  EXPECT_TRUE(fake_block.InitializeBlock(kAllocSize));
  uint8* heap_underflow_address = fake_block.block_info.body - 1;
  uint8* heap_overflow_address = fake_block.block_info.body +
      kAllocSize * sizeof(uint8);
  EXPECT_EQ(HEAP_BUFFER_UNDERFLOW,
            ErrorInfoGetBadAccessKind(heap_underflow_address,
                                      fake_block.block_info.header));
  EXPECT_EQ(HEAP_BUFFER_OVERFLOW,
            ErrorInfoGetBadAccessKind(heap_overflow_address,
                                      fake_block.block_info.header));
  EXPECT_TRUE(fake_block.MarkBlockAsQuarantined());
  EXPECT_EQ(USE_AFTER_FREE, ErrorInfoGetBadAccessKind(
      fake_block.block_info.body, fake_block.block_info.header));
}

TEST_F(AsanErrorInfoTest, ErrorInfoGetAsanBlockInfo) {
  const size_t kAllocSize = 100;
  testing::FakeAsanBlock fake_block(kShadowRatioLog, runtime_->stack_cache());
  EXPECT_TRUE(fake_block.InitializeBlock(kAllocSize));

  AsanBlockInfo asan_block_info = {};
  ErrorInfoGetAsanBlockInfo(fake_block.block_info,
                            runtime_->stack_cache(),
                            &asan_block_info);

  // Test ErrorInfoGetAsanBlockInfo with an allocated block.
  EXPECT_EQ(fake_block.block_info.body_size, asan_block_info.user_size);
  EXPECT_EQ(ALLOCATED_BLOCK, static_cast<BlockState>(asan_block_info.state));
  EXPECT_EQ(fake_block.block_info.header->state,
            static_cast<BlockState>(asan_block_info.state));
  EXPECT_EQ(::GetCurrentThreadId(), asan_block_info.alloc_tid);
  EXPECT_EQ(0, asan_block_info.free_tid);
  EXPECT_EQ(kDataIsClean, asan_block_info.analysis.block_state);
  EXPECT_EQ(fake_block.block_info.header->alloc_stack->num_frames(),
            asan_block_info.alloc_stack_size);
  EXPECT_EQ(0, asan_block_info.free_stack_size);
  EXPECT_EQ(kUnknownHeapType, asan_block_info.heap_type);

  // Now test it with a quarantined block.
  EXPECT_TRUE(fake_block.MarkBlockAsQuarantined());
  ErrorInfoGetAsanBlockInfo(fake_block.block_info,
                            runtime_->stack_cache(),
                            &asan_block_info);
  EXPECT_EQ(QUARANTINED_BLOCK, static_cast<BlockState>(asan_block_info.state));
  EXPECT_EQ(fake_block.block_info.header->state,
            static_cast<BlockState>(asan_block_info.state));
  EXPECT_EQ(::GetCurrentThreadId(), asan_block_info.free_tid);
  EXPECT_EQ(fake_block.block_info.header->free_stack->num_frames(),
            asan_block_info.free_stack_size);
  EXPECT_EQ(kUnknownHeapType, asan_block_info.heap_type);

  // Ensure that the block is correctly tagged as corrupt if the header is
  // invalid.
  fake_block.block_info.header->magic = ~kBlockHeaderMagic;
  ErrorInfoGetAsanBlockInfo(fake_block.block_info,
                            runtime_->stack_cache(),
                            &asan_block_info);
  EXPECT_EQ(kDataIsCorrupt, asan_block_info.analysis.block_state);
  fake_block.block_info.header->magic = kBlockHeaderMagic;
}

TEST_F(AsanErrorInfoTest, GetTimeSinceFree) {
  const size_t kAllocSize = 100;
  const size_t kSleepTime = 25;
  testing::FakeAsanBlock fake_block(kShadowRatioLog, runtime_->stack_cache());
  EXPECT_TRUE(fake_block.InitializeBlock(kAllocSize));

  uint32 ticks_before_free = ::GetTickCount();
  EXPECT_TRUE(fake_block.MarkBlockAsQuarantined());
  ::Sleep(kSleepTime);
  AsanErrorInfo error_info = {};
  error_info.error_type = USE_AFTER_FREE;
  error_info.location = fake_block.block_info.body;
  EXPECT_TRUE(ErrorInfoGetBadAccessInformation(runtime_->stack_cache(),
                                               &error_info));
  EXPECT_NE(0U, error_info.block_info.milliseconds_since_free);

  uint32 ticks_delta = ::GetTickCount() - ticks_before_free;
  EXPECT_GT(ticks_delta, 0U);

  EXPECT_GE(ticks_delta, error_info.block_info.milliseconds_since_free);
}

namespace {

void InitAsanBlockInfo(AsanBlockInfo* block_info) {
  block_info->header = reinterpret_cast<void*>(0xDEADBEEF);
  block_info->user_size = 1024;
  block_info->state = ALLOCATED_BLOCK;
  block_info->alloc_tid = 47;
  block_info->analysis.block_state = kDataIsCorrupt;
  block_info->analysis.header_state = kDataIsCorrupt;
  block_info->analysis.body_state = kDataStateUnknown;
  block_info->analysis.trailer_state = kDataIsClean;
  block_info->alloc_stack[0] = reinterpret_cast<void*>(1);
  block_info->alloc_stack[1] = reinterpret_cast<void*>(2);
  block_info->alloc_stack_size = 2;
  block_info->heap_type = kWinHeap;
}

}  // namespace

TEST_F(AsanErrorInfoTest, PopulateBlockInfo) {
  AsanBlockInfo block_info = {};
  InitAsanBlockInfo(&block_info);

  {
    crashdata::Value info;
    PopulateBlockInfo(block_info, &info);
    std::string json;
    EXPECT_TRUE(crashdata::ToJson(true, &info, &json));
    const char kExpected[] =
        "{\n"
        "  \"header\": 0xDEADBEEF,\n"
        "  \"user-size\": 1024,\n"
        "  \"state\": \"allocated\",\n"
        "  \"heap-type\": \"WinHeap\",\n"
        "  \"analysis\": {\n"
        "    \"block\": \"corrupt\",\n"
        "    \"header\": \"corrupt\",\n"
        "    \"body\": \"(unknown)\",\n"
        "    \"trailer\": \"clean\"\n"
        "  },\n"
        "  \"alloc-thread-id\": 47,\n"
        "  \"alloc-stack\": [\n"
        "    0x00000001, 0x00000002\n"
        "  ]\n"
        "}";
    EXPECT_EQ(kExpected, json);
  }

  block_info.state = QUARANTINED_BLOCK;
  block_info.free_tid = 32;
  block_info.free_stack[0] = reinterpret_cast<void*>(3);
  block_info.free_stack[1] = reinterpret_cast<void*>(4);
  block_info.free_stack[2] = reinterpret_cast<void*>(5);
  block_info.free_stack_size = 3;
  block_info.heap_type = kCtMallocHeap;
  block_info.milliseconds_since_free = 100;

  {
    crashdata::Value info;
    PopulateBlockInfo(block_info, &info);
    std::string json;
    EXPECT_TRUE(crashdata::ToJson(true, &info, &json));
    const char kExpected[] =
        "{\n"
        "  \"header\": 0xDEADBEEF,\n"
        "  \"user-size\": 1024,\n"
        "  \"state\": \"quarantined\",\n"
        "  \"heap-type\": \"CtMallocHeap\",\n"
        "  \"analysis\": {\n"
        "    \"block\": \"corrupt\",\n"
        "    \"header\": \"corrupt\",\n"
        "    \"body\": \"(unknown)\",\n"
        "    \"trailer\": \"clean\"\n"
        "  },\n"
        "  \"alloc-thread-id\": 47,\n"
        "  \"alloc-stack\": [\n"
        "    0x00000001, 0x00000002\n"
        "  ],\n"
        "  \"free-thread-id\": 32,\n"
        "  \"free-stack\": [\n"
        "    0x00000003, 0x00000004, 0x00000005\n"
        "  ],\n"
        "  \"milliseconds-since-free\": 100\n"
        "}";
    EXPECT_EQ(kExpected, json);
  }
}

TEST_F(AsanErrorInfoTest, PopulateCorruptBlockRange) {
  AsanBlockInfo block_info = {};
  InitAsanBlockInfo(&block_info);

  AsanCorruptBlockRange range = {};
  range.address = reinterpret_cast<void*>(0xBAADF00D);
  range.length = 1024 * 1024;
  range.block_count = 100;
  range.block_info_count = 1;
  range.block_info = &block_info;

  crashdata::Value info;
  PopulateCorruptBlockRange(range, &info);

  std::string json;
  EXPECT_TRUE(crashdata::ToJson(true, &info, &json));
  const char kExpected[] =
      "{\n"
      "  \"address\": 0xBAADF00D,\n"
      "  \"length\": 1048576,\n"
      "  \"block-count\": 100,\n"
      "  \"blocks\": [\n"
      "    {\n"
      "      \"header\": 0xDEADBEEF,\n"
      "      \"user-size\": 1024,\n"
      "      \"state\": \"allocated\",\n"
      "      \"heap-type\": \"WinHeap\",\n"
      "      \"analysis\": {\n"
      "        \"block\": \"corrupt\",\n"
      "        \"header\": \"corrupt\",\n"
      "        \"body\": \"(unknown)\",\n"
      "        \"trailer\": \"clean\"\n"
      "      },\n"
      "      \"alloc-thread-id\": 47,\n"
      "      \"alloc-stack\": [\n"
      "        0x00000001, 0x00000002\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  EXPECT_EQ(kExpected, json);
}

TEST_F(AsanErrorInfoTest, PopulateErrorInfo) {
  AsanBlockInfo block_info = {};
  InitAsanBlockInfo(&block_info);

  AsanCorruptBlockRange range = {};
  range.address = reinterpret_cast<void*>(0xBAADF00D);
  range.length = 1024 * 1024;
  range.block_count = 100;
  range.block_info_count = 1;
  range.block_info = &block_info;

  // The 'location' address needs to be at a consistent place in system memory
  // so that shadow memory contents and page bits don't vary, otherwise the
  // test won't be deterministic.
  AsanErrorInfo error_info = {};
  error_info.location = reinterpret_cast<void*>(0x00001000);
  error_info.crash_stack_id = 1234;
  InitAsanBlockInfo(&error_info.block_info);
  error_info.error_type = WILD_ACCESS;
  error_info.access_mode = ASAN_READ_ACCESS;
  error_info.access_size = 4;
  ::strncpy(error_info.shadow_info,
            "shadow info!",
            sizeof(error_info.shadow_info));
  ::strncpy(error_info.shadow_memory,
            "shadow memory!",
            sizeof(error_info.shadow_memory));
  error_info.heap_is_corrupt = true;
  error_info.corrupt_range_count = 10;
  error_info.corrupt_block_count = 200;
  error_info.corrupt_ranges_reported = 1;
  error_info.corrupt_ranges = &range;

  crashdata::Value info;
  PopulateErrorInfo(error_info, &info);

  std::string json;
  EXPECT_TRUE(crashdata::ToJson(true, &info, &json));
  const char kExpected[] =
      "{\n"
      "  \"location\": 0x00001000,\n"
      "  \"crash-stack-id\": 1234,\n"
      "  \"block-info\": {\n"
      "    \"header\": 0xDEADBEEF,\n"
      "    \"user-size\": 1024,\n"
      "    \"state\": \"allocated\",\n"
      "    \"heap-type\": \"WinHeap\",\n"
      "    \"analysis\": {\n"
      "      \"block\": \"corrupt\",\n"
      "      \"header\": \"corrupt\",\n"
      "      \"body\": \"(unknown)\",\n"
      "      \"trailer\": \"clean\"\n"
      "    },\n"
      "    \"alloc-thread-id\": 47,\n"
      "    \"alloc-stack\": [\n"
      "      0x00000001, 0x00000002\n"
      "    ]\n"
      "  },\n"
      "  \"error-type\": \"wild-access\",\n"
      "  \"access-mode\": \"read\",\n"
      "  \"access-size\": 4,\n"
      "  \"shadow-memory-index\": 512,\n"
      "  \"shadow-memory\": {\n"
      "    \"type\": \"blob\",\n"
      "    \"address\": null,\n"
      "    \"size\": null,\n"
      "    \"data\": [\n"
      "      0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2,\n"
      "      0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2,\n"
      "      0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2,\n"
      "      0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2,\n"
      "      0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2,\n"
      "      0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2,\n"
      "      0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2,\n"
      "      0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2\n"
      "    ]\n"
      "  },\n"
      "  \"page-bits-index\": 0,\n"
      "  \"page-bits\": {\n"
      "    \"type\": \"blob\",\n"
      "    \"address\": null,\n"
      "    \"size\": null,\n"
      "    \"data\": [\n"
      "      0x00, 0x00, 0x00\n"
      "    ]\n"
      "  },\n"
      "  \"heap-is-corrupt\": 1,\n"
      "  \"corrupt-range-count\": 10,\n"
      "  \"corrupt-block-count\": 200,\n"
      "  \"corrupt-ranges\": [\n"
      "    {\n"
      "      \"address\": 0xBAADF00D,\n"
      "      \"length\": 1048576,\n"
      "      \"block-count\": 100,\n"
      "      \"blocks\": [\n"
      "        {\n"
      "          \"header\": 0xDEADBEEF,\n"
      "          \"user-size\": 1024,\n"
      "          \"state\": \"allocated\",\n"
      "          \"heap-type\": \"WinHeap\",\n"
      "          \"analysis\": {\n"
      "            \"block\": \"corrupt\",\n"
      "            \"header\": \"corrupt\",\n"
      "            \"body\": \"(unknown)\",\n"
      "            \"trailer\": \"clean\"\n"
      "          },\n"
      "          \"alloc-thread-id\": 47,\n"
      "          \"alloc-stack\": [\n"
      "            0x00000001, 0x00000002\n"
      "          ]\n"
      "        }\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  EXPECT_EQ(kExpected, json);
}

}  // namespace asan
}  // namespace agent
