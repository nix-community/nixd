<div align="center">
  <h1>nixd</code></h1>

  <p>
    <strong>Nix language server</strong>
  </p>
</div>

## About

This is a feature-rich nix language server interoperating with C++ nix.

Some notable features provided by linking with the Nix library include:

- Nixpkgs option support, for all option system (NixOS/home-manager/flake-parts).
- Nixpkgs package complete, lazily evaluated.
- Shared eval caches (flake, file) with your system's Nix.
- Support for cross-file analysis (goto definition to locations in nixpkgs).


## Get Started

You can *try nixd without installation*.
We have tested some working & reproducible [editor environments](/nixd/docs/editors/editors.md).

## Resources

- [Editor Setup](nixd/docs/editor-setup.md) (get basic working language server out of box)
- [Configuration](nixd/docs/configuration.md) (see how to, and which options are tunable)
- [Features](nixd/docs/features.md) (features explanation)
- [Developers' Manual](nixd/docs/dev.md) (internal design, contributing)
- Public Talk by @inclyc at [NixOS CN Meetup #2](https://discourse.nixos.org/t/nix-cn-meetup-2-2025-12-27-28-shanghai/73323), [slides](https://inclyc.github.io/talk-nixf-nixd/), (zh_CN)


## Sponsor this project

### Chinese Users

<details>

中国用户可以通过以下方式支持 nixd 的开发！
赞助的付费项目主要是技术支持，新功能开发/体验！

开源项目编写不易，还请各位老板多多资瓷 :)

- **爱发电**: [@inclyc](https://afdian.com/a/inclyc) 进行定期赞助
- **微信/支付宝**: 扫描二维码进行一次性赞助

|                                  支付宝                                  |                                 微信                                |
| :--------------------------------------------------------------------: | :--------------------------------------------------------------------: |
| ![WeChat](nixd/docs/funding/63fa7571-9a21-4a5c-9785-1b634ed102e3.jpeg) | ![Alipay](nixd/docs/funding/784e933e-31d1-4392-83b6-24f3a62a27c1.jpeg) |

</details>
