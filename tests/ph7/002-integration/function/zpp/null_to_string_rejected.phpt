--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Passing null to a non-nullable string builtin is a TypeError (php deprecates; PHL removes)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip PHL removes what php only deprecates'; ?>
--FILE--
<?php
// php 8.1 only DEPRECATES passing null to a non-nullable internal string parameter;
// PHL targets php's non-deprecated surface and rejects it with a TypeError.
try { strlen(null); }        catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { substr(null, 0); }     catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { strtoupper(null); }    catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { trim(null); }          catch (TypeError $e) { echo $e->getMessage(), "\n"; }
// a real string still works
echo strlen("ok"), "\n";
?>
--EXPECT--
strlen(): Argument #1 ($string) must be of type string, null given
substr(): Argument #1 ($string) must be of type string, null given
strtoupper(): Argument #1 ($string) must be of type string, null given
trim(): Argument #1 ($string) must be of type string, null given
2
