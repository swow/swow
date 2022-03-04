<?php

return [
    'target_php_version' => '8.0',

    'directory_list' => [
        'lib',
    ],

    'exclude_file_regex' => '@^lib/swow-stub/src/Swow.php@',

    'exclude_analysis_directory_list' => [
        'vendor/'
    ],
    'autoload_internal_extension_signatures' => [
        'swow' => 'lib/swow-stub/src/Swow.php'
    ]
];
