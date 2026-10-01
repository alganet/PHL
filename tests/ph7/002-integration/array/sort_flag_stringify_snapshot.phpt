--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A __toString() called from a flag sort sees the array as it was at the call — php only guarantees that for the three user-comparator sorts (a recorded divergence)
--DESCRIPTION--
php duplicates the array only for `usort()`, `uasort()` and `uksort()`. The
flag sorts reorder the live array in place, so the one piece of user code they
can reach — a `__toString()` on an element, called to render it for a STRING
flag — can observe an intermediate order, and which order that is belongs to
zend_sort's hybrid insertion sort rather than to any rule.

PHL decides the order over a vector of node pointers and relinks the list once,
after the last comparison, which is what stops a `__toString()` that walks the
array from reading a half-merged list. That applies to every sort it has, so a
flag sort's `__toString()` sees the pre-call array here where php's sees the
sort in progress. Both engines answer the same SORTED array; only what the
element's own renderer can see while it runs differs.
--SKIPIF--
<?php
if (function_exists("zend_version")) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
class Rendered
{
    public function __construct(public int $v) {}

    public function __toString(): string
    {
        $GLOBALS['seen'][implode(',', array_column($GLOBALS['subject'], 'v'))] = 1;
        return (string) $this->v;
    }
}

$seen = [];
$subject = [new Rendered(8), new Rendered(3), new Rendered(6), new Rendered(1),
             new Rendered(7), new Rendered(2), new Rendered(5), new Rendered(4)];
$GLOBALS['subject'] = &$subject;
$GLOBALS['seen'] = &$seen;

sort($subject, SORT_STRING);

$snapshots = array_keys($seen);
var_dump(count($snapshots) === 1 && $snapshots[0] === '8,3,6,1,7,2,5,4');
echo implode(',', array_column($subject, 'v')), "\n";
?>
--EXPECT--
bool(true)
1,2,3,4,5,6,7,8
--CLEAN--
<?php
unset($GLOBALS['subject'], $GLOBALS['seen']);
