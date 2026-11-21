[English](./README.md) | [简体中文](./README-CHS.md) | [繁體中文](./README-CHT.md) | [日本語](./README-JP.md) | Português | [Русский](./README-RU.md)

<h2 align="center">
<a href="https://github.com/swow/swow"><img width="320" height="86" alt="Swow Logo" src="https://docs.toast.run/assets/images/swow.svg" /></a><br />
🚀 Swow é um motor multiplataforma baseado em corrotinas, focado em I/O concorrente
</h2>

[![license][license-badge]][license-link]
[![ci][ci-badge]][ci-link]
[![codecov][codecov-badge]][codecov-link]
[![release][release-badge]][release-link]
![❤️][made-with-love-badge]
![php][supported-php-versions-badge]
![platform][supported-platforms-badge]
![architecture][supported-architectures-badge]

## 👾 Filosofia de design

O Swow se compromete a usar o menor núcleo em C possível e a maior parte do código em PHP para dar suporte à programação de rede de alto desempenho em PHP.
Por isso, além de garantir o desempenho nos pontos críticos, ele se dedica principalmente a oferecer uma poderosa capacidade de desenvolvimento secundário, integrando de forma transparente o código PHP e o núcleo em C.

Além disso, ele fornece uma variedade de mecanismos de depuração e ferramentas de depuração poderosas e de fácil acesso,
que mantêm os desenvolvedores o mais longe possível dos problemas causados por bugs.
Com a ajuda dessas ferramentas, desenvolvedores comuns também conseguem depurar em um nível próximo ao de especialistas,
maximizando assim a eficiência do desenvolvimento.

## ✨ Recursos

**Escreva PHP comum, em estilo bloqueante. O Swow o torna concorrente e, desde a versão 2.0, ele roda em todos os núcleos da CPU.**

| | Capacidade | O que você ganha |
| --- | --- | --- |
| 🧶 | **Corrotinas sob seu controle total** | Corrotinas stackful (`Coroutine`) com `Channel`, `WaitGroup`, `WaitReference`, `Selector` e `Defer`. Inspecione, interrompa ou mate qualquer corrotina a qualquer momento |
| 🪄 | **Seu código existente, agora concorrente** | Tudo o que está listado abaixo, e mais, cede a vez a outras corrotinas em vez de bloquear o processo. Sem alterações no código:<br>**Temporizadores**: `sleep()`, `usleep()`, `time_nanosleep()`, `time_sleep_until()`<br>**DNS**: `gethostbyname()`<br>**Arquivos e pipes**: `fopen()`, `fread()`, `fwrite()`, `file_get_contents()`, `flock()` e as demais funções de arquivo<br>**Streams do PHP**: TCP, UDP, sockets Unix e TLS, incluindo `fsockopen()`, `stream_socket_client()`, `stream_socket_server()`, `stream_select()` e `file_get_contents()` sobre HTTP(S)<br>**cURL**: `curl_exec()`, `curl_multi_*` e HTTP/2, o que cobre o Guzzle e outros clientes HTTP baseados em cURL<br>**MySQL**: PDO MySQL e `mysqli`, por meio do `mysqlnd`<br>**PostgreSQL**: PDO PgSQL<br>**MongoDB** <sup>2.0</sup>: a extensão oficial `mongodb`<br>**Redis**: clientes construídos sobre streams do PHP, como phpredis e Predis<br>**Processos filhos**: `proc_open()`<br>**E mais**: qualquer extensão ou biblioteca em PHP puro construída sobre os itens acima |
| 🌐 | **Uma pilha de rede completa** | `Socket` cobre TCP, UDP, sockets Unix, pipes e TLS, com parsers de HTTP e WebSocket embutidos, além de servidores e clientes HTTP e WebSocket compatíveis com PSR-7, escritos em PHP |
| 🧵 | **Multithreading de verdade** <sup>2.0</sup> | `Thread` executa PHP em threads do sistema operacional, com `Thannel` (um canal entre threads), `Mutex`, `Semaphore`, `Futex`, `SharedBuffer` (memória compartilhada) e um `ThreadManager` para threads de trabalho. Requer uma build thread-safe do PHP |
| 🏭 | **Multiprocessamento em todos os sistemas operacionais** <sup>2.0</sup> | `ChildProcess` e um `ProcessManager` que reinicia e recarrega workers, também no Windows |
| 🍃 | **Driver MongoDB compatível com corrotinas** <sup>2.0</sup> | A extensão oficial `mongodb` passa a fazer seu I/O de forma assíncrona e não bloqueia mais o processo. Compile com `--enable-swow-mongodb` |
| 🐞 | **SDB, o Swow Debugger** | Um depurador interativo escrito em PHP, com comandos no estilo do gdb: liste todas as corrotinas, leia backtraces e código-fonte, defina breakpoints, avance passo a passo e imprima variáveis |
| 🐶 | **Watchdog** | Detecta corrotinas que monopolizam a CPU ou que bloqueiam em uma chamada de sistema. Ele pode fazer a corrotina ceder a vez às outras ou entregá-la ao seu callback para suspendê-la ou matá-la |
| 🔬 | **Mais ferramentas de depuração** | Detecção de deadlock e rastreamento de execução |

Veja todas as novidades da versão 2.0 no [CHANGELOG](./CHANGELOG.md).

## 🎮 Instalação

> Como qualquer projeto de código aberto, o Swow sempre oferece a maior estabilidade e os melhores recursos na **versão mais recente**. Procure garantir que você está usando a versão mais nova.

### 🔎 Requisitos

- Sistemas operacionais comuns, como Linux, Windows, macOS etc. Praticamente os mesmos suportados pela [libuv](https://github.com/libuv/libuv/blob/v1.x/SUPPORTED_PLATFORMS.md)
- PHP 8.0.0 ou superior; recomenda-se a versão mais recente
- Uma build thread-safe (ZTS) do PHP, caso você queira usar os recursos de multithreading

### 🥧 Usando o PIE

Primeiro, obtenha o [PIE](https://github.com/php/pie).

```shell
pie install swow/swow-extension
```

Para a versão de desenvolvimento (pode conter correções mais recentes):

```shell
pie install swow/swow-extension:dev-develop
```

### 👨‍🎤 Pelo Composer

Baixe o código-fonte:

```shell
composer require swow/swow
```

Em seguida, execute o instalador automático `swow-builder`, no diretório `vendor/bin`, para iniciar a instalação:

```shell
./vendor/bin/swow-builder --install
```

Depois de atualizar o código-fonte do Swow, use a opção `rebuild` para recompilá-lo e instalá-lo novamente:

```shell
./vendor/bin/swow-builder --rebuild --install
```

Mais informações sobre o `swow-builder` estão no [documento de instalação da extensão - Pelo Composer](https://docs.toast.run/swow/en/install.html).

### 🐧 Instalação manual (sistemas do tipo UNIX)

Clone o Swow (você também pode importá-lo pelo Composer, entrar em `vendor/swow/swow/ext` e instalar manualmente):

```shell
git clone https://github.com/swow/swow.git
```

O procedimento de compilação é o habitual. Instale no sistema com privilégios de root:

```shell
cd swow/ext && \
phpize && ./configure && \
make clean && make -j && \
sudo make install
```

Veja o [documento de instalação - Compilação e instalação manual (sistemas do tipo UNIX)](https://docs.toast.run/swow/en/install.html#manually-build-and-install-unix-like-or-cygwin-msys-wsl).

### Ⓜ️ Instalação manual (Windows)

Veja o [documento de instalação - Compilação e instalação manual (Windows)](https://docs.toast.run/swow/en/install.html#manually-build-and-install-windows).

Você também pode baixar a DLL diretamente na [versão mais recente](https://github.com/swow/swow/releases/latest).

## 🔰️ Problemas de segurança

Problemas de segurança devem ser reportados de forma privada, por e-mail, para [twosee@php.net](mailto:twosee@php.net).
Você deve receber uma resposta em até 24 horas.
Se por algum motivo isso não acontecer, envie outro e-mail para confirmar que recebemos a mensagem original.

## 🖊️ Contribuição

Sua contribuição para o desenvolvimento do Swow é muito bem-vinda!

Você pode contribuir das seguintes formas:

* [Reportar problemas e enviar feedback](https://github.com/swow/swow/issues)
* [Enviar correções e recursos por Pull Request](https://github.com/swow/swow/pulls)
* [Escrever ou aprimorar a documentação pelo GitHub](https://github.com/toastrun/docs.toast.run)

## ❤️ Contribuidores

Desenvolver um motor de rede de alto desempenho com corrotinas para PHP é um trabalho de nicho
e, muitas vezes, um trabalho de código aberto solitário.

Muito obrigado aos seguintes contribuidores do projeto Swow.
Sem vocês, o Swow de hoje não existiria.

[![Contributors](https://opencollective.com/swow/contributors.svg?width=890&button=false)](https://github.com/swow/swow/graphs/contributors)

## 💬 Desenvolvimento e discussão

- **Documentação (inglês)**: [https://docs.toast.run/swow/en](https://docs.toast.run/swow/en)
- **Blog (chinês)**: [https://docs.toast.run/swow-blog/chs](https://docs.toast.run/swow-blog/chs)
- **Recursos (chinês)**: [https://docs.toast.run/swow-blog/chs/init.html](https://docs.toast.run/swow-blog/chs/init.html)
- **Referência da API**: [https://docs.toast.run/swow-api/develop/](https://docs.toast.run/swow-api/develop/)
- **Discussões**: [https://github.com/swow/swow/discussions](https://github.com/swow/swow/discussions)
- **TODO**: [https://github.com/swow/swow/projects](https://github.com/swow/swow/projects)

## 📃 Licença

O Swow está disponível sob a [Apache License Version 2.0](http://www.apache.org/licenses/LICENSE-2.0.html).
O Swow também inclui bibliotecas externas disponíveis sob diversas licenças.
Veja [LICENSES.full](LICENSES.full) para o texto completo das licenças.

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
