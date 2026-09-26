--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A non-representable float subscript wraps into the collection
--FILE--
<?php
/* A collection subscript reads an int out of a non-string offset with the
 * engine's own cast, so a float outside the int64 range wraps there the way it
 * wraps everywhere else -- and warns once per subscript, which is what php's
 * read_dimension does. 1e100 and every non-finite value wrap to 0, so both
 * collections answer their FIRST entry; nothing here is DOM's own rule. This
 * pair used to be twinned: PHL answered PHP_INT_MIN in silence, which read out
 * of range instead (closed in the 70th session). */
set_error_handler(function ($n, $s) { echo "  W: $s\n"; return true; });
$doc = new DOMDocument;
$doc->loadXML('<r a="1" b="2"><k/><j/></r>');
$list = $doc->documentElement->childNodes;
$map = $doc->documentElement->attributes;
foreach (['1e100' => 1.0E+100, '-1e100' => -1.0E+100, 'NAN' => NAN,
          'INF' => INF, '1e19' => 1.0E+19, '1.9 (in range)' => 1.9] as $label => $offset) {
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
  W: The float 1.0E+19 is not representable as an int, cast occurred
  W: The float 1.0E+19 is not representable as an int, cast occurred
1e19             list NULL | map ValueError: must be between 0 and 2147483647
1.9 (in range)   list j | map b
