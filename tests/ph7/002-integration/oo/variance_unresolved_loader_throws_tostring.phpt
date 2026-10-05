--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The loader-throw fatal at a declaration renders the exception through its own __toString()
--DESCRIPTION--
php builds the message from the exception's string form, so a user override
replaces the whole "Class: message in file" text, and the fatal is located at
the declaration rather than at the throw.
--FILE--
<?php
class VuLsEx extends Exception {
    function __toString(): string { return "VuLsEx says " . $this->getMessage(); }
}
function vuLsLoad($c) {
    echo "load $c\n";
    if ($c === 'VuLsB') {
        throw new VuLsEx("no $c", 0, new RuntimeException("cause"));
    }
}
spl_autoload_register('vuLsLoad');
interface VuLsI { public VuLsA $p { get; } }
class VuLsKid implements VuLsI { public VuLsB $p; }
echo "unreached\n";
?>
--EXPECTF--
load VuLsB
%s Fatal error:  During inheritance of VuLsKid, while autoloading VuLsB: Uncaught VuLsEx says no VuLsB in %s on line 13
Stack trace:
#0 {main}
