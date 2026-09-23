--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A read-modify-write that creates a property warns that it was undefined first
--DESCRIPTION--
`$o->hits++` on a fresh object READS before it writes, so php warns
`Undefined property: C::$hits` and then goes on with null — answering int(1) and
leaving the property behind. PHL created the slot in silence. Only the
read-modify-write forms: a subscript-write base (`$o->arr[] = 1`) and a `??=` read
nothing, and php says nothing for either. A property that is DECLARED is defined
whatever it holds, so it stays silent — but one that was unset() is undefined
again.
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "  [$no] $msg\n"; return true; });

class RmwWarnDeclared { public $n; public $m = 5; }
#[AllowDynamicProperties] class RmwWarnDynamic {}

$o = new RmwWarnDeclared(); $o->n++;  echo "declared null ++ => ", var_export($o->n, true), "\n";
$o = new RmwWarnDeclared(); $o->m++;  echo "declared 5 ++    => ", var_export($o->m, true), "\n";
$o = new RmwWarnDeclared(); unset($o->n); $o->n++;
                                      echo "unset then ++    => ", var_export($o->n, true), "\n";

$d = new RmwWarnDynamic(); $d->z++;      echo "dynamic ++  => ", var_export($d->z, true), "\n";
$d = new RmwWarnDynamic(); $d->s .= "a"; echo "dynamic .=  => ", var_export($d->s, true), "\n";
$d = new RmwWarnDynamic(); $d->w += 2;   echo "dynamic +=  => ", var_export($d->w, true), "\n";
$d = new RmwWarnDynamic(); $d->q--;      echo "dynamic --  => ", var_export($d->q, true), "\n";

// Neither of these READS the property, so neither warns.
$d = new RmwWarnDynamic(); $d->k ??= 3;  echo "??=         => ", var_export($d->k, true), "\n";
$s = new stdClass(); $s->arr[] = 1;      echo "subscript   => ", var_export($s->arr, true), "\n";
$s = new stdClass(); $s->p = 7;          echo "plain store => ", var_export($s->p, true), "\n";

$s = new stdClass(); $s->t++;            echo "stdClass ++ => ", var_export($s->t, true), "\n";
?>
--EXPECT--
declared null ++ => 1
declared 5 ++    => 6
  [2] Undefined property: RmwWarnDeclared::$n
unset then ++    => 1
  [2] Undefined property: RmwWarnDynamic::$z
dynamic ++  => 1
  [2] Undefined property: RmwWarnDynamic::$s
dynamic .=  => 'a'
  [2] Undefined property: RmwWarnDynamic::$w
dynamic +=  => 2
  [2] Undefined property: RmwWarnDynamic::$q
  [2] Decrement on type null has no effect, this will change in the next major version of PHP
dynamic --  => NULL
??=         => 3
subscript   => array (
  0 => 1,
)
plain store => 7
  [2] Undefined property: stdClass::$t
stdClass ++ => 1
