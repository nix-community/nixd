{ pkgs ? import <nixpkgs> { } }:
let
  neovim = pkgs.neovim.override {
    configure = {
      customRC = ''
        lua <<EOF
        ${luaRc}
        EOF
      '';

      packages.myPlugins.start = with pkgs.vimPlugins; [
        (nvim-treesitter.withPlugins (parsers: [
          parsers.nix
          parsers.json
        ]))
        friendly-snippets
        luasnip
        nvim-cmp
        cmp-nvim-lsp
        cmp-buffer
        cmp_luasnip
        cmp-path
        null-ls-nvim
        nvim-lspconfig
      ];
    };
  };

  # lua
  luaRc = ''
    -------------------
    -- Basic options --
    -------------------
    local options = {
      clipboard = "unnamedplus",
      mouse = "a",
      undofile = true,
      ignorecase = true,
      showmode = false,
      showtabline = 2,
      smartindent = true,
      autoindent = true,
      swapfile = false,
      hidden = true, --default on
      expandtab = true,
      cmdheight = 1,
      shiftwidth = 2, --insert 2 spaces for each indentation
      tabstop = 2, --insert 2 spaces for a tab
      cursorline = true, --Highlight the line where the cursor is located
      cursorcolumn = false,
      number = true,
      numberwidth = 4,
      relativenumber = true,
      scrolloff = 8,
      updatetime = 50, -- faster completion (4000ms default)
    }

    for k, v in pairs(options) do
      vim.opt[k] = v
    end

    ----------------------
    -- About treesitter --
    ----------------------
    require("nvim-treesitter.configs").setup ({
      highlight = {
        enable = true,
      },
      indent = {
        enable = true,
      },
    })

    ---------------
    -- About cmp --
    ---------------
    local cmp_status_ok, cmp = pcall(require, "cmp")
    if not cmp_status_ok then
    	return
    end
    local snip_status_ok, luasnip = pcall(require, "luasnip")
    if not snip_status_ok then
    	return
    end

    require("luasnip/loaders/from_vscode").lazy_load()

    local check_backspace = function()
    	local col = vim.fn.col(".") - 1
    	return col == 0 or vim.fn.getline("."):sub(col, col):match("%s")
    end

    local kind_icons = {
    	Text = "󰊄",
    	Method = "",
    	Function = "󰡱",
    	Constructor = "",
    	Field = "",
    	Variable = "󱀍",
    	Class = "",
    	Interface = "",
    	Module = "󰕳",
    	Property = "",
    	Unit = "",
    	Value = "",
    	Enum = "",
    	Keyword = "",
    	Snippet = "",
    	Color = "",
    	File = "",
    	Reference = "",
    	Folder = "",
    	EnumMember = "",
    	Constant = "",
    	Struct = "",
    	Event = "",
    	Operator = "",
    	TypeParameter = "",
    }
    -- find more here: https://www.nerdfonts.com/cheat-sheet
    cmp.setup({
    	snippet = {
    		expand = function(args)
    			luasnip.lsp_expand(args.body) -- For `luasnip` users.
    		end,
    	},
    	mapping = {
    		["<C-k>"] = cmp.mapping.select_prev_item(),
    		["<C-j>"] = cmp.mapping.select_next_item(),
    		["<C-b>"] = cmp.mapping(cmp.mapping.scroll_docs(-1), { "i", "c" }),
    		["<C-f>"] = cmp.mapping(cmp.mapping.scroll_docs(1), { "i", "c" }),
    		["<C-Space>"] = cmp.mapping(cmp.mapping.complete(), { "i", "c" }),
    		["<C-y>"] = cmp.config.disable, -- Specify `cmp.config.disable` if you want to remove the default `<C-y>` mapping.
    		["<C-e>"] = cmp.mapping({
    			i = cmp.mapping.abort(),
    			c = cmp.mapping.close(),
    		}),
    		-- Accept currently selected item. If none selected, `select` first item.
    		-- Set `select` to `false` to only confirm explicitly selected items.
    		["<CR>"] = cmp.mapping.confirm({ select = true }),
    		["<Tab>"] = cmp.mapping(function(fallback)
    			if cmp.visible() then
    				cmp.select_next_item()
    			elseif luasnip.expandable() then
    				luasnip.expand()
    			elseif luasnip.expand_or_jumpable() then
    				luasnip.expand_or_jump()
    			elseif check_backspace() then
    				fallback()
    			else
    				fallback()
    			end
    		end, {
    			"i",
    			"s",
    		}),
    		["<S-Tab>"] = cmp.mapping(function(fallback)
    			if cmp.visible() then
    				cmp.select_prev_item()
    			elseif luasnip.jumpable(-1) then
    				luasnip.jump(-1)
    			else
    				fallback()
    			end
    		end, {
    			"i",
    			"s",
    		}),
    	},
    	formatting = {
    		fields = { "kind", "abbr", "menu" },
    		format = function(entry, vim_item)
    			vim_item.kind = string.format("%s", kind_icons[vim_item.kind])
    			vim_item.menu = ({
    				path = "[Path]",
    				nvim_lua = "[NVIM_LUA]",
    				nvim_lsp = "[LSP]",
    				luasnip = "[Snippet]",
    				buffer = "[Buffer]",
    			})[entry.source.name]
    			return vim_item
    		end,
    	},
    	sources = {
    		{ name = "path" },
    		{ name = "nvim_lua" },
    		{ name = "nvim_lsp" },
    		{ name = "luasnip" },
    		{ name = "buffer" },
    	},
    	confirm_opts = {
    		behavior = cmp.ConfirmBehavior.Replace,
    		select = false,
    	},
    	window = {
    		completion = cmp.config.window.bordered(),
    		documentation = cmp.config.window.bordered(),
    	},
    	experimental = {
    		ghost_text = false,
    		native_menu = false,
    	},
    })

    local capabilities = require("cmp_nvim_lsp").default_capabilities()

    -------------------
    -- About null-ls --
    -------------------
    require("null-ls").setup({
    	sources = {
    		-- you must download code formatter by yourself!
    		require("null-ls").builtins.formatting.nixpkgs_fmt,
    	},
    })
    ---------------------
    -- About lspconfig --
    ---------------------
    local lsp_formatting = function(bufnr)
    	vim.lsp.buf.format({
    		filter = function(client)
    			-- apply whatever logic you want (in this example, we'll only use null-ls)
    			return client.name == "null-ls"
    		end,
    		bufnr = bufnr,
    	})
    end

    -- if you want to set up formatting on save, you can use this as a callback
    local augroup = vim.api.nvim_create_augroup("LspFormatting", {})

    local on_attach = function(client, bufnr)
    	-- Mappings.
    	-- See `:help vim.lsp.*` for documentation on any of the below functions
    	local bufopts = { noremap = true, silent = true, buffer = bufnr }
    	vim.keymap.set("n", "gD", vim.lsp.buf.declaration, bufopts)
    	vim.keymap.set("n", "gd", vim.lsp.buf.definition, bufopts)
    	vim.keymap.set("n", "K", vim.lsp.buf.hover, bufopts)
    	vim.keymap.set("n", "gi", vim.lsp.buf.implementation, bufopts)
    	vim.keymap.set("n", "<C-k>", vim.lsp.buf.signature_help, bufopts)
    	vim.keymap.set("n", "<space>wa", vim.lsp.buf.add_workspace_folder, bufopts)
    	vim.keymap.set("n", "<space>wr", vim.lsp.buf.remove_workspace_folder, bufopts)
    	vim.keymap.set("n", "<space>wl", function()
    		print(vim.inspect(vim.lsp.buf.list_workspace_folders()))
    	end, bufopts)
    	vim.keymap.set("n", "<space>D", vim.lsp.buf.type_definition, bufopts)
    	vim.keymap.set("n", "<space>rn", vim.lsp.buf.rename, bufopts)
    	vim.keymap.set("n", "<space>ca", vim.lsp.buf.code_action, bufopts)
    	vim.keymap.set("n", "gr", vim.lsp.buf.references, bufopts)
    	-- add to your shared on_attach callback
    	if client.supports_method("textDocument/formatting") then
    		vim.api.nvim_clear_autocmds({ group = augroup, buffer = bufnr })
    		vim.api.nvim_create_autocmd("BufWritePre", {
    			group = augroup,
    			buffer = bufnr,
    			callback = function()
    				lsp_formatting(bufnr)
    			end,
    		})
    	end
    end

    -- Add additional capabilities supported by nvim-cmp
    -- nvim hasn't added foldingRange to default capabilities, users must add it manually
    local capabilities = vim.lsp.protocol.make_client_capabilities()
    capabilities.textDocument.foldingRange = {
    	dynamicRegistration = false,
    	lineFoldingOnly = true,
    }

    local nvim_lsp = require("lspconfig")

    nvim_lsp.nixd.setup({
    	on_attach = on_attach,
    })

    --Change diagnostic symbols in the sign column (gutter)
    local signs = { Error = " ", Warn = " ", Hint = " ", Info = " " }
    for type, icon in pairs(signs) do
    	local hl = "DiagnosticSign" .. type
    	vim.fn.sign_define(hl, { text = icon, texthl = hl, numhl = hl })
    end
    vim.diagnostic.config({
    	-- virtual_text = {
    	--  source = "always", -- Or "if_many"
    	-- },
    	virtual_text = false,
    	signs = true,
    	underline = true,
    	update_in_insert = true,
    	severity_sort = false,
    	float = {
    		source = "always", -- Or "if_many"
    	},
    })
  '';

in
pkgs.runCommand "nvim-lsp" { } ''
  mkdir -p $out/bin
  ln -s ${neovim}/bin/nvim $out/bin/nvim-lsp
''
