--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous class: what a message SHOWS is the name up to the NUL
--FILE--
<?php
class AnonDpBase { public $inherited = 1; }

$o = new class extends AnonDpBase { public $own = 2; };

/* Every DISPLAY surface stops at the NUL php puts after `@anonymous`. The object
 * handle is not part of what is being tested and does not match between engines. */
ob_start(); var_dump($o); print_r($o); $dump = ob_get_clean();
echo preg_replace('/#\d+/', '#N', $dump);
echo get_debug_type($o), "\n";

$say = static function (callable $f): void {
    try { $f(); } catch (\Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
};
$say(static fn() => $o->missing());
$say(static fn() => serialize($o));
$say(static fn() => (string) $o);
$say(static fn() => $o[0]);
$say(static fn() => strlen($o));

// …while var_export hands back the IDENTITY, NUL and all — it is source php can read.
var_dump(str_contains(var_export($o, true), "@anonymous\0"));
?>
--EXPECT--
object(AnonDpBase@anonymous)#N (2) {
  ["inherited"]=>
  int(1)
  ["own"]=>
  int(2)
}
AnonDpBase@anonymous Object
(
    [inherited] => 1
    [own] => 2
)
AnonDpBase@anonymous
Error: Call to undefined method AnonDpBase@anonymous::missing()
Exception: Serialization of 'AnonDpBase@anonymous' is not allowed
Error: Object of class AnonDpBase@anonymous could not be converted to string
Error: Cannot use object of type AnonDpBase@anonymous as array
TypeError: strlen(): Argument #1 ($string) must be of type string, AnonDpBase@anonymous given
bool(true)
--CLEAN--
<?php
unset($o, $say, $dump);
