--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
var_export() arrays: lists, mixed keys, nesting, indentation
--FILE--
<?php

var_export([1,2,3]); echo "\n";
var_export([5=>"a","k"=>"b"]); echo "\n";
var_export([1,"k"=>[true,null,"x"]]); echo "\n";
var_export([]); echo "\n";
var_export(["n"=>null,"b"=>false,"f"=>1.5,"deep"=>[[1],[2,3]]]); echo "\n";
?>
--EXPECT--
array (
  0 => 1,
  1 => 2,
  2 => 3,
)
array (
  5 => 'a',
  'k' => 'b',
)
array (
  0 => 1,
  'k' => 
  array (
    0 => true,
    1 => NULL,
    2 => 'x',
  ),
)
array (
)
array (
  'n' => NULL,
  'b' => false,
  'f' => 1.5,
  'deep' => 
  array (
    0 => 
    array (
      0 => 1,
    ),
    1 => 
    array (
      0 => 2,
      1 => 3,
    ),
  ),
)
