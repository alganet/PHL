--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
debug_backtrace() returns a list of frames, each at its call site
--FILE--
<?php
function btInner($x, $y) {
    foreach (debug_backtrace() as $i => $f) {
        printf("#%d %s%s%s(%d args) at line %d, file %s\n", $i,
            $f['class'] ?? '', $f['type'] ?? '', $f['function'],
            count($f['args'] ?? []), $f['line'],
            basename($f['file']));
    }
}
function btOuter($z) {
    btInner($z, "two");
}
btOuter(1);
?>
--EXPECTF--
#0 btInner(2 args) at line 11, file %s
#1 btOuter(1 args) at line 13, file %s
--CLEAN--
<?php
