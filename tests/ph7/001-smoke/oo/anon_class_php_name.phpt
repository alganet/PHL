--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous class: php's two-part name (prefix@anonymous, NUL, file:line$hex)
--FILE--
<?php
interface AnonNmIface {}
interface AnonNmIface2 {}
class AnonNmBase {}

$halves = static function (object $o): array {
    $n = get_class($o);
    $p = strpos($n, "\0");
    return $p === false ? [$n, null] : [substr($n, 0, $p), substr($n, $p + 1)];
};

// The prefix is the parent, else the FIRST interface, else the word "class".
var_dump($halves(new class extends AnonNmBase {})[0]);
var_dump($halves(new class implements AnonNmIface2, AnonNmIface {})[0]);
var_dump($halves(new class {})[0]);
var_dump($halves(new class extends AnonNmBase implements AnonNmIface {})[0]);

// The tail behind the NUL is where the class was written.
$tail = $halves(new class {})[1];
var_dump((bool) preg_match('~^' . preg_quote(__FILE__, '~') . ':\d+\$[0-9a-f]+$~', $tail));

// The idiom every library uses to recognize one.
$o = new class extends AnonNmBase implements AnonNmIface {};
var_dump(strpos(get_class($o), "@anonymous\0") !== false);
var_dump(get_parent_class(get_class($o)));
var_dump(array_key_first(class_implements(get_class($o))));

// Two written on one line are two classes; one SITE is one class however often it runs.
$a = new class {}; $b = new class {};
var_dump(get_class($a) === get_class($b));
$mk = static fn() => new class {};
var_dump(get_class($mk()) === get_class($mk()));

// The full name is the identity: it resolves, and Reflection hands back the same bytes.
var_dump(class_exists(get_class($o)));
var_dump((new ReflectionClass($o))->getName() === get_class($o));
var_dump((new ReflectionClass($o))->isAnonymous());
var_dump((new ReflectionClass('AnonNmBase'))->isAnonymous());
?>
--EXPECT--
string(20) "AnonNmBase@anonymous"
string(22) "AnonNmIface2@anonymous"
string(15) "class@anonymous"
string(20) "AnonNmBase@anonymous"
bool(true)
bool(true)
string(10) "AnonNmBase"
string(11) "AnonNmIface"
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
--CLEAN--
<?php
unset($halves, $tail, $o, $a, $b, $mk);
