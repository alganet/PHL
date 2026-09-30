--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The EMPTY property name is a real name, in every shape, and reading a missing one is a warning and not a crash
--DESCRIPTION--
`$o->{''}` names a real property in php -- `$o->{''} = 1` creates one and reads it
back -- and reading a missing one is an ordinary "Undefined property" warning
answering null. Passing that read as a CALL ARGUMENT went down the deferred-lvalue
path instead, which copies the property name; an empty php string hands out a NULL
data pointer, and the copy measured it with strlen. So `f($o->{''})` SEGFAULTED --
on any object, including a bare stdClass, and with the one-line reproducer
`var_dump($o->{''})`.

The shapes are here together because they take different routes through the member
op: an argument defers, a lookup (isset/empty/`??`) short-circuits before the name
is read at all, a plain read answers immediately, and unset() is silent. Only the
three that pass the read DIRECTLY as an argument used to crash -- inside an array
literal or a concatenation the read is not the argument, so nothing captures it as
a deferred lvalue and nothing copies its name.
--FILE--
<?php
set_error_handler(function ($n, $m) { echo "[warn] $m\n"; return true; });
class EmptyNmA { public $x = 'x'; }
function emptyNmTake($v) { return var_export($v, true); }
function emptyNmRef(&$v) { $v = 'bound'; return 'byref'; }
$emptyNmO = new EmptyNmA;
$emptyNmN = '';

echo "as argument : ", emptyNmTake($emptyNmO->{''}), "\n";
echo "dynamic name: ", emptyNmTake($emptyNmO->{$emptyNmN}), "\n";
echo "nested arg  : ", emptyNmTake([$emptyNmO->{''}]), "\n";
echo "concat arg  : ", emptyNmTake('[' . $emptyNmO->{''} . ']'), "\n";
echo "var_dump    : "; var_dump($emptyNmO->{''});
echo "coalesce    : ", emptyNmTake($emptyNmO->{''} ?? 'fallback'), "\n";
echo "isset       : ", var_export(isset($emptyNmO->{''}), true), "\n";
echo "empty       : ", var_export(empty($emptyNmO->{''}), true), "\n";
echo "chain       : ", emptyNmTake($emptyNmO->{''}->{''} ?? 'chain-null'), "\n";
unset($emptyNmO->{''});
echo "unset       : silent\n";
echo "stdClass    : ", emptyNmTake((new stdClass)->{''}), "\n";
restore_error_handler();
?>
--EXPECT--
as argument : [warn] Undefined property: EmptyNmA::$
NULL
dynamic name: [warn] Undefined property: EmptyNmA::$
NULL
nested arg  : [warn] Undefined property: EmptyNmA::$
array (
  0 => NULL,
)
concat arg  : [warn] Undefined property: EmptyNmA::$
'[]'
var_dump    : [warn] Undefined property: EmptyNmA::$
NULL
coalesce    : 'fallback'
isset       : false
empty       : true
chain       : 'chain-null'
unset       : silent
stdClass    : [warn] Undefined property: stdClass::$
NULL
