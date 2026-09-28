--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A throw from inside `f(...$x)` is caught and the rest of the function still runs
--DESCRIPTION--
OP_SPREAD sent every throw straight to the dispatch loop's Exception label, which unwinds the
whole invocation — right at a CALL boundary, wrong for an argument list, which is mid-expression.
A `try` around the call caught the exception and then EVERY STATEMENT AFTER IT was dropped, exit
0: the function's own `return` never ran and the caller read null. It was reachable before the
key screen existed (a throwing rewind(), key() or current() in `f(...$it)` shows it) and ordinary
after. It routes like every other mid-expression throw now (PH7_THROW_ROUTE_MIDEXPR): an inline
resume, else this frame's own catch landing pad, else propagate.
--FILE--
<?php
function utrH($x = 1, $y = 2) { return "$x/$y"; }
class UtrIt implements Iterator {
    public $mode;
    function __construct($m) { $this->mode = $m; }
    function rewind(): void  { if ($this->mode === 'rewind') throw new Exception("boom-rewind"); }
    function valid(): bool   { return $this->mode !== 'done'; }
    function current(): mixed { if ($this->mode === 'current') throw new Exception("boom-current"); return 1; }
    function key(): mixed    { if ($this->mode === 'key') throw new Exception("boom-key"); return 0; }
    function next(): void    { $this->mode = 'done'; }
}
function w($label, $fn) {
    echo "before[$label]\n";
    try { $fn(); } catch (Throwable $e) { echo "  caught: ", get_class($e), ": ", $e->getMessage(), "\n"; }
    echo "after[$label]\n";
    return "returned[$label]";
}
echo w('null',      fn() => utrH(...null)), "\n";
echo w('object',    fn() => utrH(...new stdClass())), "\n";
echo w('bad key',   function () { $g = (function () { yield 1.5 => 1; })(); return utrH(...$g); }), "\n";
echo w('duplicate', function () { $g = (function () { yield 'x' => 1; yield 'x' => 2; })(); return utrH(...$g); }), "\n";
echo w('rewind',    fn() => utrH(...new UtrIt('rewind'))), "\n";
echo w('key',       fn() => utrH(...new UtrIt('key'))), "\n";
echo w('current',   fn() => utrH(...new UtrIt('current'))), "\n";
echo w('new',       fn() => new ArrayObject(...null)), "\n";
echo w('nested',    fn() => utrH(...utrH(...null))), "\n";

function utrU() { utrH(...null); return "never"; }
try { var_dump(utrU()); } catch (Throwable $e) { echo "propagated: ", $e->getMessage(), "\n"; }
echo "END\n";
?>
--EXPECT--
before[null]
  caught: TypeError: Only arrays and Traversables can be unpacked, null given
after[null]
returned[null]
before[object]
  caught: TypeError: Only arrays and Traversables can be unpacked, stdClass given
after[object]
returned[object]
before[bad key]
  caught: Error: Keys must be of type int|string during argument unpacking
after[bad key]
returned[bad key]
before[duplicate]
  caught: Error: Named parameter $x overwrites previous argument
after[duplicate]
returned[duplicate]
before[rewind]
  caught: Exception: boom-rewind
after[rewind]
returned[rewind]
before[key]
  caught: Exception: boom-key
after[key]
returned[key]
before[current]
  caught: Exception: boom-current
after[current]
returned[current]
before[new]
  caught: TypeError: Only arrays and Traversables can be unpacked, null given
after[new]
returned[new]
before[nested]
  caught: TypeError: Only arrays and Traversables can be unpacked, null given
after[nested]
returned[nested]
propagated: Only arrays and Traversables can be unpacked, null given
END
