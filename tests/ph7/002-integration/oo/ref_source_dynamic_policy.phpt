--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a reference source creating a dynamic property meets the scope policy's refusal (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
class RspDecl { public $a = 1; }
#[AllowDynamicProperties] class RspOpen { public $a = 1; }
class RspSub extends stdClass {}

function rsp(string $label, callable $fn): void {
    set_error_handler(function ($no, $msg) { echo "  E$no: $msg\n"; return true; });
    try { $out = $fn(); } catch (\Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    restore_error_handler();
    printf("%-32s -> %s\n", $label, json_encode($out));
}
rsp('a declaring class', function () { $o = new RspDecl; $r =& $o->z; $r = 'V'; return get_object_vars($o); });
rsp('...opted in', function () { $o = new RspOpen; $r =& $o->z; $r = 'V'; return get_object_vars($o); });
rsp('a subclass of stdClass', function () { $o = new RspSub; $r =& $o->z; $r = 'V'; return get_object_vars($o); });
rsp('...and its plain write', function () { $o = new RspSub; $o->z = 'W'; return get_object_vars($o); });
rsp('a native class', function () { $iv = new DateInterval('PT5S'); $r =& $iv->nope; $r = 9; return $iv->nope; });
rsp('an SPL container', function () { $ao = new ArrayObject([]); $r =& $ao->z; $r = 3; return [$ao->getArrayCopy(), get_object_vars($ao)]; });
rsp('a by-reference foreach', function () { $o = new RspDecl; foreach ($o->list as &$v) {} return get_object_vars($o); });
--EXPECT--
a declaring class                -> "Error: Cannot create dynamic property RspDecl::$z"
...opted in                      -> {"a":1,"z":"V"}
a subclass of stdClass           -> {"z":"V"}
...and its plain write           -> {"z":"W"}
a native class                   -> "Error: Cannot create dynamic property DateInterval::$nope"
an SPL container                 -> "Error: Cannot create dynamic property ArrayObject::$z"
a by-reference foreach           -> "Error: Cannot create dynamic property RspDecl::$list"
--CLEAN--
<?php
