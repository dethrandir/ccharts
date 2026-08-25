-- helper.lua — shared test harness for the ccharts Lua binding.
--
-- Provides a tiny dependency-free test framework (TinyTest) plus a minimal
-- pure-Lua JSON decoder so the conformance suite can read
-- conformance/cases.json with no external modules.

-- Make src/ (facade) and ./build (compiled C module) findable.
local here = debug.getinfo(1, "S").source:sub(2)
local bindir = here:match("^(.*/)")
package.path = bindir .. "../src/?.lua;" .. bindir .. "../src/?/init.lua;" .. package.path
package.cpath = bindir .. "../build/?.so;" .. bindir .. "../build/?/init.so;" .. package.cpath

-- ----------------------------- TinyTest -----------------------------
local TinyTest = {}
TinyTest.__index = TinyTest

local tests = {}
local function add(name, blk) tests[#tests + 1] = { name = name, blk = blk } end

function test(name, blk) add(name, blk) end

-- Alias so tests can call either `test "name" do ... end`
-- (handled by the global `test` function above).
_G.test = test

function TinyTest.run()
  local ran, failed = 0, 0
  local failures = {}
  for _, t in ipairs(tests) do
    ran = ran + 1
    local ok, err = pcall(t.blk)
    if ok then
      print("PASS  " .. t.name)
    else
      failed = failed + 1
      failures[#failures + 1] = t.name
      print("FAIL  " .. t.name .. ": " .. tostring(err))
    end
  end
  print(string.format("%d/%d tests passed", ran - failed, ran))
  if failed > 0 then
    error("failed: " .. table.concat(failures, ", "))
  end
end

_G.TinyTest = TinyTest

-- --------------------------- assertions -----------------------------
function assert_equal(expected, actual, msg)
  if not (expected == actual) then
    error(string.format("expected %q, got %q%s",
                        tostring(expected), tostring(actual),
                        msg and (": " .. msg) or ""))
  end
end

_G.assert_equal = assert_equal

-- -------------------------- JSON decoder ----------------------------
-- A minimal, correct-enough JSON decoder for cases.json (objects, arrays,
-- strings with escapes, numbers, true/false/null).
local json = {}
json.__index = json

function json.decode(s)
  local pos = 1
  local function skip_ws()
    while pos <= #s do
      local c = s:sub(pos, pos)
      if c == " " or c == "\t" or c == "\n" or c == "\r" then pos = pos + 1 else break end
    end
  end

  local function parse_unicode4()
    local hex = s:sub(pos, pos + 3)
    pos = pos + 4
    return tonumber(hex, 16)
  end

  local function utf8_char(cp)
    if cp < 0x80 then return string.char(cp) end
    if cp < 0x800 then
      return string.char(0xC0 + math.floor(cp / 0x40),
                         0x80 + (cp % 0x40))
    end
    return string.char(0xE0 + math.floor(cp / 0x1000),
                       0x80 + (math.floor(cp / 0x40) % 0x40),
                       0x80 + (cp % 0x40))
  end

  local function parse_string()
    -- assumes current char is '"'
    pos = pos + 1
    local out = {}
    while pos <= #s do
      local c = s:sub(pos, pos)
      if c == '"' then pos = pos + 1; return table.concat(out) end
      if c == "\\" then
        pos = pos + 1
        local e = s:sub(pos, pos)
        if e == '"' then out[#out + 1] = '"'; pos = pos + 1
        elseif e == "\\" then out[#out + 1] = "\\"; pos = pos + 1
        elseif e == "/" then out[#out + 1] = "/"; pos = pos + 1
        elseif e == "b" then out[#out + 1] = "\b"; pos = pos + 1
        elseif e == "f" then out[#out + 1] = "\f"; pos = pos + 1
        elseif e == "n" then out[#out + 1] = "\n"; pos = pos + 1
        elseif e == "r" then out[#out + 1] = "\r"; pos = pos + 1
        elseif e == "t" then out[#out + 1] = "\t"; pos = pos + 1
        elseif e == "u" then
          pos = pos + 1
          local cp = parse_unicode4()
          if cp >= 0xD800 and cp <= 0xDBFF then -- high surrogate: expect low
            pos = pos + 1 -- skip "\"
            pos = pos + 1 -- skip "u"
            local lo = parse_unicode4()
            cp = 0x10000 + (cp - 0xD800) * 0x400 + (lo - 0xDC00)
          end
          out[#out + 1] = utf8_char(cp)
        else
          out[#out + 1] = "\\" .. e; pos = pos + 1
        end
      else
        out[#out + 1] = c
        pos = pos + 1
      end
    end
    error("unterminated string in JSON")
  end

  local function parse_number()
    local start = pos
    if s:sub(pos, pos) == "-" then pos = pos + 1 end
    while s:sub(pos, pos):match("%d") do pos = pos + 1 end
    if s:sub(pos, pos) == "." then
      pos = pos + 1
      while s:sub(pos, pos):match("%d") do pos = pos + 1 end
    end
    if s:sub(pos, pos):match("[eE]") then
      pos = pos + 1
      if s:sub(pos, pos):match("[+-]") then pos = pos + 1 end
      while s:sub(pos, pos):match("%d") do pos = pos + 1 end
    end
    return tonumber(s:sub(start, pos - 1))
  end

  local function parse_value()
    skip_ws()
    local c = s:sub(pos, pos)
    if c == "{" then
      pos = pos + 1
      local obj = {}
      skip_ws()
      if s:sub(pos, pos) == "}" then pos = pos + 1; return obj end
      while true do
        skip_ws()
        local key = parse_string()
        skip_ws()
        if s:sub(pos, pos) ~= ":" then error("expected ':' in JSON object") end
        pos = pos + 1
        obj[key] = parse_value()
        skip_ws()
        c = s:sub(pos, pos)
        if c == "," then pos = pos + 1
        elseif c == "}" then pos = pos + 1; return obj
        else error("expected ',' or '}' in JSON object") end
      end
    elseif c == "[" then
      pos = pos + 1
      local arr = {}
      skip_ws()
      if s:sub(pos, pos) == "]" then pos = pos + 1; return arr end
      while true do
        arr[#arr + 1] = parse_value()
        skip_ws()
        c = s:sub(pos, pos)
        if c == "," then pos = pos + 1
        elseif c == "]" then pos = pos + 1; return arr
        else error("expected ',' or ']' in JSON array") end
      end
    elseif c == '"' then
      return parse_string()
    elseif c == "t" then
      if s:sub(pos, pos + 3) == "true" then pos = pos + 4; return true end
      error("invalid JSON token")
    elseif c == "f" then
      if s:sub(pos, pos + 4) == "false" then pos = pos + 5; return false end
      error("invalid JSON token")
    elseif c == "n" then
      if s:sub(pos, pos + 3) == "null" then pos = pos + 4; return nil end
      error("invalid JSON token")
    else
      return parse_number()
    end
  end

  local v = parse_value()
  skip_ws()
  if pos <= #s then error("trailing content in JSON") end
  return v
end

_G.json = json

-- ----------------------- conformance helpers ------------------------
function conformance_dir()
  -- tests/ sits at bindings/<lang>/tests; the shared suite lives at the repo
  -- root, three levels up (tests -> <lang> -> bindings -> root).
  return bindir .. "../../../conformance"
end
_G.conformance_dir = conformance_dir