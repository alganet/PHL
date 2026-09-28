--TEST--
Reflection: the whole Extension block, php's sections and its nesting
--FILE--
<?php
// The extension export printed one placeholder line here -- `Extension [
// extension #1 name ]` -- because it NESTS the function and class blocks and
// could not match php until those did. It is php's now: each section only when
// it has rows, in php's order, each preceded by a blank line, and a nested
// block the STANDALONE export with four spaces on every non-empty line.
// The head's module NUMBER and version belong to the build, so they are the
// one part normalised away here.
$reflXShow = function (string $reflXName) {
    $reflXS = (string)new ReflectionExtension($reflXName);
    echo preg_replace('/^Extension \[ <persistent> extension #\d+ (\S+) version \S+ \]/',
        'Extension [ <persistent> extension #N $1 version V ]', $reflXS, 1);
};
// Functions only, and the section carries no count.
$reflXShow('ctype');
// Constants, functions and classes, with a blank line between class blocks.
$reflXShow('json');
echo preg_replace('/#\d+ (\S+) version \S+/', '#N $1 version V',
    strtok((string)new ReflectionExtension('tokenizer'), "\n")), "\n";
// The sections an extension has, and the ones it does not.
foreach (['Core', 'json', 'ctype', 'tokenizer', 'Reflection', 'SPL'] as $reflXE) {
    $reflXR = new ReflectionExtension($reflXE);
    $reflXSec = [];
    foreach (explode("\n", (string)$reflXR) as $reflXL) {
        if (preg_match('/^  - ([A-Za-z]+)/', $reflXL, $reflXM)) { $reflXSec[] = $reflXM[1]; }
    }
    printf("%-12s %s\n", $reflXE, implode(',', $reflXSec));
}
--EXPECT--
Extension [ <persistent> extension #N ctype version V ] {

  - Functions {
    Function [ <internal:ctype> function ctype_alnum ] {

      - Parameters [1] {
        Parameter #0 [ <required> mixed $text ]
      }
      - Return [ bool ]
    }
    Function [ <internal:ctype> function ctype_alpha ] {

      - Parameters [1] {
        Parameter #0 [ <required> mixed $text ]
      }
      - Return [ bool ]
    }
    Function [ <internal:ctype> function ctype_cntrl ] {

      - Parameters [1] {
        Parameter #0 [ <required> mixed $text ]
      }
      - Return [ bool ]
    }
    Function [ <internal:ctype> function ctype_digit ] {

      - Parameters [1] {
        Parameter #0 [ <required> mixed $text ]
      }
      - Return [ bool ]
    }
    Function [ <internal:ctype> function ctype_lower ] {

      - Parameters [1] {
        Parameter #0 [ <required> mixed $text ]
      }
      - Return [ bool ]
    }
    Function [ <internal:ctype> function ctype_graph ] {

      - Parameters [1] {
        Parameter #0 [ <required> mixed $text ]
      }
      - Return [ bool ]
    }
    Function [ <internal:ctype> function ctype_print ] {

      - Parameters [1] {
        Parameter #0 [ <required> mixed $text ]
      }
      - Return [ bool ]
    }
    Function [ <internal:ctype> function ctype_punct ] {

      - Parameters [1] {
        Parameter #0 [ <required> mixed $text ]
      }
      - Return [ bool ]
    }
    Function [ <internal:ctype> function ctype_space ] {

      - Parameters [1] {
        Parameter #0 [ <required> mixed $text ]
      }
      - Return [ bool ]
    }
    Function [ <internal:ctype> function ctype_upper ] {

      - Parameters [1] {
        Parameter #0 [ <required> mixed $text ]
      }
      - Return [ bool ]
    }
    Function [ <internal:ctype> function ctype_xdigit ] {

      - Parameters [1] {
        Parameter #0 [ <required> mixed $text ]
      }
      - Return [ bool ]
    }
  }
}
Extension [ <persistent> extension #N json version V ] {

  - Constants [29] {
    Constant [ <persistent> int JSON_HEX_TAG ] { 1 }
    Constant [ <persistent> int JSON_HEX_AMP ] { 2 }
    Constant [ <persistent> int JSON_HEX_APOS ] { 4 }
    Constant [ <persistent> int JSON_HEX_QUOT ] { 8 }
    Constant [ <persistent> int JSON_FORCE_OBJECT ] { 16 }
    Constant [ <persistent> int JSON_NUMERIC_CHECK ] { 32 }
    Constant [ <persistent> int JSON_UNESCAPED_SLASHES ] { 64 }
    Constant [ <persistent> int JSON_PRETTY_PRINT ] { 128 }
    Constant [ <persistent> int JSON_UNESCAPED_UNICODE ] { 256 }
    Constant [ <persistent> int JSON_PARTIAL_OUTPUT_ON_ERROR ] { 512 }
    Constant [ <persistent> int JSON_PRESERVE_ZERO_FRACTION ] { 1024 }
    Constant [ <persistent> int JSON_UNESCAPED_LINE_TERMINATORS ] { 2048 }
    Constant [ <persistent> int JSON_OBJECT_AS_ARRAY ] { 1 }
    Constant [ <persistent> int JSON_BIGINT_AS_STRING ] { 2 }
    Constant [ <persistent> int JSON_INVALID_UTF8_IGNORE ] { 1048576 }
    Constant [ <persistent> int JSON_INVALID_UTF8_SUBSTITUTE ] { 2097152 }
    Constant [ <persistent> int JSON_THROW_ON_ERROR ] { 4194304 }
    Constant [ <persistent> int JSON_ERROR_NONE ] { 0 }
    Constant [ <persistent> int JSON_ERROR_DEPTH ] { 1 }
    Constant [ <persistent> int JSON_ERROR_STATE_MISMATCH ] { 2 }
    Constant [ <persistent> int JSON_ERROR_CTRL_CHAR ] { 3 }
    Constant [ <persistent> int JSON_ERROR_SYNTAX ] { 4 }
    Constant [ <persistent> int JSON_ERROR_UTF8 ] { 5 }
    Constant [ <persistent> int JSON_ERROR_RECURSION ] { 6 }
    Constant [ <persistent> int JSON_ERROR_INF_OR_NAN ] { 7 }
    Constant [ <persistent> int JSON_ERROR_UNSUPPORTED_TYPE ] { 8 }
    Constant [ <persistent> int JSON_ERROR_INVALID_PROPERTY_NAME ] { 9 }
    Constant [ <persistent> int JSON_ERROR_UTF16 ] { 10 }
    Constant [ <persistent> int JSON_ERROR_NON_BACKED_ENUM ] { 11 }
  }

  - Functions {
    Function [ <internal:json> function json_encode ] {

      - Parameters [3] {
        Parameter #0 [ <required> mixed $value ]
        Parameter #1 [ <optional> int $flags = 0 ]
        Parameter #2 [ <optional> int $depth = 512 ]
      }
      - Return [ string|false ]
    }
    Function [ <internal:json> function json_decode ] {

      - Parameters [4] {
        Parameter #0 [ <required> string $json ]
        Parameter #1 [ <optional> ?bool $associative = null ]
        Parameter #2 [ <optional> int $depth = 512 ]
        Parameter #3 [ <optional> int $flags = 0 ]
      }
      - Return [ mixed ]
    }
    Function [ <internal:json> function json_validate ] {

      - Parameters [3] {
        Parameter #0 [ <required> string $json ]
        Parameter #1 [ <optional> int $depth = 512 ]
        Parameter #2 [ <optional> int $flags = 0 ]
      }
      - Return [ bool ]
    }
    Function [ <internal:json> function json_last_error ] {

      - Parameters [0] {
      }
      - Return [ int ]
    }
    Function [ <internal:json> function json_last_error_msg ] {

      - Parameters [0] {
      }
      - Return [ string ]
    }
  }

  - Classes [2] {
    Interface [ <internal:json> interface JsonSerializable ] {

      - Constants [0] {
      }

      - Static properties [0] {
      }

      - Static methods [0] {
      }

      - Properties [0] {
      }

      - Methods [1] {
        Method [ <internal:json> abstract public method jsonSerialize ] {

          - Parameters [0] {
          }
          - Tentative return [ mixed ]
        }
      }
    }

    Class [ <internal:json> class JsonException extends Exception implements Throwable, Stringable ] {

      - Constants [0] {
      }

      - Static properties [0] {
      }

      - Static methods [0] {
      }

      - Properties [4] {
        Property [ protected $message = '' ]
        Property [ protected $code = 0 ]
        Property [ protected string $file = '' ]
        Property [ protected int $line = 0 ]
      }

      - Methods [10] {
        Method [ <internal:Core, inherits Exception, ctor> public method __construct ] {

          - Parameters [3] {
            Parameter #0 [ <optional> string $message = "" ]
            Parameter #1 [ <optional> int $code = 0 ]
            Parameter #2 [ <optional> ?Throwable $previous = null ]
          }
        }

        Method [ <internal:Core, inherits Exception> public method __wakeup ] {

          - Parameters [0] {
          }
          - Tentative return [ void ]
        }

        Method [ <internal:Core, inherits Exception, prototype Throwable> final public method getMessage ] {

          - Parameters [0] {
          }
          - Return [ string ]
        }

        Method [ <internal:Core, inherits Exception, prototype Throwable> final public method getCode ] {

          - Parameters [0] {
          }
        }

        Method [ <internal:Core, inherits Exception, prototype Throwable> final public method getFile ] {

          - Parameters [0] {
          }
          - Return [ string ]
        }

        Method [ <internal:Core, inherits Exception, prototype Throwable> final public method getLine ] {

          - Parameters [0] {
          }
          - Return [ int ]
        }

        Method [ <internal:Core, inherits Exception, prototype Throwable> final public method getTrace ] {

          - Parameters [0] {
          }
          - Return [ array ]
        }

        Method [ <internal:Core, inherits Exception, prototype Throwable> final public method getPrevious ] {

          - Parameters [0] {
          }
          - Return [ ?Throwable ]
        }

        Method [ <internal:Core, inherits Exception, prototype Throwable> final public method getTraceAsString ] {

          - Parameters [0] {
          }
          - Return [ string ]
        }

        Method [ <internal:Core, inherits Exception, prototype Stringable> public method __toString ] {

          - Parameters [0] {
          }
          - Return [ string ]
        }
      }
    }
  }
}
Extension [ <persistent> extension #N tokenizer version V ] {
Core         INI,Constants,Functions,Classes
json         Constants,Functions,Classes
ctype        Functions
tokenizer    Constants,Functions,Classes
Reflection   Classes
SPL          Dependencies,Functions,Classes
