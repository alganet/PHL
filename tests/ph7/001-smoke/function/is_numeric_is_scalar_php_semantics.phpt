--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: is_numeric rejects booleans, is_scalar rejects null (strict php semantics)
--FILE--
<?php
// is_numeric: booleans are NOT numeric in php (PHL used to accept them because
// its arithmetic-coercion helper treats bool as numeric); int/float and numeric
// strings still are.
$isnvNum = [true, false, null, 0, 1, 3.14, '42', '3.14', '1e3', ' 5', '0x1A', 'x', [], '0'];
$isnvOut = [];
foreach ($isnvNum as $isnvV) { $isnvOut[] = var_export(is_numeric($isnvV), true); }
echo implode(' ', $isnvOut), "\n";
// is_scalar: int/float/string/bool are scalar; null/array/object are not (PHL's
// internal scalar bucket wrongly included null).
$isnvScal = [true, false, null, 0, 0.0, '', 's', [], new stdClass];
$isnvOut = [];
foreach ($isnvScal as $isnvV) { $isnvOut[] = var_export(is_scalar($isnvV), true); }
echo implode(' ', $isnvOut), "\n";
?>
--EXPECT--
false false false true true true true true true true false false false true
true true false true true true true false false
