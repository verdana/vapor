--TEST--
Check for vapor extension presence
--SKIPIF--
<?php if (!extension_loaded("vapor")) {
    print "skip";
}
?>
--FILE--
<?php
echo "vapor extension is available";
?>
--EXPECT--
vapor extension is available
