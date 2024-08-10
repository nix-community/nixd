{
  pkgs ? import <nixpkgs> { },
}:
let
  neovim = pkgs.neovim.override {
    configure = {
      customRC = ''
        lua <<EOF
        ${luaRc}
        EOF
      '';

      packages.myPlugins.start = with pkgs.vimPlugins; [
        (nvim-treesitter.withPlugins (parsers: [ parsers.nix ]))
        friendly-snippets
        luasnip
        nvim-cmp
        cmp-nvim-lsp
        cmp-buffer
        cmp_luasnip
        cmp-path
        cmp-cmdline
        none-ls-nvim
        nvim-lspconfig
        nord-nvim
        noice-nvim
        lualine-nvim
        bufferline-nvim
        lspsaga-nvim
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

    ----------------
    -- nord theme --
    ----------------
    vim.g.nord_contrast = false
    vim.g.nord_borders = true
    vim.g.nord_disable_background = false
    vim.g.nord_italic = true
    vim.g.nord_uniform_diff_background = true
    vim.g.nord_enable_sidebar_background = true
    vim.g.nord_bold = true
    vim.g.nord_cursorline_transparent = false
    require("nord").set()

    -----------------
    -- About noice --
    -----------------
    require("noice").setup({
        routes = {
            {
                filter = {
                    event = "msg_show",
                    any = {
                        { find = "%d+L, %d+B" },
                        { find = "; after #%d+" },
                        { find = "; before #%d+" },
                        { find = "%d fewer lines" },
                        { find = "%d more lines" },
                    },
                },
                opts = { skip = true },
            },
        },
    })

    -------------------
    -- About lualine --
    -------------------
    require("lualine").setup({
        options = {
            theme = "auto",
            globalstatus = true,
        },
    })

    ----------------------
    -- About bufferline --
    ----------------------
    local highlights
    highlights = require("nord").bufferline.highlights({
        italic = true,
        bold = true,
    })
    require("bufferline").setup({
        highlights = highlights,
    })

    ----------------------
    -- About treesitter --
    ----------------------
    require("nvim-treesitter.configs").setup({
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
    	mapping = cmp.mapping.preset.insert({
    		["<C-u>"] = cmp.mapping.scroll_docs(-4), -- Up
    		["<C-d>"] = cmp.mapping.scroll_docs(4), -- Down
    		-- C-b (back) C-f (forward) for snippet placeholder navigation.
    		["<C-Space>"] = cmp.mapping.complete(),
    		["<CR>"] = cmp.mapping.confirm({
    			behavior = cmp.ConfirmBehavior.Replace,
    			select = true,
    		}),
    		["<Tab>"] = cmp.mapping(function(fallback)
    			if cmp.visible() then
    				cmp.select_next_item()
    			elseif luasnip.expand_or_jumpable() then
    				luasnip.expand_or_jump()
    			else
    				fallback()
    			end
    		end, { "i", "s" }),
    		["<S-Tab>"] = cmp.mapping(function(fallback)
    			if cmp.visible() then
    				cmp.select_prev_item()
    			elseif luasnip.jumpable(-1) then
    				luasnip.jump(-1)
    			else
    				fallback()
    			end
    		end, { "i", "s" }),
    	}),
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
    cmp.setup.cmdline(":", {
    	mapping = cmp.mapping.preset.cmdline(),
    	sources = cmp.config.sources({
    		{ name = "path" },
    	}, {
    		{ name = "cmdline" },
    	}),
    })

    -------------------
    -- About none-ls --
    -------------------
    -- format(async)
    local async_formatting = function(bufnr)
    	bufnr = bufnr or vim.api.nvim_get_current_buf()

    	vim.lsp.buf_request(
    		bufnr,
    		"textDocument/formatting",
    		vim.lsp.util.make_formatting_params({}),
    		function(err, res, ctx)
    			if err then
    				local err_msg = type(err) == "string" and err or err.message
    				-- you can modify the log message / level (or ignore it completely)
    				vim.notify("formatting: " .. err_msg, vim.log.levels.WARN)
    				return
    			end

    			-- don't apply results if buffer is unloaded or has been modified
    			if not vim.api.nvim_buf_is_loaded(bufnr) or vim.api.nvim_buf_get_option(bufnr, "modified") then
    				return
    			end

    			if res then
    				local client = vim.lsp.get_client_by_id(ctx.client_id)
    				vim.lsp.util.apply_text_edits(res, bufnr, client and client.offset_encoding or "utf-16")
    				vim.api.nvim_buf_call(bufnr, function()
    					vim.cmd("silent noautocmd update")
    				end)
    			end
    		end
    	)
    end
    local augroup = vim.api.nvim_create_augroup("LspFormatting", {})
    local lsp_formatting = function(bufnr)
    	vim.lsp.buf.format({
    		filter = function(client)
    			-- apply whatever logic you want (in this example, we'll only use null-ls)
    			return client.name == "null-ls"
    		end,
    		bufnr = bufnr,
    	})
    end
    require("null-ls").setup({
    	sources = {
    		-- you must download code formatter by yourself!
    		require("null-ls").builtins.formatting.nixpkgs_fmt,
    	},
    	debug = false,
    	on_attach = function(client, bufnr)
    		if client.supports_method("textDocument/formatting") then
    			vim.api.nvim_clear_autocmds({ group = augroup, buffer = bufnr })
    			vim.api.nvim_create_autocmd("BufWritePost", {
    				group = augroup,
    				buffer = bufnr,
    				callback = function()
    					async_formatting(bufnr)
    					lsp_formatting(bufnr)
    				end,
    			})
    		end
    	end,
    })
    ---------------------
    -- About lspconfig --
    ---------------------
    local nvim_lsp = require("lspconfig")

    -- Add additional capabilities supported by nvim-cmp
    -- nvim hasn't added foldingRange to default capabilities, users must add it manually
    local capabilities = require("cmp_nvim_lsp").default_capabilities()
    capabilities = vim.lsp.protocol.make_client_capabilities()
    capabilities.textDocument.foldingRange = {
    	dynamicRegistration = false,
    	lineFoldingOnly = true,
    }

    --Change diagnostic symbols in the sign column (gutter)
    local signs = { Error = " ", Warn = " ", Hint = " ", Info = " " }
    for type, icon in pairs(signs) do
    	local hl = "DiagnosticSign" .. type
    	vim.fn.sign_define(hl, { text = icon, texthl = hl, numhl = hl })
    end
    vim.diagnostic.config({
    	virtual_text = false,
    	signs = true,
    	underline = true,
    	update_in_insert = true,
    	severity_sort = false,
    })

    local on_attach = function(bufnr)
    	vim.api.nvim_create_autocmd("CursorHold", {
    		buffer = bufnr,
    		callback = function()
    			local opts = {
    				focusable = false,
    				close_events = { "BufLeave", "CursorMoved", "InsertEnter", "FocusLost" },
    				border = "rounded",
    				source = "always",
    				prefix = " ",
    				scope = "line",
    			}
    			vim.diagnostic.open_float(nil, opts)
    		end,
    	})
    end
    nvim_lsp.nixd.setup({
        on_attach = on_attach(),
        capabilities = capabilities,
        settings = {
            nixd = {
                nixpkgs = {
                    expr = "import <nixpkgs> { }",
                },
                formatting = {
                    command = { "nixfmt" },
                },
                options = {
                    nixos = {
                        expr = '(builtins.getFlake "/tmp/NixOS_Home-Manager").nixosConfigurations.hostname.options',
                    },
                    home_manager = {
                        expr = '(builtins.getFlake "/tmp/NixOS_Home-Manager").homeConfigurations."user@hostname".options',
                    },
                    flake_parts = {
                        expr = 'let flake = builtins.getFlake ("/tmp/NixOS_Home-Manager"); in flake.debug.options // flake.currentSystem.options',
                    },
                },
            },
        },
    })

    -------------------
    -- About lspsaga --
    -------------------
    local colors, kind
    colors = { normal_bg = "#3b4252" }
    require("lspsaga").setup({
        ui = {
            colors = colors,
            kind = kind,
            border = "single",
        },
        outline = {
            win_width = 25,
        },
    })
    vim.cmd([[ colorscheme nord ]])

    local keymap = vim.keymap.set

    -- Lsp finder
    -- Find the symbol definition, implementation, reference.
    -- If there is no implementation, it will hide.
    -- When you use action in finder like open, vsplit, then you can use <C-t> to jump back.
    keymap("n", "gh", "<cmd>Lspsaga lsp_finder<CR>", { silent = true, desc = "Lsp finder" })

    -- Code action
    keymap("n", "<leader>ca", "<cmd>Lspsaga code_action<CR>", { silent = true, desc = "Code action" })
    keymap("v", "<leader>ca", "<cmd>Lspsaga code_action<CR>", { silent = true, desc = "Code action" })

    -- Rename
    keymap("n", "gr", "<cmd>Lspsaga rename<CR>", { silent = true, desc = "Rename" })
    -- Rename word in whole project
    keymap("n", "gr", "<cmd>Lspsaga rename ++project<CR>", { silent = true, desc = "Rename in project" })

    -- Peek definition
    keymap("n", "gD", "<cmd>Lspsaga peek_definition<CR>", { silent = true, desc = "Peek definition" })

    -- Go to definition
    keymap("n", "gd", "<cmd>Lspsaga goto_definition<CR>", { silent = true, desc = "Go to definition" })

    -- Show line diagnostics
    keymap("n", "<leader>sl", "<cmd>Lspsaga show_line_diagnostics<CR>", { silent = true, desc = "Show line diagnostics" })

    -- Show cursor diagnostics
    keymap(
        "n",
        "<leader>sc",
        "<cmd>Lspsaga show_cursor_diagnostics<CR>",
        { silent = true, desc = "Show cursor diagnostic" }
    )

    -- Show buffer diagnostics
    keymap("n", "<leader>sb", "<cmd>Lspsaga show_buf_diagnostics<CR>", { silent = true, desc = "Show buffer diagnostic" })

    -- Diagnostic jump prev
    keymap("n", "[e", "<cmd>Lspsaga diagnostic_jump_prev<CR>", { silent = true, desc = "Diagnostic jump prev" })

    -- Diagnostic jump next
    keymap("n", "]e", "<cmd>Lspsaga diagnostic_jump_next<CR>", { silent = true, desc = "Diagnostic jump next" })

    -- Goto prev error
    keymap("n", "[E", function()
        require("lspsaga.diagnostic"):goto_prev({ severity = vim.diagnostic.severity.ERROR })
    end, { silent = true, desc = "Goto prev error" })

    -- Goto next error
    keymap("n", "]E", function()
        require("lspsaga.diagnostic"):goto_next({ severity = vim.diagnostic.severity.ERROR })
    end, { silent = true, desc = "Goto next error" })

    -- Toggle outline
    keymap("n", "ss", "<cmd>Lspsaga outline<CR>", { silent = true, desc = "Toggle outline" })

    -- Hover doc
    keymap("n", "K", "<cmd>Lspsaga hover_doc ++keep<CR>", { silent = true, desc = "Hover doc" })

    -- Incoming calls
    keymap("n", "<Leader>ci", "<cmd>Lspsaga incoming_calls<CR>", { silent = true, desc = "Incoming calls" })

    -- Outgoing calls
    keymap("n", "<Leader>co", "<cmd>Lspsaga outgoing_calls<CR>", { silent = true, desc = "Outgoing calls" })

    -- Float terminal
    keymap("n", "<A-d>", "<cmd>Lspsaga term_toggle<CR>", { silent = true, desc = "Float terminal" })
    keymap("t", "<A-d>", "<cmd>Lspsaga term_toggle<CR>", { silent = true, desc = "Float terminal" })
  '';

in
pkgs.runCommand "nvim-lsp" { } ''
  mkdir -p $out/bin
  ln -s ${neovim}/bin/nvim $out/bin/nvim-lsp
''
