--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A comparator answering a bool is deprecated and a false is asked again swapped
--DESCRIPTION--
php 8 reads `return $a > $b;` from a sort or diff/intersect comparator as
deprecated: it raises "Returning bool from comparison function is deprecated"
once per call, and because false cannot tell "less" from "equal" it asks the
callback again with the operands swapped. The three sorts and the eight members
of the diff/intersect family that sort and merge do both; array_udiff_assoc()
and array_uintersect_assoc() ask their value callback through a plain compare
and do neither. The deprecation is armed again by every call (a usort() inside
a comparator re-arms the outer one), and a handler that throws stops the
comparison where php stops it. The callback's answer is reduced by its sign
over all 64 bits, so `($a <=> $b) << 32` still orders.
--FILE--
<?php
set_error_handler(function ($no, $msg, $file, $line) {
    echo "  [$no] $msg @", $line, "\n";
    return true;
});
$log = [];
$lt  = function ($a, $b) use (&$log) { $log[] = "$a,$b"; return $a > $b; };
$lt2 = function ($a, $b) use (&$log) { $log[] = "k$a,$b"; return $a > $b; };
$run = function ($name, callable $f) use (&$log) {
    $log = [];
    echo "$name\n";
    $r = $f();
    echo "  ", json_encode($r), "\n  calls: ", implode(' ', $log), "\n";
};
$run('usort', function () use ($lt) { $a = [3, 1, 2, 1]; usort($a, $lt); return $a; });
$run('uasort', function () use ($lt) { $a = ['x' => 3, 'y' => 1, 'z' => 2]; uasort($a, $lt); return $a; });
$run('uksort', function () use ($lt) { $a = [3 => 'a', 1 => 'b', 2 => 'c']; uksort($a, $lt); return $a; });
$run('usort int', function () use (&$log) { $a = [3, 1, 2]; usort($a, function ($a, $b) use (&$log) { $log[] = "$a,$b"; return ($a <=> $b) << 32; }); return $a; });
$A = ['a' => 1, 'b' => 5, 'c' => 3, 'd' => 4];
$B = ['a' => 3, 'b' => 5, 'e' => 4];
$run('array_udiff', fn() => array_udiff($A, $B, $lt));
$run('array_uintersect', fn() => array_uintersect($A, $B, $lt));
$run('array_diff_ukey', fn() => array_diff_ukey($A, $B, $lt2));
$run('array_intersect_ukey', fn() => array_intersect_ukey($A, $B, $lt2));
$run('array_diff_uassoc', fn() => array_diff_uassoc($A, $B, $lt2));
$run('array_intersect_uassoc', fn() => array_intersect_uassoc($A, $B, $lt2));
$run('array_udiff_uassoc', fn() => array_udiff_uassoc($A, $B, $lt, $lt2));
$run('array_uintersect_uassoc', fn() => array_uintersect_uassoc($A, $B, $lt, $lt2));
$run('array_udiff_assoc', fn() => array_udiff_assoc($A, $B, $lt));
$run('array_uintersect_assoc', fn() => array_uintersect_assoc($A, $B, $lt));
// once per call, re-armed by the next call
$run('twice', function () use ($lt) { $a = [2, 1]; usort($a, $lt); $b = [2, 1]; usort($b, $lt); return [$a, $b]; });
// a nested call re-arms the outer one's latch
$run('nested', function () use (&$log) {
    $a = [3, 1, 2];
    usort($a, function ($x, $y) use (&$log) {
        $log[] = "$x,$y";
        $i = [2, 1];
        usort($i, fn($p, $q) => $p > $q);
        return $x > $y;
    });
    return $a;
});
// ArrayObject runs uasort/uksort
$run('ArrayObject::uasort', function () use ($lt) { $o = new ArrayObject(['x' => 2, 'y' => 1]); $o->uasort($lt); return $o->getArrayCopy(); });
$run('ArrayObject::uksort', function () use ($lt) { $o = new ArrayObject(['b' => 2, 'a' => 1]); $o->uksort($lt); return $o->getArrayCopy(); });
// a throwing error handler stops the sort
set_error_handler(function ($no, $msg) { throw new ErrorException($msg, 0, $no); });
$log = [];
$a = [3, 1, 2];
try { usort($a, $lt); } catch (ErrorException $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
echo json_encode($a), ' calls: ', implode(' ', $log), "\n";
$log = [];
$k = [3 => 'a', 1 => 'b', 2 => 'c'];
try { uksort($k, $lt); } catch (ErrorException $e) { echo "caught from uksort\n"; }
echo json_encode($k), ' calls: ', implode(' ', $log), "\n";
$log = [];
try { array_udiff([1, 5, 3], [3], $lt); } catch (ErrorException $e) { echo "caught from array_udiff\n"; }
echo 'calls: ', implode(' ', $log), "\n";
// false first: the swapped call is never made with the exception pending
$a = [1, 2, 3];
try { usort($a, fn($x, $y) => $x < $y); } catch (ErrorException $e) { echo "caught from usort\n"; }
echo json_encode($a), "\n";
restore_error_handler();
restore_error_handler();
--EXPECT--
usort
  [8192] usort(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @15
  [1,1,2,3]
  calls: 3,1 2,1 3,2 3,1 2,1 1,1 1,1
uasort
  [8192] uasort(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @16
  {"y":1,"z":2,"x":3}
  calls: 3,1 2,1 3,2
uksort
  [8192] uksort(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @17
  {"1":"b","2":"c","3":"a"}
  calls: 3,1 2,1 3,2
usort int
  [1,2,3]
  calls: 3,1 2,1 3,2
array_udiff
  [8192] array_udiff(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @21
  {"a":1}
  calls: 1,5 5,1 5,3 1,3 3,1 5,4 3,4 4,3 3,5 5,3 5,4 3,4 4,3 1,3 3,1 1,3 3,1 3,3 3,3 3,4 4,3 4,4 4,4 4,5 5,4 5,5 5,5
array_uintersect
  [8192] array_uintersect(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @22
  {"b":5,"c":3,"d":4}
  calls: 1,5 5,1 5,3 1,3 3,1 5,4 3,4 4,3 3,5 5,3 5,4 3,4 4,3 1,3 3,1 3,3 3,3 3,3 3,3 3,4 4,3 4,4 4,4 4,5 5,4 5,5 5,5
array_diff_ukey
  [8192] array_diff_ukey(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @23
  {"c":3,"d":4}
  calls: ka,b kb,a kb,c kc,b kc,d kd,c ka,b kb,a kb,e ke,b ka,a ka,a kb,a kb,b kb,b kc,a kc,b kc,e ke,c kd,a kd,b kd,e ke,d
array_intersect_ukey
  [8192] array_intersect_ukey(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @24
  {"a":1,"b":5}
  calls: ka,b kb,a kb,c kc,b kc,d kd,c ka,b kb,a kb,e ke,b ka,a ka,a kb,b kb,b kc,e ke,c kd,e ke,d
array_diff_uassoc
  [8192] array_diff_uassoc(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @25
  {"a":1,"c":3,"d":4}
  calls: ka,b kb,a kb,c kc,b kc,d kd,c ka,b kb,a kb,e ke,b ka,a ka,a kb,a kb,b kb,b kc,a kc,b kc,e ke,c kd,a kd,b kd,e ke,d
array_intersect_uassoc
  [8192] array_intersect_uassoc(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @26
  {"b":5}
  calls: ka,b kb,a kb,c kc,b kc,d kd,c ka,b kb,a kb,e ke,b ka,a ka,a kb,a kb,b kb,b kc,e ke,c kd,e ke,d
array_udiff_uassoc
  [8192] array_udiff_uassoc(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @27
  {"a":1,"c":3,"d":4}
  calls: ka,b kb,a kb,c kc,b kc,d kd,c ka,b kb,a kb,e ke,b ka,a ka,a 1,3 3,1 kb,a kb,b kb,b 5,5 5,5 kc,a kc,b kc,e ke,c kd,a kd,b kd,e ke,d
array_uintersect_uassoc
  [8192] array_uintersect_uassoc(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @28
  {"b":5}
  calls: ka,b kb,a kb,c kc,b kc,d kd,c ka,b kb,a kb,e ke,b ka,a ka,a 1,3 3,1 kb,a kb,b kb,b 5,5 5,5 kc,e ke,c kd,e ke,d
array_udiff_assoc
  {"c":3,"d":4}
  calls: 1,3 5,5
array_uintersect_assoc
  {"a":1,"b":5}
  calls: 1,3 5,5
twice
  [8192] usort(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @32
  [8192] usort(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @32
  [[1,2],[1,2]]
  calls: 2,1 2,1
nested
  [8192] usort(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @39
  [8192] usort(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @39
  [8192] usort(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @39
  [1,2,3]
  calls: 3,1 2,1 3,2
ArrayObject::uasort
  [8192] uasort(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @45
  {"y":1,"x":2}
  calls: 2,1
ArrayObject::uksort
  [8192] uksort(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero @46
  {"a":1,"b":2}
  calls: b,a
ErrorException: usort(): Returning bool from comparison function is deprecated, return an integer less than, equal to, or greater than zero
[1,3,2] calls: 3,1
caught from uksort
{"1":"b","3":"a","2":"c"} calls: 3,1
caught from array_udiff
calls: 1,5
caught from usort
[2,1,3]
