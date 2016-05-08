#!/usr/bin/env tarantool

local listen = "10010"
local instance_dir = "./tarantool"

box.cfg {
	pid_file = instance_dir .. "/tarantool.pid",
	wal_dir = instance_dir .. "/data",
	snap_dir = instance_dir .. "/data",
	sophia_dir = instance_dir .. "/data",
	logger = instance_dir .. "/tarantool.log",
	username = "p.bereznoy",
	listen = listen,
	log_level = 5,
	background = true,
}

box.once("initialization", function ()
	box.schema.user.passwd('admin', 'some_pass') -- admin have all privileges
end)

local __types_pri_idx = 1
local __types_tuple_type_id = 2
local __types_tuple_type_str = 3
local __types_tuple_size = 4
local __types_tuple_sizes_list = 5
local __types_max_id = 5

local __tuples_tuple_data_off = 2 -- used to move to user data into tarantool's tuple

local __select_limit = 1000

box.once("space_creation", function ()
	-- Space contains a list of types.
	--	{ 'type_id', 'type_1', 'type_2', ... }
	s = box.schema.space.create('types')
	s:create_index('primary', {
		type = 'tree',
		unique = true,
		parts = {
			__types_pri_idx, 'NUM',		-- just an autoincrement field
		},
	})
	s:create_index('tuple_type_id', {
		type = 'tree',
		unique = false,
		parts = {
			__types_tuple_type_id, 'NUM',	-- tuple type id. Needed to count max type id
		},
	})
	s:create_index('tuple_type_str', {
		type = 'tree',
		unique = false,
		parts = {
			__types_tuple_type_str, 'STR',	-- tuple type str. Needed to find a tuple with some type
		},
	})

	-- Space contains a list of tuples of type specified.
	--	{ 'id', 'type_id', ... }
	-- XXX: set __tuples_tuple_data_off if scheme will be changed
	s = box.schema.space.create('tuples')
	s:create_index('required_unique', {
		type = 'tree',
		parts = {
			1, 'NUM',	-- just an autoincrement key (required by tarantool)
		},
	})
	s:create_index('primary', {
		type = 'tree',
		unique = false,
		parts = {
			2, 'NUM',	-- tuple type
		},
	})
	s:create_index('primary_with_id', {
		type = 'tree',
		unique = true,
		parts = {
			1, 'NUM',
			2, 'NUM',
		},
	})

	s = box.schema.space.create('master')
	s:create_index('primary', {
		type = 'tree',
		unique = true,
		parts = {
			1, 'NUM',
		},
	})
end)

local tuples_data_offset = 2
local Null = {} -- needed to store nil values into table

local json = require("json")
local log = require("log")

local function _error(msg)
	box.error{ code = 789, reason = msg .. " at " .. debug.getinfo(2, 'S').source .. ", line " .. debug.getinfo(2, 'l').currentline }
end

-- LOCAL
Item = {}
function Item:new(item)
	item = item or {}
	setmetatable(item, self)
	self.__index = self
	return item
end

function Item:_type(t)
	if not t and not self.__type then
		_error("Can't get type")
	elseif t then
		self.__type = t
	end
	return self.__type
end

function Item:val(v)
	if not v and not self.__val then
		_error("Can't get val")
	elseif v then
		self.__val = v
	end
	return self.__val
end

function Item:isFormal(f)
	if f ~= nil then
		self.__is_formal = f
	end
	return self.__is_formal
end

function Item:isArr(v)
	if v ~= nil then
		self.__is_arr = v and true or false
	end
	return self.__is_arr
end

function Item:size(s)
	if s ~= nil then
		self.__arr_size = s
	end
	return self.__arr_size or 0
end

function Item:tostr()
	local str = self:_type()
	if self:isFormal() then
		str = "?" .. str
	end
	if self:isArr() then
		str = str .. ":" .. self:size()
	end
	if not self:isFormal() then
		str = str .. "=" .. json.encode(self:val())
	end
	return str
end

local function isArray(v)
	local b, e = string.find(v[1], "array_")
	if b == nil or e == nil or b ~= 1 then
		return nil
	end

	return e + 1
end

local function getArrayTypeAndSize(v)
	local arr_prefix_end = isArray(v)
	if not arr_prefix_end then
		_error("Invalid item found in tuple: " .. json.encode(v))
	end

	local t = string.sub(v[1], arr_prefix_end)
	if t == nil or t == "" then
		_error("Type is expected in array: " .. json.encode(v))
	end

	if type(v[2]) ~= "number" then
		_error("Invalid size type: number is expected: " .. json.encode(v))
	end

	return t, v[2]
end

local function getItem(v)
	local it = Item:new()

	if type(v) ~= "table" then
		_error("Invalid tuple item: " .. json.encode(v))
	end

	if #v == 2 then
		if v[1] == "formal" then
			it:_type(v[2])
			it:isFormal(true)
		else
			-- simple item
			it:_type(v[1])
			it:val(v[2])
		end
		return it
	elseif #v ~= 3 then
		_error("Invalid number of fields in item: " .. json.encode(v))
	end

	if v[1] == "formal" then
		local x = { v[2], v[3] }
		it:isFormal(true)
		it:isArr(isArray(x))

		local t = v[2]
		if it:isArr() then
			local s
			t, s = getArrayTypeAndSize(x)
			it:size(s)
		end

		it:_type(t)
	else
		local t, s = getArrayTypeAndSize(v)
		it:isArr(true)
		it:size(s)
		it:_type(t)

		if type(v[3]) ~= "table" or #v[3] ~= s then
			_error("Invalid items of tuple: " .. json.encode(v))
		end

		it:val(v[3])
	end

	return it
end

-- LOCAL
Tuple = {}
function Tuple:new(t)
	local tuple = {
		items = {},
		size = 0,
		type_id_idx = box.space.types.index.tuple_type_id,
		type_str_idx = box.space.types.index.tuple_type_str,
		tuple_pri_idx = box.space.tuples.index.required_unique,
		tuple_idx = box.space.tuples.index.primary,
		tuple_idx_prm = box.space.tuples.index.primary_with_id,
	}

	log.info("Tuple came: " .. json.encode(t))

	for _, v in ipairs(t) do
		table.insert(tuple.items, getItem(v))
		tuple.size = tuple.size + 1
	end

	if tuple.size == 0 then
		_error("Too short tuple passed")
	end

	setmetatable(tuple, self)
	self.__index = self
	return tuple
end

function Tuple:values()
	if self:hasFormals() then
		_error("Unexpected values() call: formals found in tuple")
	end
	local vals = {}
	for _, v in ipairs(self.items) do
		table.insert(vals, v:val())
	end
	return vals
end

function Tuple:tostr()
	local str = ""
	for _, v in ipairs(self.items) do
		if str ~= "" then
			str = str .. ", "
		end
		str = str .. v:tostr()
	end
	return "{ " .. str .. " }"
end

function Tuple:hasFormals()
	for _, v in ipairs(self.items) do
		if v:isFormal() then
			return true
		end
	end
	return false
end

function Tuple:iterate()
	local types = {}
	local sizes = {}
	for _, v in ipairs(self.items) do
		table.insert(types, (v:isArr() and "*" or "") .. v:_type())
		table.insert(sizes, v:size())
	end

	return function ()
		if #types == 0 then
			return nil
		end

		local str = table.concat(types, ",")
		local szs = { unpack(sizes) }

		table.remove(types)
		table.remove(sizes)

		return {
			str = str,
			sizes = szs,
		}
	end
end

-- type id will be returned here
-- if <exact> arg is set, total tuple length will be also checked
function Tuple:find_type(exact)
	local type_it = self:iterate()
	local _type = type_it()

	if not _type then
		_error("No items in tuple!")
	end

	local found_in_cloud = self.type_str_idx:select({ _type.str })
	if found_in_cloud == nil then
		return nil
	end

	local found_type_id = nil
	local found_type_size = nil
	for _, v in ipairs(found_in_cloud) do
		if #v ~= __types_max_id then
			_error("Unexpected tuple found in clioud: " .. json.encode({ v:unpack() }))
		end

		if not exact or v[__types_tuple_size] == self.size then
			-- types names are equal. Compare subtuples lengths
			local lens = v[__types_tuple_sizes_list]
			if #lens ~= #(_type.sizes) then
				_error("Unexpected number of sizes in tuple: " .. #(_types.sizes) .. " is expected")
			end

			local equal = true
			for i, v in ipairs(lens) do
				if v ~= _type.sizes[i] and (exact or not self.items[i]:isFormal() or v < _type.sizes[i]) then
					-- for non-exact match, when current item is formal,
					-- following sizes should be equal:
					-- lens:	{ 0, 2, 4 }
					-- _type.sizes:	{ 0, 2, 3 }
					--
					-- not formal args should be strictly matched
					equal = false
					break
				end
			end

			local found_tid = v[__types_tuple_type_id]
			local found_tsize = v[__types_tuple_size]

			if equal and exact then
				return found_tid
			elseif equal and (found_type_size == nil or found_type_size > found_tsize) then
				-- for non-exact match, tuple with minimum size will be returned
				found_type_id = found_tid
				found_type_size = found_tsize
			end
		end
	end

	return found_type_id
end

function Tuple:select()
	return self:_select(true)
end

function Tuple:read()
	return self:_select(false)
end

function Tuple:iterateCloud(type_id)
	if type_id == nil then
		_error("Can't iterate over nil key")
	end

	local to_process = {}
	for _, v in ipairs(self.tuple_idx:select({ type_id }, { iterator = 'EQ', limit = __select_limit })) do
		table.insert(to_process, { v[1], v[2] })
	end

	local last_prc_idx = 1

	local __expand = function ()
		if not #to_process then
			return nil
		end

		local last_id = to_process[#to_process]

		last_prc_idx = 1
		to_process = {}

		for _, v in ipairs(self.tuple_idx_prm:select(last_id, { iterator = 'GT', limit = __select_limit })) do
			table.insert(to_process, { v[1], v[2] })
		end

		if not #to_process then
			return nil
		end

		return 1
	end

	return function ()
		if last_prc_idx > #to_process and not __expand() then
			return nil
		end

		local ret = nil
		local idx = nil
		while ret == nil do
			idx = to_process[last_prc_idx]
			ret = self.tuple_pri_idx:delete({ idx[1] }) -- we mark tuple as 'in process'
			last_prc_idx = last_prc_idx + 1
			if ret == nil and last_prc_idx > #to_process and not __expand() then
				return nil
			end
		end

		return idx, { select(__tuples_tuple_data_off + 1, ret:unpack()) }
	end
end

function Tuple:_select(need_remove)
	local type_id = self:find_type(false)
	if type_id == nil then
		log.info("Tuple with type " .. self:tostr() .. " not found in cloud")
		return nil
	end

	-- select all tuples with specified type id and return first matched
	for idx, t in self:iterateCloud(type_id) do
		local ok = true
		for i, item in ipairs(self.items) do
			if not item:isFormal() then
				if item:isArr() then
					for j, v in ipairs(item:val()) do
						if t[i][j] ~= v then
							ok = false
							break
						end
					end
				else
					ok = (item:val() == t[i])
				end
			end
			if not ok then
				break
			end
		end
		if (not need_remove) or (not ok) then
			-- move tuple back in cloud
			-- Double unpack in one stmt works incorrect => hack with indexes
			box.space.tuples:insert({ idx[1], idx[2], unpack(t) })
		end
		if ok then
			return t
		end
	end

	return nil
end

function Tuple:insert()
	local type_id = self:find_type(true) -- exact type should be found
	if type_id == nil then
		-- insert new tuple into cloud
		type_id = self.type_id_idx:max()
		if type_id == nil then
			type_id = 1
		else
			type_id = type_id[2] + 1 -- box.tuple was returned from max()
		end

		local type_str = nil
		for it in self:iterate() do
			if type_str == nil then
				type_str = "types: " .. it.str .. ", sizes: " .. json.encode(it.sizes)
			end

			box.space.types:auto_increment({
				type_id,
				it.str,
				self.size,
				it.sizes,
			})
		end

		log.info("New type inserted into tuple space: id = " .. type_id .. " " .. type_str)
	end

	-- insert new tuple with found type_id
	box.space.tuples:auto_increment({
		type_id,
		unpack(self:values()),
	})

	log.info("New tuple with type id " .. type_id .. " was added into tuple space")

	return type_id
end

tuple_space = {
	add_tuple = function (...)
		log.info("add_tuple was called")
		local tuple = Tuple:new({...})
		if tuple:hasFormals() then
			_error("Unexpected formals in tuple " .. tuple:tostr())
		end

		local type_id = tuple:insert()
		log.info("Returning " .. type_id .. " to client");
	end,

	read_tuple = function (...)
		log.info("read_tuple was called")
		local tuple = Tuple:new({...})

		local tup = tuple:read()
		if tup then
			log.info("Returning tuple to client: " .. json.encode(tup))
		else
			log.info("Returning nil to client")
		end
		return tup
	end,

	get_tuple = function (...)
		log.info("get_tuple was called")
		local tuple = Tuple:new({...})

		local tup = tuple:select()
		if tup then
			log.info("Returning tuple to client: " .. json.encode(tup))
		else
			log.info("Returning nil to client")
		end
		return tup
	end,

	is_master = function ()
		-- TODO: call auto_increment on a global taratntool
		local id = box.space.master:auto_increment{}
		if id[1] == 1 then
			return 1
		end
		return nil
	end,

	cleanup = function ()
		box.space.tuples:truncate()
		box.space.types:truncate()
		box.space.master:truncate()
	end,
}

tuple_space.cleanup() -- TODO: is it really needed ?
