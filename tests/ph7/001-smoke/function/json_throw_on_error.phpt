--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_THROW_ON_ERROR + JsonException, and Inf/NaN encode rejection
--FILE--
<?php
echo JSON_THROW_ON_ERROR, " ", JSON_ERROR_INF_OR_NAN, "\n";
echo get_parent_class(new JsonException('x')), "\n";
// a valid decode with the flag is unaffected
echo json_encode(json_decode('[1,2,3]', true, 512, JSON_THROW_ON_ERROR)), "\n";
// malformed decode WITHOUT the flag: NULL, error recorded, no throw
var_export(json_decode('{bad')); echo " ", json_last_error(), " ", json_last_error_msg(), "\n";
// malformed decode WITH the flag: a JsonException (catchable as Exception)
try { json_decode('{bad', true, 512, JSON_THROW_ON_ERROR); }
catch (JsonException $jseE) { echo "decode: ", get_class($jseE), " ", $jseE->getMessage(), "\n"; }
// Inf/NaN encode WITHOUT the flag: false + JSON_ERROR_INF_OR_NAN (no invalid token)
var_export(json_encode(NAN)); echo " ", json_last_error(), "\n";
var_export(json_encode([1.5, INF, 3])); echo " ", json_last_error(), "\n";
// Inf/NaN encode WITH the flag: throws
try { json_encode(INF, JSON_THROW_ON_ERROR); }
catch (Exception $jseE) { echo "encode: ", get_class($jseE), " ", $jseE->getMessage(), "\n"; }
// finite floats still encode fine
echo json_encode([1.5, -0.0, 2.0]), "\n";
?>
--EXPECT--
4194304 7
Exception
[1,2,3]
NULL 4 Syntax error
decode: JsonException Syntax error
false 7
false 7
encode: JsonException Inf and NaN cannot be JSON encoded
[1.5,-0,2]
