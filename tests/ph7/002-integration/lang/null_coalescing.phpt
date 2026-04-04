--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Null coalescing operator ??
--FILE--
<?php
// Non-null left side
$a = "hello";
echo ($a ?? "default") . "\n";

// Null left side
$b = null;
echo ($b ?? "fallback") . "\n";

// Chaining
$c = null;
$d = null;
echo ($c ?? $d ?? "end") . "\n";

// Non-null stops the chain
$e = "found";
echo ($c ?? $e ?? "end") . "\n";

// With array access
$arr = ['key' => 'value'];
echo ($arr['key'] ?? 'missing') . "\n";
echo ($arr['nope'] ?? 'missing') . "\n";

// Assignment with ??
$x = null;
$y = $x ?? 42;
echo $y . "\n";
?>
--EXPECT--
hello
fallback
end
found
value
missing
42
--CLEAN--
<?php

