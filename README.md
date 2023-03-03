<h1 align="center">tokiscript</h1>
<p align="center">A simple scripting language in C</p>

<p align="center">
	<a href="./LICENSE">
		<img alt="License" src="https://img.shields.io/badge/license-GPL v3-26c374?style=for-the-badge">
	</a>
	<a href="https://github.com/LordOfTrident/tokiscript/issues">
		<img alt="Issues" src="https://img.shields.io/github/issues/LordOfTrident/tokiscript?style=for-the-badge&color=4f79e4">
	</a>
	<a href="https://github.com/LordOfTrident/tokiscript/pulls">
		<img alt="GitHub pull requests" src="https://img.shields.io/github/issues-pr/LordOfTrident/tokiscript?style=for-the-badge&color=4f79e4">
	</a>
</p>

A simple dynamically typed interpreted language i decided to write in C for fun and practice.

Clone this repo with
```sh
$ git clone --recurse-submodules https://github.com/LordOfTrident/tokiscript
```

## Table of contents
* [Quickstart](#quickstart)
* [Features](#features)
* [Documentation](#documentation)
* [Editors](#editors)
* [Bugs](#bugs)

## Quickstart
```sh
$ cc build.c -o build
$ ./build
$ ./build install
$ tokiscript tests/hello_world.toki
```

See build usage with `./build -h`

## Features
- [X] Lexer
- [X] Parser
- [X] Interpreter
- [X] Printing
- [X] Expressions
- [X] Variables
- [X] If statements
- [ ] Else and elif statements
- [ ] Proper type checking
- [ ] Scopes
- [ ] While loops
- [ ] For loops
- [ ] Functions

## Documentation
Coming soon

## Editors
Syntax highlighting configs for text editors are in the [`./editors`](./editors) folder

## Bugs
If you find any bugs, please create an issue and report them.
