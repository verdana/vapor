--TEST--
Check for vapor presence
--SKIPIF--
<?php if (!extension_loaded("vapor")) print "skip"; ?>
--FILE--
<?php
echo "vapor extension is available";
?>
--EXPECT--
vapor extension is available
