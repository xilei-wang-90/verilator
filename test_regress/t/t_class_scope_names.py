#!/usr/bin/env python3
# DESCRIPTION: Verilator: Test class and ordinary scope names with similar prefixes
# SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

import vltest_bootstrap

test.scenarios('vlt')
test.compile(verilator_flags2=["--hierarchical"])
for scope in ("std___05Fcounter", "uvm_pkg___05Fcounter"):
    test.file_grep_not(test.obj_dir + "/" + test.vm_prefix + "__Syms__Vlib.h", scope)
    test.file_grep(test.obj_dir + "/" + test.vm_prefix + "__Syms.h", scope)
test.execute()
test.passes()
