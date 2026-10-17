<?php
// A second CGI (bonus): run by php-cgi.
header("Content-Type: text/html; charset=utf-8");
$name = isset($_GET["name"]) ? $_GET["name"] : "world";
echo "<!DOCTYPE html><html><body><h1>Hello from PHP, ",
     htmlspecialchars($name), "!</h1>";
echo "<p>Method: ", htmlspecialchars($_SERVER["REQUEST_METHOD"]), "</p>";
echo '<p><a href="/">Home</a></p></body></html>';
