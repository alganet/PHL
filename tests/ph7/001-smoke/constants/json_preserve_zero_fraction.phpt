--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_PRESERVE_ZERO_FRACTION constant and behaviour
--FILE--
<?php
echo "JSON_PRESERVE_ZERO_FRACTION=" . JSON_PRESERVE_ZERO_FRACTION . "\n";
// A float with no fractional digits keeps its ".0" so the round trip stays a float.
echo json_encode(1.0), "\n";
echo json_encode(1.0, JSON_PRESERVE_ZERO_FRACTION), "\n";
echo json_encode([1.0, 2.5, -3.0, 1e20], JSON_PRESERVE_ZERO_FRACTION), "\n";
echo json_encode(-0.0), "\n";
echo json_encode(-0.0, JSON_PRESERVE_ZERO_FRACTION), "\n";
// Already fractional or exponential shapes are untouched.
echo json_encode(1e17, JSON_PRESERVE_ZERO_FRACTION), "\n";
echo json_encode(3.14, JSON_PRESERVE_ZERO_FRACTION), "\n";
// It rides JSON_NUMERIC_CHECK's float too.
echo json_encode("1.0", JSON_NUMERIC_CHECK | JSON_PRESERVE_ZERO_FRACTION), "\n";
echo json_encode("1.0", JSON_NUMERIC_CHECK), "\n";
?>
--EXPECT--
JSON_PRESERVE_ZERO_FRACTION=1024
1
1.0
[1.0,2.5,-3.0,1.0e+20]
-0
-0.0
1.0e+17
3.14
1.0
1
--CLEAN--
<?php
