[English](./README.md) | [简体中文](./README-CHS.md) | [繁體中文](./README-CHT.md) | [日本語](./README-JP.md) | [Português](./README-PT.md) | Русский

<h2 align="center">
<a href="https://github.com/swow/swow"><img width="320" height="86" alt="Swow Logo" src="https://docs.toast.run/assets/images/swow.svg" /></a><br />
🚀 Swow — кроссплатформенный движок на корутинах, ориентированный на конкурентный ввод-вывод
</h2>

[![license][license-badge]][license-link]
[![ci][ci-badge]][ci-link]
[![codecov][codecov-badge]][codecov-link]
[![release][release-badge]][release-link]
![❤️][made-with-love-badge]
![php][supported-php-versions-badge]
![platform][supported-platforms-badge]
![architecture][supported-architectures-badge]

## 👾 Философия проекта

Swow стремится поддерживать высокопроизводительное сетевое программирование на PHP с помощью минимального ядра на C и кода, большая часть которого написана на PHP.
Поэтому, обеспечивая производительность в критичных местах, он в первую очередь даёт мощные возможности для расширения и доработки за счёт бесшовной совместной работы PHP-кода и ядра на C.

Кроме того, он предоставляет разнообразные механизмы отладки и мощные отладочные инструменты с низким порогом входа,
которые максимально избавляют разработчиков от проблем, вызванных ошибками.
С помощью этих инструментов обычные разработчики тоже могут отлаживать код почти на уровне экспертов,
что позволяет добиться максимальной эффективности разработки.

## ✨ Возможности

**Пишите обычный PHP-код в блокирующем стиле. Swow делает его конкурентным, а начиная с версии 2.0 он работает на всех ядрах процессора.**

| | Возможность | Что вы получаете |
| --- | --- | --- |
| 🧶 | **Корутины под вашим полным контролем** | Стековые корутины (`Coroutine`) с `Channel`, `WaitGroup`, `WaitReference`, `Selector` и `Defer`. Любую корутину можно в любой момент осмотреть, прервать или завершить |
| 🪄 | **Ваш существующий код становится конкурентным** | Всё перечисленное ниже и не только уступает управление другим корутинам, а не блокирует процесс. Менять код не нужно:<br>**Таймеры**: `sleep()`, `usleep()`, `time_nanosleep()`, `time_sleep_until()`<br>**DNS**: `gethostbyname()`<br>**Файлы и каналы (pipe)**: `fopen()`, `fread()`, `fwrite()`, `file_get_contents()`, `flock()` и остальные файловые функции<br>**Потоки PHP (streams)**: TCP, UDP, Unix-сокеты и TLS, включая `fsockopen()`, `stream_socket_client()`, `stream_socket_server()`, `stream_select()` и `file_get_contents()` по HTTP(S)<br>**cURL**: `curl_exec()`, `curl_multi_*` и HTTP/2, а значит, Guzzle и другие HTTP-клиенты на основе cURL<br>**MySQL**: PDO MySQL и `mysqli` через `mysqlnd`<br>**PostgreSQL**: PDO PgSQL<br>**MongoDB** <sup>2.0</sup>: официальное расширение `mongodb`<br>**Redis**: клиенты, построенные на потоках PHP, например phpredis и Predis<br>**Дочерние процессы**: `proc_open()`<br>**И не только**: любое расширение или библиотека на чистом PHP, построенные на перечисленном выше |
| 🌐 | **Полноценный сетевой стек** | `Socket` охватывает TCP, UDP, Unix-сокеты, каналы (pipe) и TLS; встроенные парсеры HTTP и WebSocket, а также написанные на PHP серверы и клиенты HTTP и WebSocket, совместимые с PSR-7 |
| 🧵 | **Настоящая многопоточность** <sup>2.0</sup> | `Thread` выполняет PHP в потоках операционной системы; в комплекте `Thannel` (канал между потоками), `Mutex`, `Semaphore`, `Futex`, `SharedBuffer` (разделяемая память) и `ThreadManager` для рабочих потоков. Требуется потокобезопасная сборка PHP |
| 🏭 | **Многопроцессность на любой ОС** <sup>2.0</sup> | `ChildProcess` и `ProcessManager`, который перезапускает и перезагружает воркеры, в том числе на Windows |
| 🍃 | **Драйвер MongoDB с поддержкой корутин** <sup>2.0</sup> | Официальное расширение `mongodb` выполняет ввод-вывод асинхронно и больше не блокирует процесс. Собирайте с `--enable-swow-mongodb` |
| 🐞 | **SDB, Swow Debugger** | Интерактивный отладчик, написанный на PHP, с командами в стиле gdb: список всех корутин, просмотр стека вызовов и исходного кода, точки останова, пошаговое выполнение и вывод переменных |
| 🐶 | **Watchdog** | Обнаруживает корутины, которые занимают процессор или блокируются в системном вызове. Он может заставить такую корутину уступить управление остальным или передать её вашему обработчику, чтобы приостановить или завершить её |
| 🔬 | **Другие средства отладки** | Обнаружение взаимных блокировок и трассировка выполнения |

Все новшества версии 2.0 смотрите в [CHANGELOG](./CHANGELOG.md).

## 🎮 Установка

> Как и любой проект с открытым исходным кодом, Swow обеспечивает наилучшую стабильность и возможности в **самом новом релизе**. Постарайтесь использовать последнюю версию.

### 🔎 Требования

- Распространённые операционные системы: Linux, Windows, macOS и другие. Почти те же, что поддерживает [libuv](https://github.com/libuv/libuv/blob/v1.x/SUPPORTED_PLATFORMS.md)
- PHP 8.0.0 или выше, рекомендуется последняя версия
- Потокобезопасная (ZTS) сборка PHP, если вы хотите использовать средства многопоточности

### 🥧 Установка через PIE

Сначала установите [PIE](https://github.com/php/pie).

```shell
pie install swow/swow-extension
```

Версия для разработки (может содержать свежие исправления):

```shell
pie install swow/swow-extension:dev-develop
```

### 👨‍🎤 Установка через Composer

Загрузите исходный код:

```shell
composer require swow/swow
```

Затем запустите автоматический установщик `swow-builder` из каталога `vendor/bin`, чтобы начать установку:

```shell
./vendor/bin/swow-builder --install
```

После обновления исходного кода Swow используйте опцию `rebuild`, чтобы пересобрать и заново установить его:

```shell
./vendor/bin/swow-builder --rebuild --install
```

Подробнее о `swow-builder` — в [документации по установке расширения, раздел о Composer](https://docs.toast.run/swow/en/install.html).

### 🐧 Ручная установка (UNIX-подобные системы)

Клонируйте Swow (его также можно подключить через Composer, затем перейти в `vendor/swow/swow/ext` и установить вручную):

```shell
git clone https://github.com/swow/swow.git
```

Дальше обычная процедура сборки. Установка в систему с правами root:

```shell
cd swow/ext && \
phpize && ./configure && \
make clean && make -j && \
sudo make install
```

См. [документацию по установке — ручная сборка и установка (UNIX-подобные системы)](https://docs.toast.run/swow/en/install.html#manually-build-and-install-unix-like-or-cygwin-msys-wsl).

### Ⓜ️ Ручная установка (Windows)

См. [документацию по установке — ручная сборка и установка (Windows)](https://docs.toast.run/swow/en/install.html#manually-build-and-install-windows).

DLL также можно скачать напрямую на странице [последнего релиза](https://github.com/swow/swow/releases/latest).

## 🔰️ Вопросы безопасности

О проблемах безопасности следует сообщать в частном порядке по электронной почте: [twosee@php.net](mailto:twosee@php.net).
Вы должны получить ответ в течение 24 часов.
Если по какой-то причине ответа нет, напишите ещё раз, чтобы убедиться, что мы получили исходное сообщение.

## 🖊️ Участие в разработке

Мы будем очень рады вашему вкладу в развитие Swow!

Вы можете помочь следующими способами:

* [Сообщать о проблемах и оставлять отзывы](https://github.com/swow/swow/issues)
* [Отправлять исправления и новые возможности через Pull Request](https://github.com/swow/swow/pulls)
* [Писать и улучшать документацию на GitHub](https://github.com/toastrun/docs.toast.run)

## ❤️ Участники

Разработка высокопроизводительного сетевого движка на корутинах для PHP — мелодия столь высокая, что подпевать ей могут немногие.
Это работа над открытым кодом, в которой на вершине одиноко.

Большое спасибо следующим друзьям за вклад в код проекта Swow.
Без вас сегодняшнего Swow не было бы.

[![Contributors](https://opencollective.com/swow/contributors.svg?width=890&button=false)](https://github.com/swow/swow/graphs/contributors)

## 💬 Разработка и обсуждение

- **Документация (на английском)**: [https://docs.toast.run/swow/en](https://docs.toast.run/swow/en)
- **Блог (на китайском)**: [https://docs.toast.run/swow-blog/chs](https://docs.toast.run/swow-blog/chs)
- **Возможности (на китайском)**: [https://docs.toast.run/swow-blog/chs/init.html](https://docs.toast.run/swow-blog/chs/init.html)
- **Справочник по API**: [https://docs.toast.run/swow-api/develop/](https://docs.toast.run/swow-api/develop/)
- **Обсуждения**: [https://github.com/swow/swow/discussions](https://github.com/swow/swow/discussions)
- **TODO**: [https://github.com/swow/swow/projects](https://github.com/swow/swow/projects)

## 📃 Лицензия

Swow распространяется по лицензии [Apache License Version 2.0](http://www.apache.org/licenses/LICENSE-2.0.html).
Swow также включает сторонние библиотеки, распространяемые по различным лицензиям.
Полный текст лицензий см. в [LICENSES.full](LICENSES.full).

[license-badge]: https://img.shields.io/badge/license-apache2-blue.svg
[license-link]: LICENSE
[ci-badge]: https://github.com/swow/swow/workflows/tests/badge.svg
[ci-link]: https://github.com/swow/swow/actions?query=workflow:tests
[codecov-badge]: https://codecov.io/gh/swow/swow/branch/develop/graph/badge.svg
[codecov-link]: https://codecov.io/gh/swow/swow
[release-badge]: https://img.shields.io/github/release/swow/swow.svg?include_prereleases
[release-link]: https://github.com/swow/swow/releases
[made-with-love-badge]: https://img.shields.io/badge/made%20with-%E2%9D%A4-f00
[supported-php-versions-badge]: https://img.shields.io/badge/php-8.0--8.5-royalblue.svg
[supported-platforms-badge]: https://img.shields.io/badge/platform-Win32%20|%20GNU/Linux%20|%20macOS%20|%20FreeBSD%20-gold
[supported-architectures-badge]: https://img.shields.io/badge/architecture-x86--64%20|%20ARM64%20|%20mips64el%20|%20riscv64%20-maroon
