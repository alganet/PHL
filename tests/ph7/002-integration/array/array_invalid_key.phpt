--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object and array offsets are php TypeErrors in every context; a resource offset warns and casts to its id (was a bare skip freezing PHL's silent "Object" key)
--FILE--
<?php
class Foo {}
$obj = new Foo();
$nested = [1, 2];
$a = [];

function attempt($label, callable $fn) {
    echo str_pad($label, 14);
    try {
        $line = 'ok ' . var_export($fn(), true);
    } catch (TypeError $e) {
        $line = get_class($e) . ': ' . $e->getMessage();
    }
    echo $line, "\n";
}

attempt('write obj',   function () use (&$a, $obj) { $a[$obj] = 1; return count($a); });
attempt('read obj',    function () use (&$a, $obj) { return $a[$obj]; });
attempt('coalesce',    function () use (&$a, $obj) { return $a[$obj] ?? 'none'; });
attempt('isset obj',   function () use (&$a, $obj) { return isset($a[$obj]); });
attempt('empty obj',   function () use (&$a, $obj) { return empty($a[$obj]); });
attempt('unset obj',   function () use (&$a, $obj) { unset($a[$obj]); return 'done'; });
attempt('literal obj', function () use ($obj) { $x = [$obj => 1]; return count($x); });
attempt('write arr',   function () use (&$a, $nested) { $a[$nested] = 1; return count($a); });
attempt('isset arr',   function () use (&$a, $nested) { return isset($a[$nested]); });

// A resource offset is NOT rejected: php warns and uses the integer id as key.
// The warnings are captured rather than printed, so the assertions stay on their
// own lines and match on the message BODY (php's log copy prefixes "PHP ").
$warnings = [];
set_error_handler(function ($no, $msg) use (&$warnings) { $warnings[] = $msg; return true; });
$r = fopen('php://memory', 'r');
$id = get_resource_id($r);
$a[$r] = 'from-resource';
$byResource = $a[$r];
$byInt = $a[$id];
restore_error_handler();
fclose($r);

echo 'res key:      ', var_export(array_keys($a) === [$id], true), "\n";
echo 'res read:     ', var_export($byResource === 'from-resource', true), "\n";
echo 'int alias:    ', var_export($byInt === 'from-resource', true), "\n";
echo 'warned:       ', var_export(count($warnings) === 2, true), "\n";
echo 'warn text:    ', $warnings[0], "\n";
?>
--EXPECTF--
write obj     TypeError: Cannot access offset of type Foo on array
read obj      TypeError: Cannot access offset of type Foo on array
coalesce      TypeError: Cannot access offset of type Foo on array
isset obj     TypeError: Cannot access offset of type Foo in isset or empty
empty obj     TypeError: Cannot access offset of type Foo in isset or empty
unset obj     TypeError: Cannot unset offset of type Foo on array
literal obj   TypeError: Cannot access offset of type Foo on array
write arr     TypeError: Cannot access offset of type array on array
isset arr     TypeError: Cannot access offset of type array in isset or empty
res key:      true
res read:     true
int alias:    true
warned:       true
warn text:    Resource ID#%d used as offset, casting to integer (%d)
--CLEAN--
<?php
unset($obj, $nested, $a, $r, $id, $warnings, $byResource, $byInt);
