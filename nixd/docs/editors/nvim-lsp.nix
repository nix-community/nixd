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

    -------------------
    -- About null-ls --
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
    })

    -- Global mappings.
    -- See `:help vim.diagnostic.*` for documentation on any of the below functions
    vim.keymap.set("n", "<space>e", vim.diagnostic.open_float)
    vim.keymap.set("n", "[d", vim.diagnostic.goto_prev)
    vim.keymap.set("n", "]d", vim.diagnostic.goto_next)
    vim.keymap.set("n", "<space>q", vim.diagnostic.setloclist)

    -- Use LspAttach autocommand to only map the following keys
    -- after the language server attaches to the current buffer
    vim.api.nvim_create_autocmd("LspAttach", {
    	group = vim.api.nvim_create_augroup("UserLspConfig", {}),
    	callback = function(ev)
    		-- Manual, triggered completion is provided by Nvim's builtin omnifunc. For autocompletion, a general purpose autocompletion plugin(.i.e nvim-cmp) is required
    		-- Enable completion triggered by <c-x><c-o>
    		vim.bo[ev.buf].omnifunc = "v:lua.vim.lsp.omnifunc"

    		-- Buffer local mappings.
    		-- See `:help vim.lsp.*` for documentation on any of the below functions
    		local opts = { buffer = ev.buf }
    		vim.keymap.set("n", "gD", vim.lsp.buf.declaration, opts)
    		vim.keymap.set("n", "gd", vim.lsp.buf.definition, opts)
    		vim.keymap.set("n", "K", vim.lsp.buf.hover, opts)
    		vim.keymap.set("n", "gi", vim.lsp.buf.implementation, opts)
    		vim.keymap.set("n", "<C-k>", vim.lsp.buf.signature_help, opts)
    		vim.keymap.set("n", "<space>wa", vim.lsp.buf.add_workspace_folder, opts)
    		vim.keymap.set("n", "<space>wr", vim.lsp.buf.remove_workspace_folder, opts)
    		vim.keymap.set("n", "<space>wl", function()
    			print(vim.inspect(vim.lsp.buf.list_workspace_folders()))
    		end, opts)
    		vim.keymap.set("n", "<space>D", vim.lsp.buf.type_definition, opts)
    		vim.keymap.set("n", "<space>rn", vim.lsp.buf.rename, opts)
    		vim.keymap.set({ "n", "v" }, "<space>ca", vim.lsp.buf.code_action, opts)
    		vim.keymap.set("n", "gr", vim.lsp.buf.references, opts)
    		vim.keymap.set("n", "<space>f", function()
    			vim.lsp.buf.format({ async = true })
    		end, opts)
    	end,
    })
  '';

in
pkgs.runCommand "nvim-lsp" { } ''
  mkdir -p $out/bin
  ln -s ${neovim}/bin/nvim $out/bin/nvim-lsp
''
