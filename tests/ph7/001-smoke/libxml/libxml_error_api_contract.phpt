--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
libxml_use_internal_errors previous-state return, empty queue, clear
--FILE--
<?php
// No-arg (and null) calls report without changing state
var_dump(libxml_use_internal_errors());
// Flipping returns the PREVIOUS state
var_dump(libxml_use_internal_errors(true));
var_dump(libxml_use_internal_errors());
var_dump(libxml_use_internal_errors(null));
var_dump(libxml_use_internal_errors(false));
// Queue starts empty; last error unset; clear returns null
var_dump(libxml_get_errors());
var_dump(libxml_get_last_error());
var_dump(libxml_clear_errors());
?>
--EXPECT--
bool(false)
bool(false)
bool(true)
bool(true)
bool(true)
array(0) {
}
bool(false)
NULL
--CLEAN--
<?php
libxml_use_internal_errors(false);
libxml_clear_errors();
