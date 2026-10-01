--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Comparing two cyclic arrays throws a catchable Error
--FILE--
<?php
$a = [];
$b = [];
$a[] = &$b;
$b[] = &$a;

// Every comparison operator throws: the loose pair, the STRICT pair (which asks
// no compare handler but still walks), the relational ones and the spaceship.
try { $r = $a ==  $b; echo "==: no throw\n";  } catch (Error $e) { echo "==: ",  $e->getMessage(), "\n"; }
try { $r = $a !=  $b; echo "!=: no throw\n";  } catch (Error $e) { echo "!=: ",  $e->getMessage(), "\n"; }
try { $r = $a === $b; echo "===: no throw\n"; } catch (Error $e) { echo "===: ", $e->getMessage(), "\n"; }
try { $r = $a !== $b; echo "!==: no throw\n"; } catch (Error $e) { echo "!==: ", $e->getMessage(), "\n"; }
try { $r = $a <   $b; echo "<: no throw\n";   } catch (Error $e) { echo "<: ",   $e->getMessage(), "\n"; }
try { $r = $a >=  $b; echo ">=: no throw\n";  } catch (Error $e) { echo ">=: ",  $e->getMessage(), "\n"; }
try { $r = $a <=> $b; echo "<=>: no throw\n"; } catch (Error $e) { echo "<=>: ", $e->getMessage(), "\n"; }

// Same reference: the identity check comes first and must not throw.
try {
    $r = $a == $a;
    echo $r ? "self: equal\n" : "self: not equal\n";
} catch (Error $e) {
    echo "self: unexpected " . $e->getMessage() . "\n";
}

// A refusal must not leave the cycle marker standing: ordinary comparisons
// after one still work, and the same pair refuses again rather than answering.
echo 'after: ', var_export([[1], [2]] == [[1], [2]], true), "\n";
try {
    $a == $b;
    echo "again: no throw\n";
} catch (Error $e) {
    echo "again: " . $e->getMessage() . "\n";
}
?>
--EXPECT--
==: Nesting level too deep - recursive dependency?
!=: Nesting level too deep - recursive dependency?
===: Nesting level too deep - recursive dependency?
!==: Nesting level too deep - recursive dependency?
<: Nesting level too deep - recursive dependency?
>=: Nesting level too deep - recursive dependency?
<=>: Nesting level too deep - recursive dependency?
self: equal
after: true
again: Nesting level too deep - recursive dependency?
--CLEAN--
<?php
unset($a, $b);
