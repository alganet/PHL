--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `Type $x = null` default makes the param implicitly nullable (accepts null)
--FILE--
<?php
// PHP treats a literal `null` default as making the type implicitly nullable,
// so an explicit null is accepted for class, scalar and union types alike.
// (PHP 8.4 also emits a deprecation at declaration time; PHL does not — the %A
// below swallows it so this stays a cross-engine test.)
class C {}
class A {}
class B {}

function fClass(C $c = null)   { echo 'class:',  ($c === null ? 'null' : 'obj'),  "\n"; }
function fInt(int $x = null)   { echo 'int:',    ($x === null ? 'null' : $x),     "\n"; }
function fUnion(A|B $x = null) { echo 'union:',  ($x === null ? 'null' : 'obj'),  "\n"; }

// Explicit null and the OMITTED case must both yield null — a null default must
// not be cast to 0/""/false when the arg is left out.
fClass(null);
fClass();
fClass(new C());
fInt(null);
fInt();
fInt(7);
fUnion(null);
fUnion();
fUnion(new A());
?>
--EXPECTF--
%Aclass:null
class:null
class:obj
int:null
int:null
int:7
union:null
union:null
union:obj
--CLEAN--
<?php
