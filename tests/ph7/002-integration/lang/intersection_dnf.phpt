--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DNF type (A&B)|C selects the right branch and rejects values matching neither
--FILE--
<?php
interface DnfA {}
interface DnfB {}
class DnfC {}
class DnfAB implements DnfA, DnfB {}
function dnf_f((DnfA&DnfB)|DnfC $x): string { return get_class($x); }
echo dnf_f(new DnfAB), "\n";   // matches the intersection branch
echo dnf_f(new DnfC), "\n";    // matches the single-type branch
try {
    dnf_f(new class implements DnfA {});  // only DnfA → neither branch
} catch (\TypeError $e) {
    echo "rejected\n";
}
?>
--EXPECT--
DnfAB
DnfC
rejected
--CLEAN--
<?php
