// DESCRIPTION: Verilator: Test mutually dependent class static methods
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

class P;
  static function int f(input int value);
    if (value == 0) return 0;
    return Q::g(value - 1);
  endfunction
endclass

class Q;
  static function int g(input int value);
    return P::f(value);
  endfunction
endclass

module t;
  initial begin
    if (P::f(2) != 0) $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
