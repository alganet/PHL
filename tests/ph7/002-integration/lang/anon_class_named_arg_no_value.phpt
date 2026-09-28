--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An anonymous class's named argument with no value is refused where it is written
--DESCRIPTION--
Teaching PH7_CompileAnnonClass to read `name: value` has to include the degenerate form the
ordinary argument path already refuses -- a name with nothing after it -- or the argument would
be dropped in silence. Same refusal, same spelling, as `h(a:)` gets (php names the token that
stopped IT instead, `syntax error, unexpected token ")"`; that wording gap belongs to the
parse-error-kind family and the pattern below matches either engine, as its `h(a:)` sibling
named_args_error_missing_value.phpt does).
--FILE--
<?php
class AcnaB { function __construct($a = 0) {} }
$o = new class(a:) extends AcnaB {};
?>
--EXPECTF--
%s Parse error:  syntax error,%A
--CLEAN--
<?php
