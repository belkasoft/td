//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2024
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "td/telegram/net/NetQueryVerifier.h"

#include "td/telegram/Global.h"
#include "td/telegram/net/NetQueryDispatcher.h"
#include "td/telegram/Td.h"

#include "td/utils/common.h"
#include "td/utils/Slice.h"
#include "td/utils/Storer.h"
#include "td/utils/utf8.h"

namespace td {

void NetQueryVerifier::verify(NetQueryPtr query, string nonce) {
  CHECK(query->is_ready());
  CHECK(query->is_error());

  if (!check_utf8(nonce)) {
    query->set_error(Status::Error(400, "Invalid verification nonce"));
    G()->net_query_dispatcher().dispatch(std::move(query));
    return;
  }

  auto query_id = next_query_id_++;
  queries_.emplace(query_id, std::make_pair(std::move(query), nonce));

  send_closure(G()->td(), &Td::send_update,
               td_api::make_object<td_api::updateApplicationVerificationRequired>(query_id, nonce));
}

void NetQueryVerifier::set_verification_token(int64 query_id, string &&token, Promise<Unit> &&promise) {
  auto it = queries_.find(query_id);
  if (it == queries_.end()) {
    return promise.set_error(Status::Error(400, "Verification not found"));
  }
  auto query = std::move(it->second.first);
  auto nonce = std::move(it->second.second);
  queries_.erase(it);
  promise.set_value(Unit());

  if (token.empty()) {
    query->set_error(Status::Error(400, "VERIFICATION_FAILED"));
  } else {
#if TD_ANDROID
    telegram_api::invokeWithGooglePlayIntegrityPrefix prefix(nonce, token);
#else
    telegram_api::invokeWithApnsSecretPrefix prefix(nonce, token);
#endif
    auto storer = DefaultStorer<telegram_api::Function>(prefix);
    string prefix_str(storer.size(), '\0');
    auto real_size = storer.store(MutableSlice(prefix_str).ubegin());
    CHECK(real_size == prefix_str.size());
    query->add_verification_prefix(prefix_str);
  }
  query->resend();
  G()->net_query_dispatcher().dispatch(std::move(query));
}

void NetQueryVerifier::tear_down() {
  for (auto &it : queries_) {
    it.second.first->set_error(Global::request_aborted_error());
    G()->net_query_dispatcher().dispatch(std::move(it.second.first));
  }
  queries_.clear();
  parent_.reset();
}

}  // namespace td
