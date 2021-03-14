/*
    This file is part of TON Blockchain Library.

    TON Blockchain Library is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    TON Blockchain Library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with TON Blockchain Library.  If not, see <http://www.gnu.org/licenses/>.

    Copyright 2017-2019 Telegram Systems LLP
*/
#pragma once

#include "validator/validator.h"

namespace ton {

namespace validator {

struct ValidatorManagerOptionsImpl : public ValidatorManagerOptions {
 public:
  BlockIdExt zero_block_id() const override {
    return zero_block_id_;
  }
  BlockIdExt init_block_id() const override {
    return init_block_id_;
  }
  bool need_monitor(ShardIdFull shard) const override {
    return check_shard_(shard, ShardCheckMode::m_monitor);
  }
  bool need_validate(ShardIdFull shard) const override {
    return check_shard_(shard, ShardCheckMode::m_validate);
  }
  bool allow_blockchain_init() const override {
    return allow_blockchain_init_;
  }
  td::ClocksBase::Duration sync_blocks_before() const override {
    return sync_blocks_before_;
  }
  td::ClocksBase::Duration block_ttl() const override {
    return block_ttl_;
  }
  td::ClocksBase::Duration state_ttl() const override {
    return state_ttl_;
  }
  td::ClocksBase::Duration archive_ttl() const override {
    return archive_ttl_;
  }
  td::ClocksBase::Duration key_proof_ttl() const override {
    return key_proof_ttl_;
  }
  bool initial_sync_disabled() const override {
    return initial_sync_disabled_;
  }
  bool is_hardfork(BlockIdExt block_id) const override {
    if (!block_id.is_valid()) {
      return false;
    }
    for (size_t i = 0; i < hardforks_.size(); i++) {
      if (block_id == hardforks_[i]) {
        return (i == hardforks_.size() - 1) || block_id.seqno() < hardforks_[i + 1].seqno();
      }
    }
    return false;
  }
  td::uint32 get_vertical_seqno(BlockSeqno seqno) const override {
    size_t best = 0;
    for (size_t i = 0; i < hardforks_.size(); i++) {
      if (seqno >= hardforks_[i].seqno()) {
        best = i + 1;
      }
    }
    return static_cast<td::uint32>(best);
  }
  td::uint32 get_maximal_vertical_seqno() const override {
    return td::narrow_cast<td::uint32>(hardforks_.size());
  }
  td::uint32 get_last_fork_masterchain_seqno() const override {
    return hardforks_.size() ? hardforks_.rbegin()->seqno() : 0;
  }
  std::vector<BlockIdExt> get_hardforks() const override {
    return hardforks_;
  }
  td::uint32 get_filedb_depth() const override {
    return db_depth_;
  }

  void set_zero_block_id(BlockIdExt block_id) override {
    zero_block_id_ = block_id;
  }
  void set_init_block_id(BlockIdExt block_id) override {
    init_block_id_ = block_id;
  }
  void set_shard_check_function(std::function<bool(ShardIdFull, ShardCheckMode)> check_shard) override {
    check_shard_ = std::move(check_shard);
  }
  void set_allow_blockchain_init(bool value) override {
    allow_blockchain_init_ = value;
  }
  void set_sync_blocks_before(td::ClocksBase::Duration value) override {
    sync_blocks_before_ = value;
  }
  void set_block_ttl(td::ClocksBase::Duration value) override {
    block_ttl_ = value;
  }
  void set_state_ttl(td::ClocksBase::Duration value) override {
    state_ttl_ = value;
  }
  void set_archive_ttl(td::ClocksBase::Duration value) override {
    archive_ttl_ = value;
  }
  void set_key_proof_ttl(td::ClocksBase::Duration value) override {
    key_proof_ttl_ = value;
  }
  void set_initial_sync_disabled(bool value) override {
    initial_sync_disabled_ = value;
  }
  void set_hardforks(std::vector<BlockIdExt> vec) override {
    hardforks_ = std::move(vec);
  }
  void set_filedb_depth(td::uint32 value) override {
    CHECK(value <= 32);
    db_depth_ = value;
  }

  ValidatorManagerOptionsImpl *make_copy() const override {
    return new ValidatorManagerOptionsImpl(*this);
  }

  ValidatorManagerOptionsImpl(BlockIdExt zero_block_id, BlockIdExt init_block_id,
                              std::function<bool(ShardIdFull, ShardCheckMode)> check_shard, bool allow_blockchain_init,
                              td::ClocksBase::Duration sync_blocks_before, td::ClocksBase::Duration block_ttl,
                              td::ClocksBase::Duration state_ttl, td::ClocksBase::Duration archive_ttl,
                              td::ClocksBase::Duration key_proof_ttl, bool initial_sync_disabled)
      : zero_block_id_(zero_block_id)
      , init_block_id_(init_block_id)
      , check_shard_(std::move(check_shard))
      , allow_blockchain_init_(allow_blockchain_init)
      , sync_blocks_before_(sync_blocks_before)
      , block_ttl_(block_ttl)
      , state_ttl_(state_ttl)
      , archive_ttl_(archive_ttl)
      , key_proof_ttl_(key_proof_ttl)
      , initial_sync_disabled_(initial_sync_disabled) {
  }

 private:
  BlockIdExt zero_block_id_;
  BlockIdExt init_block_id_;
  std::function<bool(ShardIdFull, ShardCheckMode)> check_shard_;
  bool allow_blockchain_init_;
  td::ClocksBase::Duration sync_blocks_before_;
  td::ClocksBase::Duration block_ttl_;
  td::ClocksBase::Duration state_ttl_;
  td::ClocksBase::Duration archive_ttl_;
  td::ClocksBase::Duration key_proof_ttl_;
  bool initial_sync_disabled_;
  std::vector<BlockIdExt> hardforks_;
  td::uint32 db_depth_ = 2;
};

}  // namespace validator

}  // namespace ton
