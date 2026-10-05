--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An undefined variable passed to a call that never ran does not haunt the next value in its slot
--FILE--
<?php
/* An argument list can end without its call: a later argument throws, or a named
 * argument is refused where it is sent. An undefined plain variable sent before
 * that point must leave nothing behind in its stack slot for the next value pushed
 * there by the same body -- an array literal handed to a constructor or to a
 * function, which read the dead variable's name through the array and crashed.
 * The branch test is a lone `$i`, so nothing else touches the slot in between. */
set_error_handler(function ($no, $msg) { echo "  warning: $msg\n"; return true; });
function t() { throw new Exception("thrown"); }
function byRef(&$a, $b) {}
function byVal($a) {}
function arr(array $a) { return count($a); }
$shapes = [
    'later argument throws, then new' => function ($i) {
        if ($i) { $d = new ArrayObject([]); echo "  new: ", count($d), "\n"; } else { byRef($u, t()); }
    },
    'later argument throws, then call' => function ($i) {
        if ($i) { echo "  call: ", arr([]), "\n"; } else { byRef($u, t()); }
    },
    'name refused at its send, then new' => function ($i) {
        if ($i) { $d = new ArrayObject([]); echo "  new: ", count($d), "\n"; } else { byVal(zz: $u, a: 1); }
    },
    'name refused at its send, then call' => function ($i) {
        if ($i) { echo "  call: ", arr([]), "\n"; } else { byVal(zz: $u, a: 1); }
    },
];
foreach ($shapes as $label => $body) {
    echo "$label\n";
    for ($i = 0; $i < 2; $i++) {
        try {
            $body($i);
        } catch (Throwable $e) {
            echo "  ", get_class($e), ": ", $e->getMessage(), "\n";
        }
    }
}
--EXPECT--
later argument throws, then new
  Exception: thrown
  new: 0
later argument throws, then call
  Exception: thrown
  call: 0
name refused at its send, then new
  Error: Unknown named parameter $zz
  new: 0
name refused at its send, then call
  Error: Unknown named parameter $zz
  call: 0
