<?php

class PDOFactoryTest {
    static function create($classname = 'PDO') {
        $host = TEST_POSTGRES_HOST;
        $port = TEST_POSTGRES_PORT;
        $user = TEST_POSTGRES_USER;
        $password = TEST_POSTGRES_PASSWORD;
        $dbname = TEST_POSTGRES_DBNAME;

        $dsn = "pgsql:host={$host};port={$port};dbname={$dbname}";

        $db = new $classname($dsn, $user, $password);

        if (!$db) {
            die("Could not create PDO object (DSN=$dsn, user=$user)\n");
        }
        return $db;
    }
}
