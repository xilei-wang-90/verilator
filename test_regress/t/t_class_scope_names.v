// DESCRIPTION: Verilator: Test class and ordinary scope names with similar prefixes
// SPDX-License-Identifier: CC0-1.0

module t;
  std_counter std_counter();
  uvm_pkg_counter uvm_pkg_counter();
  std__counter std__counter();
  uvm_pkg__counter uvm_pkg__counter();
  uvm_pkg::static_reader reader;
  initial begin
    reader = new();
    if (std_counter.get() != 1) $stop;
    if (uvm_pkg_counter.get() != 2) $stop;
    if (std__counter.get(5) != 5) $stop;
    if (uvm_pkg__counter.get(6) != 6) $stop;
    if (package_value::get() != 3) $stop;
    if (reader.get() != 3) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule

class package_value;
  static int value /* verilator public_flat */ = 3;
  static function int get();
    return value;
  endfunction
endclass

package uvm_pkg;
  class static_reader;
    function int get();
      // verilator no_inline_task
      return package_value::value;
    endfunction
  endclass
endpackage

module uvm_pkg_counter;
  static int value /* verilator public_flat */ = 2;
  function int get();
    // verilator no_inline_task
    return 2;
  endfunction
endmodule

module std_counter;
  static int value /* verilator public_flat */ = 1;
  function int get();
    // verilator no_inline_task
    return 1;
  endfunction
endmodule

module std__counter;
  static int state /* verilator public_flat */ = 0;
  function int get(int value);
    // verilator no_inline_task
    return value;
  endfunction
endmodule

module uvm_pkg__counter;
  static int state /* verilator public_flat */ = 0;
  function int get(int value);
    // verilator no_inline_task
    return value;
  endfunction
endmodule
