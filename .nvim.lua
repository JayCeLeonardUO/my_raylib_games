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

-- Session names (game-run is created by build_run.sh)
local game_session = "game-run"

-- Toggle bottom pane showing game session (Ctrl+T)
vim.keymap.set("n", "<C-t>", function()
  -- Check if we already have a pane showing game-run
  local panes = vim.fn.system("tmux list-panes -F '#{pane_id}:#{pane_current_command}'")
  if panes:match("tmux") then
    -- Pane exists, close it
    vim.fn.system("tmux kill-pane -t {bottom}")
  else
    -- Check if game session exists
    local exists = vim.fn.system("tmux has-session -t " .. game_session .. " 2>/dev/null && echo yes || echo no")
    if exists:match("yes") then
      vim.fn.system("tmux split-window -v -l 30% 'tmux attach -t " .. game_session .. "'")
    else
      vim.notify("No game session running")
    end
  end
end, { desc = "Toggle game terminal pane" })

-- Build and run in tmux pane (leader+b)
vim.keymap.set("n", "<leader>b", function()
  local info = get_build_info()

  -- Run in a tmux floating popup
  local tmux_cmd = string.format(
    "tmux display-popup -w 80%% -h 50%% '%s; exec $SHELL'",
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

-- Auto-reload this config when saved
vim.api.nvim_create_autocmd("BufWritePost", {
  pattern = "*/.nvim.lua",
  callback = function()
    dofile(vim.fn.expand("%:p"))
    vim.notify(".nvim.lua reloaded")
  end,
})

-- LSP code actions (leader+ca)
vim.keymap.set("n", "<leader>ca", vim.lsp.buf.code_action, { desc = "Code actions" })

-- Open current file's directory in system file explorer (leader+x)
vim.keymap.set("n", "<leader>x", function()
  local dir = vim.fn.expand("%:p:h")
  vim.fn.system("xdg-open " .. vim.fn.shellescape(dir))
end, { desc = "Open in file explorer" })

-- Toggle AI autocomplete (leader+a)
vim.keymap.set("n", "<leader>a", function()
  if vim.g.copilot_enabled == nil then vim.g.copilot_enabled = true end
  if vim.fn.exists(":Copilot") == 2 then
    if vim.g.copilot_enabled then
      vim.cmd("Copilot disable")
      vim.g.copilot_enabled = false
      vim.notify("Copilot OFF")
    else
      vim.cmd("Copilot enable")
      vim.g.copilot_enabled = true
      vim.notify("Copilot ON")
    end
  elseif vim.fn.exists(":Codeium") == 2 then
    vim.cmd("Codeium Toggle")
  else
    vim.notify("No AI plugin found")
  end
end, { desc = "Toggle AI autocomplete" })

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
