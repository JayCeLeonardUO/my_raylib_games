-- Project-local Neovim config

-- Detect target and command from current file path
local function get_build_info()
  local filepath = vim.fn.expand("%:p")
  local project_root = vim.fn.fnamemodify(vim.fn.findfile("CMakeLists.txt", ".;"), ":p:h")

  -- Check if in src/mylibs/ - run the file directly (uses embedded shebang)
  if filepath:match("src/mylibs/.*%.hpp$") then
    return {
      target = "mylibs_tests",
      cmd = "chmod +x " .. filepath .. " && " .. filepath,
      display = vim.fn.fnamemodify(filepath, ":t"),
    }
  end

  -- Check if in src/scratch/<target>/
  local target = filepath:match("src/scratch/([^/]+)/")
  if target then
    return {
      target = target,
      cmd = "cd " .. project_root .. " && ./build_run.sh " .. target,
      display = target,
    }
  end

  -- Fallback
  return {
    target = "example_game",
    cmd = "cd " .. project_root .. " && ./build_run.sh example_game",
    display = "example_game",
  }
end

-- Build and run in tmux pane (leader+b)
vim.keymap.set("n", "<leader>b", function()
  local info = get_build_info()

  -- Run in a tmux split pane at the bottom
  local tmux_cmd = string.format(
    "tmux split-window -v -l 15 '%s; echo; echo Press enter to close...; read'",
    info.cmd
  )
  vim.fn.system(tmux_cmd)
end, { desc = "Build and run in tmux pane" })

-- Debug with gdbgui (leader+r)
vim.keymap.set("n", "<leader>r", function()
  local info = get_build_info()
  local project_root = vim.fn.fnamemodify(vim.fn.findfile("CMakeLists.txt", ".;"), ":p:h")
  local cmd = string.format("cd %s && ./build_run.sh gdbgui %s 2>/dev/null", project_root, info.target)

  vim.fn.jobstart(cmd, {
    stdout_buffered = true,
    on_exit = function(_, code)
      vim.schedule(function()
        if code == 0 then
          vim.cmd("echo 'gdbgui: http://127.0.0.1:5000'")
        else
          vim.cmd("echohl ErrorMsg | echo 'Build failed!' | echohl None")
        end
      end)
    end,
  })
end, { desc = "Build and debug with gdbgui" })

-- Auto-reload files changed externally
vim.o.autoread = true
vim.api.nvim_create_autocmd({ "FocusGained", "BufEnter", "CursorHold" }, {
  callback = function() vim.cmd("checktime") end,
})

-- LSP code actions (leader+ca)
vim.keymap.set("n", "<leader>ca", vim.lsp.buf.code_action, { desc = "Code actions" })

-- Format on save for C/C++ files using clang-format

vim.api.nvim_create_autocmd("BufWritePre", {
  pattern = { "*.cpp", "*.hpp", "*.c", "*.h" },
  callback = function()
    vim.lsp.buf.format({ async = false })
  end,
})

-- Tab completion (works with nvim-cmp)
local ok, cmp = pcall(require, "cmp")
if ok then
  cmp.setup({
    mapping = cmp.mapping.preset.insert({
      ["<Tab>"] = cmp.mapping(function(fallback)
        if cmp.visible() then
          cmp.select_next_item()
        else
          fallback()
        end
      end, { "i", "s" }),
      ["<S-Tab>"] = cmp.mapping(function(fallback)
        if cmp.visible() then
          cmp.select_prev_item()
        else
          fallback()
        end
      end, { "i", "s" }),
      ["<CR>"] = cmp.mapping.confirm({ select = true }),
    }),
  })
end
