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
#include "vm/cells/MerkleProof.h"
#include "vm/cells/CellBuilder.h"
#include "vm/cells/CellSlice.h"
#include "vm/boc.h"

#include "td/utils/HashMap.h"
#include "td/utils/HashSet.h"

namespace vm {
namespace detail {
class MerkleProofImpl {
 public:
  explicit MerkleProofImpl(MerkleProof::IsPrunnedFunction is_prunned) : is_prunned_(std::move(is_prunned)) {
  }
  explicit MerkleProofImpl(CellUsageTree *usage_tree) : usage_tree_(usage_tree) {
  }

  Ref<Cell> create_from(Ref<Cell> cell) {
    if (!is_prunned_) {
      CHECK(usage_tree_);
      dfs_usage_tree(cell, usage_tree_->root_id());
      is_prunned_ = [this](const Ref<Cell> &cell) { return visited_cells_.count(cell->get_hash()) == 0; };
    }
    return dfs(cell, cell->get_level());
  }

 private:
  using Key = std::pair<Cell::Hash, int>;
  td::HashMap<Key, Ref<Cell>> cells_;
  td::HashSet<Cell::Hash> visited_cells_;
  CellUsageTree *usage_tree_{nullptr};
  MerkleProof::IsPrunnedFunction is_prunned_;

  void dfs_usage_tree(Ref<Cell> cell, CellUsageTree::NodeId node_id) {
    if (!usage_tree_->is_loaded(node_id)) {
      return;
    }
    visited_cells_.insert(cell->get_hash());
    CellSlice cs(NoVm(), cell);
    for (unsigned i = 0; i < cs.size_refs(); i++) {
      dfs_usage_tree(cs.prefetch_ref(i), usage_tree_->get_child(node_id, i));
    }
  }

  Ref<Cell> dfs(Ref<Cell> cell, int merkle_depth) {
    CHECK(cell.not_null());
    Key key{cell->get_hash(), merkle_depth};
    {
      auto it = cells_.find(key);
      if (it != cells_.end()) {
        CHECK(it->second.not_null());
        return it->second;
      }
    }

    if (is_prunned_(cell)) {
      auto res = CellBuilder::create_pruned_branch(cell, merkle_depth + 1);
      CHECK(res.not_null());
      cells_.emplace(key, res);
      return res;
    }
    CellSlice cs(NoVm(), cell);
    int children_merkle_depth = cs.child_merkle_depth(merkle_depth);
    CellBuilder cb;
    cb.store_bits(cs.fetch_bits(cs.size()));
    for (unsigned i = 0; i < cs.size_refs(); i++) {
      cb.store_ref(dfs(cs.prefetch_ref(i), children_merkle_depth));
    }
    auto res = cb.finalize(cs.is_special());
    CHECK(res.not_null());
    cells_.emplace(key, res);
    return res;
  }
};
}  // namespace detail

Ref<Cell> MerkleProof::generate_raw(Ref<Cell> cell, IsPrunnedFunction is_prunned) {
  return detail::MerkleProofImpl(is_prunned).create_from(cell);
}

Ref<Cell> MerkleProof::generate_raw(Ref<Cell> cell, CellUsageTree *usage_tree) {
  return detail::MerkleProofImpl(usage_tree).create_from(cell);
}

Ref<Cell> MerkleProof::virtualize_raw(Ref<Cell> cell, Cell::VirtualizationParameters virt) {
  return cell->virtualize(virt);
}

Ref<Cell> MerkleProof::generate(Ref<Cell> cell, IsPrunnedFunction is_prunned) {
  int cell_level = cell->get_level();
  if (cell_level != 0) {
    return {};
  }
  auto raw = generate_raw(std::move(cell), is_prunned);
  return CellBuilder::create_merkle_proof(std::move(raw));
}

Ref<Cell> MerkleProof::generate(Ref<Cell> cell, CellUsageTree *usage_tree) {
  int cell_level = cell->get_level();
  if (cell_level != 0) {
    return {};
  }
  auto raw = generate_raw(std::move(cell), usage_tree);
  return CellBuilder::create_merkle_proof(std::move(raw));
}

td::Result<Ref<Cell>> unpack_proof(Ref<Cell> cell) {
  CHECK(cell.not_null());
  td::uint8 level = static_cast<td::uint8>(cell->get_level());
  if (level != 0) {
    return td::Status::Error("Level of MerkleProof must be zero");
  }
  CellSlice cs(NoVm(), std::move(cell));
  if (cs.special_type() != Cell::SpecialType::MerkleProof) {
    return td::Status::Error("Not a MekleProof cell");
  }
  return cs.fetch_ref();
}

Ref<Cell> MerkleProof::virtualize(Ref<Cell> cell, int virtualization) {
  auto r_raw = unpack_proof(std::move(cell));
  if (r_raw.is_error()) {
    return {};
  }
  return virtualize_raw(r_raw.move_as_ok(), {0 /*level*/, static_cast<td::uint8>(virtualization)});
}

class MerkleProofCombine {
 public:
  MerkleProofCombine(Ref<Cell> a, Ref<Cell> b) : a_(std::move(a)), b_(std::move(b)) {
  }
  td::Result<Ref<Cell>> run() {
    TRY_RESULT(a, unpack_proof(a_));
    TRY_RESULT(b, unpack_proof(b_));
    if (a->get_hash(0) != b->get_hash(0)) {
      return td::Status::Error("Can't combine MerkleProofs with different roots");
    }
    dfs(a, 0);
    dfs(b, 0);
    return CellBuilder::create_merkle_proof(create_A(a, 0, 0));
  }

 private:
  Ref<Cell> a_;
  Ref<Cell> b_;

  struct Info {
    Ref<Cell> cell_;
    Ref<Cell> prunned_cells_[Cell::max_level];  // Cache prunned cells with different levels to reuse them

    Ref<Cell> get_prunned_cell(int depth) {
      if (depth < Cell::max_level) {
        return prunned_cells_[depth];
      }
      return {};
    }
    Ref<Cell> get_any_cell() const {
      if (cell_.not_null()) {
        return cell_;
      }
      for (auto &cell : prunned_cells_) {
        if (cell.not_null()) {
          return cell;
        }
      }
      UNREACHABLE();
    }
  };

  using Key = std::pair<Cell::Hash, int>;
  td::HashMap<Cell::Hash, Info> cells_;
  td::HashMap<Key, Ref<Cell>> create_A_res_;
  td::HashSet<Key> visited_;

  void dfs(Ref<Cell> cell, int merkle_depth) {
    if (!visited_.emplace(cell->get_hash(), merkle_depth).second) {
      return;
    }

    auto &info = cells_[cell->get_hash(merkle_depth)];
    CellSlice cs(NoVm(), cell);
    // check if prunned cell is bounded
    if (cs.special_type() == Cell::SpecialType::PrunnedBranch && static_cast<int>(cell->get_level()) > merkle_depth) {
      info.prunned_cells_[cell->get_level() - 1] = std::move(cell);
      return;
    }
    info.cell_ = std::move(cell);

    auto child_merkle_depth = cs.child_merkle_depth(merkle_depth);
    for (size_t i = 0, size = cs.size_refs(); i < size; i++) {
      dfs(cs.fetch_ref(), child_merkle_depth);
    }
  }

  Ref<Cell> create_A(Ref<Cell> cell, int merkle_depth, int a_merkle_depth) {
    merkle_depth = cell->get_level_mask().apply(merkle_depth).get_level();
    auto key = Key(cell->get_hash(merkle_depth), a_merkle_depth);
    auto it = create_A_res_.find(key);
    if (it != create_A_res_.end()) {
      return it->second;
    }

    auto res = do_create_A(std::move(cell), merkle_depth, a_merkle_depth);
    create_A_res_.emplace(key, res);
    return res;
  }

  Ref<Cell> do_create_A(Ref<Cell> cell, int merkle_depth, int a_merkle_depth) {
    auto &info = cells_[cell->get_hash(merkle_depth)];

    if (info.cell_.is_null()) {
      Ref<Cell> res = info.get_prunned_cell(a_merkle_depth);
      if (res.is_null()) {
        res = CellBuilder::create_pruned_branch(info.get_any_cell(), a_merkle_depth + 1, merkle_depth);
      }
      return res;
    }

    CHECK(info.cell_.not_null());
    CellSlice cs(NoVm(), info.cell_);

    //CHECK(cs.size_refs() != 0);
    if (cs.size_refs() == 0) {
      return info.cell_;
    }

    auto child_merkle_depth = cs.child_merkle_depth(merkle_depth);
    auto child_a_merkle_depth = cs.child_merkle_depth(a_merkle_depth);

    CellBuilder cb;
    cb.store_bits(cs.fetch_bits(cs.size()));
    for (unsigned i = 0; i < cs.size_refs(); i++) {
      cb.store_ref(create_A(cs.prefetch_ref(i), child_merkle_depth, child_a_merkle_depth));
    }
    return cb.finalize(cs.is_special());
  }
};

Ref<Cell> MerkleProof::combine(Ref<Cell> a, Ref<Cell> b) {
  auto res = MerkleProofCombine(std::move(a), std::move(b)).run();
  if (res.is_error()) {
    return {};
  }
  return res.move_as_ok();
}

MerkleProofBuilder::MerkleProofBuilder(Ref<Cell> root)
    : usage_tree(std::make_shared<CellUsageTree>()), orig_root(std::move(root)) {
  usage_root = UsageCell::create(orig_root, usage_tree->root_ptr());
}

Ref<Cell> MerkleProofBuilder::init(Ref<Cell> root) {
  usage_tree = std::make_shared<CellUsageTree>();
  orig_root = std::move(root);
  usage_root = UsageCell::create(orig_root, usage_tree->root_ptr());
  return usage_root;
}

bool MerkleProofBuilder::clear() {
  usage_tree.reset();
  orig_root.clear();
  usage_root.clear();
  return true;
}

Ref<Cell> MerkleProofBuilder::extract_proof() const {
  return MerkleProof::generate(orig_root, usage_tree.get());
}

bool MerkleProofBuilder::extract_proof_to(Ref<Cell> &proof_root) const {
  return orig_root.not_null() && (proof_root = extract_proof()).not_null();
}

td::Result<td::BufferSlice> MerkleProofBuilder::extract_proof_boc() const {
  Ref<Cell> proof_root = extract_proof();
  if (proof_root.is_null()) {
    return td::Status::Error("cannot create Merkle proof");
  } else {
    return std_boc_serialize(std::move(proof_root));
  }
}

}  // namespace vm
