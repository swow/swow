[English](./README.md) | [中文](./README-CN.md) | 日本語

<h2 align="center">
<a href="https://github.com/swow/swow"><img width="320" height="86" alt="Swow Logo" src="https://docs.toast.run/assets/images/swow.svg" /></a><br />
🚀 Swowは、並行I/Oに焦点を当てたマルチプラットフォーム対応のコルーチンベースのエンジンです
</h2>

[![license][license-badge]][license-link]
[![ci][ci-badge]][ci-link]
[![codecov][codecov-badge]][codecov-link]
[![release][release-badge]][release-link]
![❤️][made-with-love-badge]
![php][supported-php-versions-badge]
![platform][supported-platforms-badge]
![architecture][supported-architectures-badge]

## 👾 デザイン哲学

Swowは、最小のCコアとほとんどのPHPコードを使用してPHPの高性能ネットワークプログラミングをサポートすることに専念しており、重要なパフォーマンスを確保しながら、PHPコードとCカーネルをシームレスに統合することで強力な二次開発機能を提供します。

さらに、さまざまなデバッグメカニズムと強力で低いハードルのデバッグツールを提供し、開発者がバグのトラブルから解放されるようにし、ツールの助けを借りて、通常の開発者でも専門家レベルのデバッグ能力を持つことができるようにします。これにより、開発効率を最大化します。

## 🎮 インストール

> 他のオープンソースプロジェクトと同様に、Swowは常に**最新のリリース**で最も強力な安定性と機能を提供します。最新バージョンを使用していることを確認してください。

### 🔎 必要条件

- Linux、Windows、macOSなどの一般的なオペレーティングシステム。詳細は[libuv](https://github.com/libuv/libuv/blob/v1.x/SUPPORTED_PLATFORMS.md)を参照してください。
- PHP 8.0.0以上。最新バージョンを推奨します。

### 👨‍🎤 Composerによるインストール

ソースコードを取得します：

```shell
composer require swow/swow
```

次に、`vendor/bin`ディレクトリにある自動インストーラー`swow-builder`を実行してインストールを開始します：

```shell
./vendor/bin/swow-builder --install
```

Swowのソースコードを更新した後、`rebuild`オプションを使用してSwowを再コンパイルし、インストールする必要があります：

```shell
./vendor/bin/swow-builder --rebuild --install
```

`swow-builder`の詳細については、[拡張機能のインストールドキュメント - Composerによるインストール](https://docs.toast.run/swow/en/install.html)を参照してください。

### 🐧 手動インストール（UNIX系）

Swowをクローンします（Composerを使用してインポートすることもできます。その後、`vendor/swow/swow/ext`に移動して手動でインストールします）：

```shell
git clone https://github.com/swow/swow.git
```

おなじみのビルド手順。root権限でシステムにインストールします：

```shell
cd swow/ext && \
phpize && ./configure && make && \
sudo make install
```

### Ⓜ️ 手動インストール（Windows）

[インストールドキュメント - 手動コンパイルインストール（Windows）](https://docs.toast.run/swow/en/install.html#manually-build-and-install-windows)を参照してください。

また、[最新リリース](https://github.com/swow/swow/releases/latest)で直接DLLをダウンロードすることもできます。

## 🔰️ セキュリティ問題

セキュリティ問題は、[twosee@php.net](mailto:twosee@php.net)にメールで非公開に報告する必要があります。
24時間以内に返信が届くはずです。
何らかの理由で返信が届かない場合は、元のメッセージを受信したことを確認するために、メールでフォローアップしてください。

## 🖊️ 貢献

Swowの開発への貢献を歓迎します！

以下の方法で貢献できます：

* [問題の報告とフィードバック](https://github.com/swow/swow/issues)
* [修正、機能のプルリクエストの提出](https://github.com/swow/swow/pulls)
* [GitHubを通じてドキュメントの作成/修正](https://github.com/toastrun/docs.toast.run)

## ❤️ 貢献者

PHPの高性能コルーチンネットワーク通信エンジンの開発は非常に困難であり、孤独なオープンソース作業です。

Swowプロジェクトへのコード貢献に感謝します。皆さんのおかげで現在のSwowがあります。

[![Contributors](https://opencollective.com/swow/contributors.svg?width=890&button=false)](https://github.com/swow/swow/graphs/contributors)

## 💬 開発と議論

- **Wiki**：[https://docs.toast.run/swow/en](https://docs.toast.run/swow/en)
- **ブログ（CHS）** [https://docs.toast.run/swow-blog/chs](https://docs.toast.run/swow-blog/chs)
- **機能（CHS）** [https://docs.toast.run/swow-blog/chs/init.html](https://docs.toast.run/swow-blog/chs/init.html)
- **APIリファレンス**：[https://docs.toast.run/swow-api/ci.html](https://docs.toast.run/swow-api/ci.html)
- **ディスカッション**：[https://github.com/swow/swow/discussions](https://github.com/swow/swow/discussions)
- **TODO**：[https://github.com/swow/swow/projects](https://github.com/swow/swow/projects)

## 📃 ライセンス

Swowは[Apache License Version 2.0](http://www.apache.org/licenses/LICENSE-2.0.html)の下で利用可能です。
Swowには、さまざまなライセンスで利用可能な外部ライブラリも含まれています。
完全なライセンステキストについては、[LICENSES.full](LICENSES.full)を参照してください。

[license-badge]: https://img.shields.io/badge/license-apache2-blue.svg
[license-link]: LICENSE
[ci-badge]: https://github.com/swow/swow/workflows/tests/badge.svg
[ci-link]: https://github.com/swow/swow/actions?query=workflow:tests
[codecov-badge]: https://codecov.io/gh/swow/swow/branch/develop/graph/badge.svg
[codecov-link]: https://codecov.io/gh/swow/swow
[release-badge]: https://img.shields.io/github/release/swow/swow.svg?include_prereleases
[release-link]: https://github.com/swow/swow/releases
[made-with-love-badge]: https://img.shields.io/badge/made%20with-%E2%9D%A4-f00
[supported-php-versions-badge]: https://img.shields.io/badge/php-8.0--8.3-royalblue.svg
[supported-platforms-badge]: https://img.shields.io/badge/platform-Win32%20|%20GNU/Linux%20|%20macOS%20|%20FreeBSD%20-gold
[supported-architectures-badge]: https://img.shields.io/badge/architecture-x86--64%20|%20ARM64%20|%20mips64el%20|%20riscv64%20-maroon
