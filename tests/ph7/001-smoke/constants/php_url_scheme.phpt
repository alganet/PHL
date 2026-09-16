--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PHP_URL_SCHEME has php's value and selects the scheme
--FILE--
<?php
// Assert the VALUE, not merely that it is numeric: these constants are also
// passed to parse_url() as literals, so a shifted numbering silently returns
// the wrong component.
var_dump(PHP_URL_SCHEME);
var_dump(parse_url('http://user:pw@host:8080/pa?qu#fr', PHP_URL_SCHEME));
?>
--EXPECT--
int(0)
string(4) "http"
--CLEAN--
<?php
