--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PHP_URL_PASS has php's value and selects the pass
--FILE--
<?php
// Assert the VALUE, not merely that it is numeric: these constants are also
// passed to parse_url() as literals, so a shifted numbering silently returns
// the wrong component.
var_dump(PHP_URL_PASS);
var_dump(parse_url('http://user:pw@host:8080/pa?qu#fr', PHP_URL_PASS));
?>
--EXPECT--
int(4)
string(2) "pw"
--CLEAN--
<?php
