--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_ends_with throws TypeError for array/object args
--FILE--
<?php
class NoToStringEndsWith {}
try { str_ends_with(array(), "x"); } catch (TypeError $e) { echo "h_arr:" . $e->getMessage() . "\n"; }
try { str_ends_with("x", array()); } catch (TypeError $e) { echo "n_arr:" . $e->getMessage() . "\n"; }
try { str_ends_with(new NoToStringEndsWith(), "x"); } catch (TypeError $e) { echo "h_obj:" . $e->getMessage() . "\n"; }
try { str_ends_with("x", new NoToStringEndsWith()); } catch (TypeError $e) { echo "n_obj:" . $e->getMessage() . "\n"; }
?>
--EXPECT--
h_arr:str_ends_with(): Argument #1 ($haystack) must be of type string, array given
n_arr:str_ends_with(): Argument #2 ($needle) must be of type string, array given
h_obj:str_ends_with(): Argument #1 ($haystack) must be of type string, NoToStringEndsWith given
n_obj:str_ends_with(): Argument #2 ($needle) must be of type string, NoToStringEndsWith given
--CLEAN--
<?php

