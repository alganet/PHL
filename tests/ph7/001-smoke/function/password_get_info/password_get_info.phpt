--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
password_get_info reports the bcrypt algo/cost, and null/unknown for an unrecognized hash
--FILE--
<?php
$i = password_get_info(password_hash("x", PASSWORD_BCRYPT, ["cost" => 10]));
echo $i["algo"], "|", $i["algoName"], "|", $i["options"]["cost"], "\n";
$g = password_get_info("not a valid hash");
echo ($g["algo"] === null ? "null" : "notnull"), "|", $g["algoName"], "|", count($g["options"]), "\n";
?>
--EXPECT--
2y|bcrypt|10
null|unknown|0
--CLEAN--
<?php
