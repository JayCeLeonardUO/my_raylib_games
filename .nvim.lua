-- Project-local Neovim config
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
