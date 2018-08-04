--TEST--
Check for vapor template constructor
--SKIPIF--
<?php if (!extension_loaded("vapor")) {
    print "skip";
}
?>
--FILE--
<?php
$t = new Vapor\Template('index');
var_dump($t);
?>
--EXPECT--
object(Vapor\Template)#1 (0) {
}
