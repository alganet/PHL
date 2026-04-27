--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_contains throws TypeError for array/object args
--FILE--
<?php
class NoToStringContains {}
try { str_contains(array(), "x"); } catch (TypeError $e) { echo "h_arr:" . $e->getMessage() . "\n"; }
try { str_contains("x", array()); } catch (TypeError $e) { echo "n_arr:" . $e->getMessage() . "\n"; }
try { str_contains(new NoToStringContains(), "x"); } catch (TypeError $e) { echo "h_obj:" . $e->getMessage() . "\n"; }
try { str_contains("x", new NoToStringContains()); } catch (TypeError $e) { echo "n_obj:" . $e->getMessage() . "\n"; }
?>
--EXPECT--
h_arr:str_contains(): Argument #1 ($haystack) must be of type string, array given
n_arr:str_contains(): Argument #2 ($needle) must be of type string, array given
h_obj:str_contains(): Argument #1 ($haystack) must be of type string, NoToStringContains given
n_obj:str_contains(): Argument #2 ($needle) must be of type string, NoToStringContains given
--CLEAN--
<?php

