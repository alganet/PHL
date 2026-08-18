--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_erase() on the live map of a by-ref foreach ends the loop cleanly (regression: dangling private cursor read freed nodes); a fresh insert resumes it
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip array_erase is a PHL extension (documented PH7-ism)";
}
?>
--FILE--
<?php
// The CowSeparate by-ref discount keeps the loop's map writable, so
// array_erase() guts the very map the cursor walks. The Release-time
// cursor parking must end the loop instead of reading freed nodes.
$a = [1, 2, 3];
foreach ($a as &$v) {
    echo $v, ",";
    array_erase($a);
}
unset($v);
echo "end,", count($a), "\n";

// Erase then insert inside the body: the link-time re-arm resumes the loop
// on the fresh element (live-array semantics), bounded here by the counter.
$b = [1, 2, 3];
$n = 0;
foreach ($b as &$w) {
    echo $w, ",";
    array_erase($b);
    if ($n++ < 2) {
        $b[] = 99;
    }
}
unset($w);
echo "end,", count($b), "\n";
?>
--EXPECT--
1,end,0
1,99,99,end,0
--CLEAN--
<?php
unset($a, $b, $n);
