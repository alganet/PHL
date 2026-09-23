--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
metaphone max_phonemes bound and argument contract
--FILE--
<?php
// $max_phonemes bounds the main loop; 0 means unbounded; a phoneme that adds
// two codes ('X' -> KS) may run one past the bound, php's own behaviour.
var_dump(metaphone("Thompson", 3));
var_dump(metaphone("Thompson", 100));
var_dump(metaphone("box", 2));
var_dump(metaphone("aggregate", 1));
var_dump(metaphone("thick", "2"));
try { metaphone("x", -1); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
try { metaphone(); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { metaphone([1]); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { metaphone("a", "b"); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { metaphone("a", 1e19); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
string(3) "0MP"
string(5) "0MPSN"
string(3) "BKS"
string(1) "A"
string(2) "0K"
metaphone(): Argument #2 ($max_phonemes) must be greater than or equal to 0
metaphone() expects at least 1 argument, 0 given
metaphone(): Argument #1 ($string) must be of type string, array given
metaphone(): Argument #2 ($max_phonemes) must be of type int, string given
metaphone(): Argument #2 ($max_phonemes) must be of type int, float given
