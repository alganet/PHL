--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
date_create_from_format and date_create_immutable_from_format procedural aliases
--FILE--
<?php
function dcffCases(): array {
    date_default_timezone_set("UTC");
    $out = [];
    $d = date_create_from_format("Y-m-d", "2020-06-15");
    $out[] = ($d instanceof DateTime ? "DT" : "?") . " " . $d->format("Y-m-d");
    $i = date_create_immutable_from_format("d/m/Y", "15/06/2020");
    $out[] = ($i instanceof DateTimeImmutable ? "DTI" : "?") . " " . $i->format("Y-m-d");
    $out[] = var_export(date_create_from_format("Y-m-d", "garbage"), true);
    $t = date_create_from_format("Y-m-d H:i", "2020-01-01 12:00", new DateTimeZone("UTC"));
    $out[] = $t->format("Y-m-d H:i:s");
    $out[] = date_create_from_format("!Y-m-d", "2021-03-04")->format("Y-m-d H:i:s");
    $out[] = date_create_immutable_from_format("!d-m-Y H:i", "04-03-2021 08:30")->format("Y-m-d H:i:s");
    return $out;
}
echo implode("\n", dcffCases()), "\n";
--EXPECT--
DT 2020-06-15
DTI 2020-06-15
false
2020-01-01 12:00:00
2021-03-04 00:00:00
2021-03-04 08:30:00
--CLEAN--
<?php
