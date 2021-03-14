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

namespace vm {

enum class Excno : int {
  none = 0,
  alt = 1,
  stk_und = 2,
  stk_ov = 3,
  int_ov = 4,
  range_chk = 5,
  inv_opcode = 6,
  type_chk = 7,
  cell_ov = 8,
  cell_und = 9,
  dict_err = 10,
  unknown = 11,
  fatal = 12,
  out_of_gas = 13,
  virt_err = 14,
  total
};

const char* get_exception_msg(Excno exc_no);

class VmError {
  Excno exc_no;
  const char* msg;
  long long arg;

 public:
  VmError(Excno _excno, const char* _msg) : exc_no(_excno), msg(_msg), arg(0) {
  }
  VmError(Excno _excno) : exc_no(_excno), msg(0), arg(0) {
  }
  VmError(Excno _excno, const char* _msg, long long _arg) : exc_no(_excno), msg(_msg), arg(_arg) {
  }
  int get_errno() const {
    return static_cast<int>(exc_no);
  }
  const char* get_msg() const {
    return msg ? msg : get_exception_msg(exc_no);
  }
  long long get_arg() const {
    return arg;
  }
};

struct VmNoGas {
  VmNoGas() = default;
  int get_errno() const {
    return static_cast<int>(Excno::out_of_gas);
  }
  const char* get_msg() const {
    return "out of gas";
  }
  operator VmError() const {
    return VmError{Excno::out_of_gas, "out of gas"};
  }
};

struct VmVirtError {
  int virtualization{0};
  VmVirtError() = default;
  VmVirtError(int virtualization) : virtualization(virtualization) {
  }
  int get_errno() const {
    return static_cast<int>(Excno::virt_err);
  }
  const char* get_msg() const {
    return "prunned branch";
  }
  operator VmError() const {
    return VmError{Excno::virt_err, "prunned branch", virtualization};
  }
};

struct VmFatal {};

}  // namespace vm
