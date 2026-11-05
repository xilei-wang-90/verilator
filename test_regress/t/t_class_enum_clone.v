// DESCRIPTION: Verilator: Test enum methods in parameterized class clones
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

package p;
  class C #(int N = 0);
    typedef enum logic {A, B} E;
    static function string f(input logic sel);
      E e = E'(sel);
      return e.name();
    endfunction
  endclass
endpackage

module t;
  logic sel;
  initial begin
    sel = 0;
    if (p::C#(1)::f(sel) != "A") $stop;
    if (p::C#(2)::f(sel) != "A") $stop;
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
