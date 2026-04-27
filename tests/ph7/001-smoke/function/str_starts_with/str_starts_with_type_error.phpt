--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_starts_with throws TypeError for array/object args
--FILE--
<?php
class NoToStringStartsWith {}
try { str_starts_with(array(), "x"); } catch (TypeError $e) { echo "h_arr:" . $e->getMessage() . "\n"; }
try { str_starts_with("x", array()); } catch (TypeError $e) { echo "n_arr:" . $e->getMessage() . "\n"; }
try { str_starts_with(new NoToStringStartsWith(), "x"); } catch (TypeError $e) { echo "h_obj:" . $e->getMessage() . "\n"; }
try { str_starts_with("x", new NoToStringStartsWith()); } catch (TypeError $e) { echo "n_obj:" . $e->getMessage() . "\n"; }
?>
--EXPECT--
h_arr:str_starts_with(): Argument #1 ($haystack) must be of type string, array given
n_arr:str_starts_with(): Argument #2 ($needle) must be of type string, array given
h_obj:str_starts_with(): Argument #1 ($haystack) must be of type string, NoToStringStartsWith given
n_obj:str_starts_with(): Argument #2 ($needle) must be of type string, NoToStringStartsWith given
--CLEAN--
<?php

