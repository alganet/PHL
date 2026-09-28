--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A Throwable's line slot is initialized by the site stamp, Error included
--FILE--
<?php
function elShape(Throwable $t): void {
    echo get_class($t), "\n";
    echo "  mangled: ", str_replace("\0", '@', implode(',', array_keys(get_mangled_object_vars($t)))), "\n";
    echo "  cast:    ", str_replace("\0", '@', implode(',', array_keys((array)$t))), "\n";
    echo "  vars:    ", implode(',', array_keys(get_object_vars($t))), "\n";
    $keys = [];
    array_walk($t, function ($v, $k) use (&$keys) { $keys[] = str_replace("\0", '@', $k); });
    echo "  walk:    ", implode(',', $keys), "\n";
    echo "  line is int: ", var_export(is_int($t->getLine()), true), "\n";
    echo "  line > 0:    ", var_export($t->getLine() > 0, true), "\n";
    ob_start(); var_dump($t); $d = ob_get_clean();
    /* The HEADER count only: the trace this dump prints carries the file and line
     * of every frame, which the test harness moves around. */
    preg_match('/^object\([A-Za-z]+\)#\d+ \((\d+)\)/', $d, $m);
    echo "  var_dump counts: ", $m[1], "\n";
    echo "  var_dump uninitialized: ", var_export(strpos($d, 'uninitialized(') !== false, true), "\n";
}
elShape(new Exception('m', 1));
elShape(new Error('m', 2));
elShape(new TypeError('m', 3));
elShape(new ValueError('m', 4));
elShape(new ErrorException('m', 5));
try { throw new DivisionByZeroError('d'); } catch (Throwable $e) { elShape($e); }
?>
--EXPECT--
Exception
  mangled: @*@message,@Exception@string,@*@code,@*@file,@*@line,@Exception@trace,@Exception@previous
  cast:    @*@message,@Exception@string,@*@code,@*@file,@*@line,@Exception@trace,@Exception@previous
  vars:    
  walk:    @*@message,@Exception@string,@*@code,@*@file,@*@line,@Exception@trace,@Exception@previous
  line is int: true
  line > 0:    true
  var_dump counts: 7
  var_dump uninitialized: false
Error
  mangled: @*@message,@Error@string,@*@code,@*@file,@*@line,@Error@trace,@Error@previous
  cast:    @*@message,@Error@string,@*@code,@*@file,@*@line,@Error@trace,@Error@previous
  vars:    
  walk:    @*@message,@Error@string,@*@code,@*@file,@*@line,@Error@trace,@Error@previous
  line is int: true
  line > 0:    true
  var_dump counts: 7
  var_dump uninitialized: false
TypeError
  mangled: @*@message,@Error@string,@*@code,@*@file,@*@line,@Error@trace,@Error@previous
  cast:    @*@message,@Error@string,@*@code,@*@file,@*@line,@Error@trace,@Error@previous
  vars:    
  walk:    @*@message,@Error@string,@*@code,@*@file,@*@line,@Error@trace,@Error@previous
  line is int: true
  line > 0:    true
  var_dump counts: 7
  var_dump uninitialized: false
ValueError
  mangled: @*@message,@Error@string,@*@code,@*@file,@*@line,@Error@trace,@Error@previous
  cast:    @*@message,@Error@string,@*@code,@*@file,@*@line,@Error@trace,@Error@previous
  vars:    
  walk:    @*@message,@Error@string,@*@code,@*@file,@*@line,@Error@trace,@Error@previous
  line is int: true
  line > 0:    true
  var_dump counts: 7
  var_dump uninitialized: false
ErrorException
  mangled: @*@message,@Exception@string,@*@code,@*@file,@*@line,@Exception@trace,@Exception@previous,@*@severity
  cast:    @*@message,@Exception@string,@*@code,@*@file,@*@line,@Exception@trace,@Exception@previous,@*@severity
  vars:    
  walk:    @*@message,@Exception@string,@*@code,@*@file,@*@line,@Exception@trace,@Exception@previous,@*@severity
  line is int: true
  line > 0:    true
  var_dump counts: 8
  var_dump uninitialized: false
DivisionByZeroError
  mangled: @*@message,@Error@string,@*@code,@*@file,@*@line,@Error@trace,@Error@previous
  cast:    @*@message,@Error@string,@*@code,@*@file,@*@line,@Error@trace,@Error@previous
  vars:    
  walk:    @*@message,@Error@string,@*@code,@*@file,@*@line,@Error@trace,@Error@previous
  line is int: true
  line > 0:    true
  var_dump counts: 7
  var_dump uninitialized: false
--CLEAN--
<?php
unset($e);
