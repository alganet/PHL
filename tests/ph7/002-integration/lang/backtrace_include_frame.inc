<?php
function bifShow() { echo str_replace([__DIR__ . '/', __DIR__ . '\\'], '', (new Exception)->getTraceAsString()), "\n"; }
echo 'top: ', str_replace("\n", ' | ', str_replace([__DIR__ . '/', __DIR__ . '\\'], '', (new Exception)->getTraceAsString())), "\n";
bifShow();
