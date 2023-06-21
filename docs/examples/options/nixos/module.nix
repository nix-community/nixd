{ config, pkgs, ... }:

{

  # <-- Can you see completion list here?

  fileSystems."/" =
    {
      device = "/dev/disk/by-label/nixos";
      fsType = "btrfs";
    };

  fileSystems."/boot" =
    {
      device = "/dev/disk/by-label/EFI";
      fsType = "vfat";
    };

  boot.loader.grub.devices = [ "/foo" ];
}
