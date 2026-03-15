{
  pkgs ? import <nixpkgs> { },
}:
let
  neovim = pkgs.neovim.override {
    configure = {
      customRC = "lua <<EOF\n${luaRc}\nEOF";
      packages.myPlugins.start = with pkgs.vimPlugins; [
        (nvim-treesitter.withPlugins (p: [ p.nix ]))
      ];
    };
  };

  luaRc = ''
    vim.opt.number = true
    vim.opt.shiftwidth = 2
    vim.opt.expandtab = true

    local nvim_lsp = vim.lsp
    local flake_expr = 'builtins.getFlake (toString ./.)'
    nvim_lsp.config("nixd", {
      cmd = { "nixd" },
      filetypes = { "nix" },
      root_markers = { 'flake.nix', '.git' },
      settings = {
        nixd = {
          nixpkgs = {
            expr = string.format('import (%s).inputs.nixpkgs { }', flake_expr)
          },
          formatting = { command = { "${pkgs.nixfmt}/bin/nixfmt" } },
          options = {
            nixos = {
              expr = string.format('(%s).nixosConfigurations.hostname.options', flake_expr)
            },
            home_manager = {
              expr = string.format('(%s).homeConfigurations."user@hostname".options', flake_expr)
            },
            flake_parts = {
              expr = string.format('let flake = %s; in flake.debug.options // flake.currentSystem.options', flake_expr)
            },
          },
        },
      },
    })
    nvim_lsp.enable("nixd")

    vim.api.nvim_create_autocmd('LspAttach', {
      callback = function(ev)
        local opts = { buffer = ev.buf }
        vim.keymap.set('n', 'gd', vim.lsp.buf.definition, opts)
        vim.keymap.set('n', 'K', vim.lsp.buf.hover, opts)
        vim.keymap.set('n', 'gr', vim.lsp.buf.references, opts)
        vim.keymap.set('n', '<leader>ca', vim.lsp.buf.code_action, opts)

        -- Enable built-in auto-completion and Format on save
        local client = vim.lsp.get_client_by_id(ev.data.client_id)
        vim.lsp.completion.enable(true, client.id, ev.buf, { autotrigger = true })
        vim.api.nvim_create_autocmd('BufWritePre', {
          buffer = ev.buf,
          callback = function() vim.lsp.buf.format({ bufnr = ev.buf, id = client.id }) end,
        })
      end,
    })

    -- Completion Menu UI (Tab to navigate next, Enter to select)
    vim.keymap.set('i', '<Tab>', function() return vim.fn.pumvisible() == 1 and "<C-n>" or "<Tab>" end, { expr = true })
    vim.keymap.set('i', '<CR>', function() return vim.fn.pumvisible() == 1 and "<C-y>" or "<CR>" end, { expr = true })
  '';
in
pkgs.runCommand "nvim-lsp" { } ''
  mkdir -p $out/bin
  ln -s ${neovim}/bin/nvim $out/bin/nvim-lsp
''
