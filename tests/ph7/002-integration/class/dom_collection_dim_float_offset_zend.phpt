--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: a non-representable float subscript reads offset 0 and warns (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* A collection subscript converts a non-string offset with the engine's own
 * int cast, and for a float outside the int64 range the two engines do not
 * agree on what that cast IS: php answers 0 behind
 * `The float X is not representable as an int, cast occurred`, PHL answers
 * PHP_INT_MIN in silence (twin-paired in
 * lang/float_to_int_out_of_range{,_zend}). The subscript makes the difference
 * VISIBLE: php reads the first entry, PHL reads out of range -- which the list
 * answers null to and the map refuses. Nothing here is DOM's own rule; closing
 * §2 closes this too. */
set_error_handler(function ($n, $s) { echo "  W: $s\n"; return true; });
$doc = new DOMDocument;
$doc->loadXML('<r a="1" b="2"><k/><j/></r>');
$list = $doc->documentElement->childNodes;
$map = $doc->documentElement->attributes;
foreach (['1e100' => 1.0E+100, '-1e100' => -1.0E+100, 'NAN' => NAN,
          'INF' => INF, '1.9 (in range)' => 1.9] as $label => $offset) {
    $said = 'list ';
    try { $hit = $list[$offset]; $said .= $hit === null ? 'NULL' : $hit->nodeName; }
    catch (Throwable $ex) { $said .= get_class($ex) . ': ' . $ex->getMessage(); }
    $said .= ' | map ';
    try { $hit = $map[$offset]; $said .= $hit === null ? 'NULL' : $hit->nodeName; }
    catch (Throwable $ex) { $said .= get_class($ex) . ': ' . $ex->getMessage(); }
    printf("%-16s %s\n", $label, $said);
}
?>
--EXPECT--
  W: The float 1.0E+100 is not representable as an int, cast occurred
  W: The float 1.0E+100 is not representable as an int, cast occurred
1e100            list k | map a
  W: The float -1.0E+100 is not representable as an int, cast occurred
  W: The float -1.0E+100 is not representable as an int, cast occurred
-1e100           list k | map a
  W: The float NAN is not representable as an int, cast occurred
  W: The float NAN is not representable as an int, cast occurred
NAN              list k | map a
  W: The float INF is not representable as an int, cast occurred
  W: The float INF is not representable as an int, cast occurred
INF              list k | map a
1.9 (in range)   list j | map b
