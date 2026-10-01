--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: an R: naming the OUTERMOST value is refused (PHL half of the twin pair)
--DESCRIPTION--
`R:<n>` binds the slot it lands in to the slot value <n> lives in, and every value has one
-- an array element's node, a property -- except the outermost, which nothing holds yet:
unserialize() answers it to its caller rather than storing it anywhere. php's zvals are
refcounted, so there it can make the outermost value a reference to itself and hand back an
array whose element is the array (`*RECURSION*`); here there is no slot to name, and the
payload is refused with php's own offset report instead.
Nothing php's own serialize() writes can reach this: the outermost value is passed by value,
never as a reference, so it is never a back-reference target. Only a hand-written payload
gets here.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
$payloads = [
    'the whole payload'  => 'a:1:{i:0;R:1;}',
    'one level down'     => 'a:2:{i:0;a:1:{i:0;R:1;}i:1;i:9;}',
];
foreach ($payloads as $label => $payload) {
    echo $label, ': ';
    $v = @unserialize($payload);
    var_dump($v === false ? false : count($v));
}
?>
--EXPECT--
the whole payload: bool(false)
one level down: bool(false)
--CLEAN--
<?php
