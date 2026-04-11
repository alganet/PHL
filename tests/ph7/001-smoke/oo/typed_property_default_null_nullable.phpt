--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: nullable with null default reads as null
--FILE--
<?php
class TpCfg {
    public ?string $host = null;
    public ?int $port = null;
}
$c = new TpCfg();
echo is_null($c->host) ? "null" : $c->host, "\n";
echo is_null($c->port) ? "null" : $c->port, "\n";
$c->host = "localhost";
$c->port = 8080;
echo $c->host, ":", $c->port, "\n";
?>
--EXPECT--
null
null
localhost:8080
--CLEAN--
<?php
unset($c);
