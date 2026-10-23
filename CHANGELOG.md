# Changelog

## [0.4.0](https://github.com/eduardoborges/claudio/compare/v0.3.0...v0.4.0) (2026-10-23)


### Features

* **daemon:** send the name of the task that just finished ([66a5565](https://github.com/eduardoborges/claudio/commit/66a55654518b4683d72b8a50a28f5f705b33d45f))
* **ui:** check the sessions that finished in the last minute ([28a2a52](https://github.com/eduardoborges/claudio/commit/28a2a5267f89f37311d524d86d5e858c2c62f52a))
* **ui:** show the finished task under the dancing Clawd ([f0ede53](https://github.com/eduardoborges/claudio/commit/f0ede5388cd9febebbccd080292a62f6d24c2b54))


### Bug Fixes

* **daemon:** count a finished session's minutes from when it finished ([2c4cfdf](https://github.com/eduardoborges/claudio/commit/2c4cfdf40da98ebd5b407788962d17374741965f))

## [0.3.0](https://github.com/eduardoborges/claudio/compare/v0.2.0...v0.3.0) (2026-10-16)


### ⚠ BREAKING CHANGES

* rename the device and the daemons to Claudio

### Features

* **mac:** app icon with Clawd ([112789f](https://github.com/eduardoborges/claudio/commit/112789fd3a388e017ba6e2c7bedf98cc18441846))


### Miscellaneous Chores

* rename the device and the daemons to Claudio ([3f30b3c](https://github.com/eduardoborges/claudio/commit/3f30b3cc407bb559d663964966119c6f27247906))

## [0.2.0](https://github.com/eduardoborges/tally/compare/v0.1.0...v0.2.0) (2026-10-16)


### Features

* **mac:** install the Claude Code hooks from the app ([e0cc6c4](https://github.com/eduardoborges/tally/commit/e0cc6c43e0af41f68ecece37ee785cef0163135b))
* **mac:** welcome guide that flashes and pairs the display ([088dd43](https://github.com/eduardoborges/tally/commit/088dd437d55a5cd552b510cb0f706d633f7f6942))


### Bug Fixes

* **mac:** mark the welcome guide seen only when its window opens ([d534f2b](https://github.com/eduardoborges/tally/commit/d534f2b429d40faa923b1e7d0825ff17336980cb))
* **mac:** write settings.json the way Claude Code formats it ([bce05c5](https://github.com/eduardoborges/tally/commit/bce05c5c8f5362d85f9ef595a9a24824278bb166))

## 0.1.0 (2026-10-16)


### ⚠ BREAKING CHANGES

* rename the device and the daemons to Tally

### Features

* **mac:** add a menu bar app skeleton ([8aeb089](https://github.com/eduardoborges/tally/commit/8aeb089ab0cc773dbed840615e6d2721e3a24d00))
* **mac:** add the menu bar panel ([c129911](https://github.com/eduardoborges/tally/commit/c129911ca4b239ab1a429e743f0197426e5270d9))
* **mac:** Claude's spark in the menu bar ([d66a314](https://github.com/eduardoborges/tally/commit/d66a314180047107399bf05849079cad09b0928f))
* **mac:** Clawd standing still as the resting icon ([e24398c](https://github.com/eduardoborges/tally/commit/e24398ca8545e13f8d4ca048486bbebe271fe7cf))
* **mac:** Clawd typing on a laptop while a session works ([19e7672](https://github.com/eduardoborges/tally/commit/19e76729c3b6706d6be375d06c78dd97827e8200))
* **mac:** connect to the display over Bluetooth ([f7a0ca7](https://github.com/eduardoborges/tally/commit/f7a0ca7c60c3c16d23730b4efe1958bc720ffe2f))
* **mac:** flash firmware zips ([06e5d31](https://github.com/eduardoborges/tally/commit/06e5d3179fad2fa4a9d214527b5835ec008885b7))
* **mac:** Liquid Glass on macOS 26 ([d167fce](https://github.com/eduardoborges/tally/commit/d167fce9e84327d9fc513fd82576ff5518e74532))
* **mac:** open at login ([02623f5](https://github.com/eduardoborges/tally/commit/02623f566aba9e8eb0c458ef2b4520c91abd40d1))
* **mac:** port the attention alerts ([3ec1b76](https://github.com/eduardoborges/tally/commit/3ec1b76b470e7213e87b5a3354642fc849b72b13))
* **mac:** port the Sessions message ([925f5a0](https://github.com/eduardoborges/tally/commit/925f5a00286504dc4112c2f517642f81517fd788))
* **mac:** port the Stats messages ([20dc362](https://github.com/eduardoborges/tally/commit/20dc362db5cbddcd391ca7ad84232af6d6156aa1))
* **mac:** port the Today message ([b0a7922](https://github.com/eduardoborges/tally/commit/b0a7922ef0504643ff8d41ed63633f7d0a12ca9a))
* **mac:** port the token and usage polling ([25e3a6b](https://github.com/eduardoborges/tally/commit/25e3a6b8589304a64aa5ccc148f8fd81adb052bc))
* **mac:** run the daemon's loop in the app ([ceecfb5](https://github.com/eduardoborges/tally/commit/ceecfb54773df7a35ef0d986b0e26c5f752f1d83))
* **mac:** settings window and menu bar usage ([948eb24](https://github.com/eduardoborges/tally/commit/948eb24942579733e3660bc89b5af5b573ac3263))
* **mac:** write the config file and read the hooks ([8c26c72](https://github.com/eduardoborges/tally/commit/8c26c72fd9121dd8e656d9a380fe8b55bb7abee3))


### Bug Fixes

* **mac:** build with Xcode 26, whose compiler timed out on the heatmap encoder ([1c759a5](https://github.com/eduardoborges/tally/commit/1c759a5cd9b74a28cb7d78e338a72938dfb3400a))
* **mac:** fit the panel window to its content ([10c73a8](https://github.com/eduardoborges/tally/commit/10c73a875304d8591829c2f169fd29bb8d0e1a56))
* **mac:** hide the focus ring on the panel tabs ([455774f](https://github.com/eduardoborges/tally/commit/455774f25187735ef05406112b3795bbc59d00bf))
* **mac:** no flicker when the panel grows ([1795355](https://github.com/eduardoborges/tally/commit/1795355980a6c9bf28ef23450f0eb828ec9bc8df))
* **mac:** steady, monochrome menu bar icon ([1d7837f](https://github.com/eduardoborges/tally/commit/1d7837f5abf3e7228312cc61edd9621e687a60b3))


### Miscellaneous Chores

* rename the device and the daemons to Tally ([50ab3ec](https://github.com/eduardoborges/tally/commit/50ab3ece3581c291019655a5b27b51cd289f47e4))
