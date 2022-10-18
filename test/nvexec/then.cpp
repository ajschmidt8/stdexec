#include <catch2/catch.hpp>
#include <stdexec/execution.hpp>

#include "nvexec/stream_context.cuh"
#include "common.cuh"

namespace ex = std::execution;

using nvexec::is_on_gpu;

TEST_CASE("then returns a sender", "[cuda][stream][adaptors][then]") {
  nvexec::stream_context stream_ctx{};
  auto snd = ex::then(ex::schedule(stream_ctx.get_scheduler()), [] {});
  STATIC_REQUIRE(ex::sender<decltype(snd)>);
  (void)snd;
}

TEST_CASE("then executes on GPU", "[cuda][stream][adaptors][then]") {
  nvexec::stream_context stream_ctx{};

  flags_storage_t flags_storage{};
  auto flags = flags_storage.get();

  auto snd = ex::schedule(stream_ctx.get_scheduler()) //
           | ex::then([=] {
               if (is_on_gpu()) {
                 flags.set();
               }
             });
  std::this_thread::sync_wait(std::move(snd));

  REQUIRE(flags_storage.all_set_once());
}

TEST_CASE("then accepts values on GPU", "[cuda][stream][adaptors][then]") {
  nvexec::stream_context stream_ctx{};

  flags_storage_t flags_storage{};
  auto flags = flags_storage.get();

  auto snd = ex::transfer_just(stream_ctx.get_scheduler(), 42) //
           | ex::then([=](int val) {
               if (is_on_gpu()) {
                 if (val == 42) {
                   flags.set();
                 }
               }
             });
  std::this_thread::sync_wait(std::move(snd));

  REQUIRE(flags_storage.all_set_once());
}

TEST_CASE("then accepts multiple values on GPU", "[cuda][stream][adaptors][then]") {
  nvexec::stream_context stream_ctx{};

  flags_storage_t flags_storage{};
  auto flags = flags_storage.get();

  auto snd = ex::transfer_just(stream_ctx.get_scheduler(), 42, 4.2) //
           | ex::then([=](int i, double d) {
               if (is_on_gpu()) {
                 if (i == 42 && d == 4.2) {
                   flags.set();
                 }
               }
             });
  std::this_thread::sync_wait(std::move(snd));

  REQUIRE(flags_storage.all_set_once());
}

TEST_CASE("then returns values on GPU", "[cuda][stream][adaptors][then]") {
  nvexec::stream_context stream_ctx{};

  auto snd = ex::schedule(stream_ctx.get_scheduler()) //
           | ex::then([=]() -> int {
               if (is_on_gpu()) {
                 return 42;
               }

               return 0;
             });
  const auto [result] = std::this_thread::sync_wait(std::move(snd)).value();

  REQUIRE(result == 42);
}

TEST_CASE("then can preceed a sender without values", "[cuda][stream][adaptors][then]") {
  nvexec::stream_context stream_ctx{};

  flags_storage_t<2> flags_storage{};
  auto flags = flags_storage.get();

  auto snd = ex::schedule(stream_ctx.get_scheduler()) //
           | ex::then([flags] {
               if (is_on_gpu()) {
                 flags.set(0);
               }
             })
           | a_sender([flags] {
               if (is_on_gpu()) {
                 flags.set(1);
               }
             });
  std::this_thread::sync_wait(std::move(snd));

  REQUIRE(flags_storage.all_set_once());
}

TEST_CASE("then can succeed a sender", "[cuda][stream][adaptors][then]") {
  SECTION("without values") {
    nvexec::stream_context stream_ctx{};
    flags_storage_t<2> flags_storage{};
    auto flags = flags_storage.get();

    auto snd = ex::schedule(stream_ctx.get_scheduler()) //
             | a_sender([flags] {
                 if (is_on_gpu()) {
                   flags.set(1);
                 }
               })
             | ex::then([flags] {
                 if (is_on_gpu()) {
                   flags.set(0);
                 }
               });
    std::this_thread::sync_wait(std::move(snd));

    REQUIRE(flags_storage.all_set_once());
  }

  SECTION("with values") {
    nvexec::stream_context stream_ctx{};
    flags_storage_t flags_storage{};
    auto flags = flags_storage.get();

    auto snd = ex::schedule(stream_ctx.get_scheduler()) //
             | a_sender([]() -> bool {
                 return is_on_gpu();
               })
             | ex::then([flags](bool a_sender_was_on_gpu) {
                 if (a_sender_was_on_gpu * is_on_gpu()) {
                   flags.set();
                 }
               });
    std::this_thread::sync_wait(std::move(snd)).value();

    REQUIRE(flags_storage.all_set_once());
  }
}

/*
TEST_CASE("then can return values of non-trivial types", "[cuda][stream][adaptors][then]") {
  nvexec::stream_context stream_ctx{};

  auto snd = ex::schedule(stream_ctx.get_scheduler()) //
           | ex::then([]() -> move_only_t {
               return move_only_t{42};
             });
  auto [v] = std::this_thread::sync_wait(std::move(snd)).value();

  REQUIRE(v == move_only_t{42});
}
*/

