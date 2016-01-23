#!/usr/bin/env tarantool

local listen = "10010"
local instance_dir = "./tarantool_data"

box.cfg {
	pid_file = instance_dir .. "/tarantool.pid",
	wal_dir = instance_dir,
	snap_dir = instance_dir,
	sophia_dir = instance_dir,
	logger = instance_dir .. "/logs",
	username = "p.bereznoy",
	listen = listen,
	log_level = 5,
}

box.once("initialization", function ()
	-- Assume, that user hasn't `execute' permission by default
	box.schema.user.grant('guest', 'read,write,execute', 'universe')
end)

box.once("space_creation", function ()
	-- Space contains a list of types.
	--	{ 'type_id', 'type_1', 'type_2', ... }
	s = box.schema.space.create('types')
	s:create_index('primary', {
		type = 'tree',
		unique = true,
		parts = {
			1, 'NUM',
		},
	})

	-- Space contains a list of tuples of type specified.
	--	{ 'id', 'type_id', ... }
	s = box.schema.space.create('tuples')
	s:create_index('required_unique', {
		type = 'tree',
		parts = {
			1, 'NUM', -- just an autoincrement key (required by tarantool)
		},
	})
	s:create_index('primary', {
		type = 'tree',
		unique = false,
		parts = {
			2, 'NUM',
		},
	})
end)

local json = require("json")
local log = require("log")

tuple_space = {
	-- All this functions takes a tuple, where each element is a pair { "elem_type", "value" }

	__tuple_to_stuff = function (tuple)
		local types = {}
		local values = {}
		for _, v in ipairs(tuple) do
			local cur_val = nil
			local cur_type = nil

			if v[1] == 'TUPLE' then
				-- Tuple type found. Store a tuple type and tuple size
				cur_type = { v[2], v[3] }
				if #v > 3 then
					cur_val = { select(4, unpack(v)) }
				end
			elseif v[1] == 'TYPE' then
				-- Trying to select any value of specified type
				cur_type = v[2]
			else
				-- Other (flat) type found. Just store a type
				cur_type = v[1]
				cur_val = v[2]
			end

			table.insert(types, cur_type)
			table.insert(values, cur_val)
		end

		return types, values
	end,

	__compare_tuple_types = function (expected_types, found_types)
		local max_index = math.max(#expected_types, #found_types)
		local equal = false

		for i, x in ipairs(expected_types) do
			local y = found_types[i]

			if type(x) ~= type(y) then
				break
			end

			if type(x) == 'table' and not tuple_space.__compare_tuple_types(x, y) then
				break
			elseif x ~= y then
				break
			end

			if i == max_index then
				-- If number of types in expected_types greater then number of types
				-- in found_types, we have a partial equation => success compare
				equal = (max_index == #found_types)
				break
			end
		end

		return equal
	end,

	__find_type = function (types_found)
		-- Function searchs a tuple type same with a type of given tuple
		-- Returns an index of this type or nil if not found
		for _, tuple in box.space.types.index.primary:pairs(nil, { iterator = box.index.ALL }) do
			-- remove first tuple element(index) and compare types
			if tuple_space.__compare_tuple_types({ select(2, tuple:unpack()) }, types_found) then
				return tuple[1]
			end
		end

		return nil
	end,

	read_tuple = function (...)
		-- Get tuple from tuple space and return to client
		-- TODO
		log.info("Tuple came into read_tuple: " .. json.encode({...}))
		return {}
	end,

	get_tuple = function (...)
		-- Get tuple from tuple space, remove it from tuple space and return to client
		-- TODO xxx
		log.info("Tuple came into get_tuple: " .. json.encode({...}))
		return {}
	end,

	add_tuple = function (...)
		-- Add tuple into tuple space (tuple may be duplicated)
		-- Return number of tuples in tuple space
		local types, values = tuple_space.__tuple_to_stuff({...})
		local type_id = tuple_space.__find_type(types)

		log.info("Tuple came into add_tuple: " .. json.encode({...}) .. ". Type id is "
			.. (type_id or "not found"))

		if type_id == nil then
			-- Insert a type with new id
			local inserted = box.space.types:auto_increment(types)
			type_id = inserted[1]

			log.info("New type with id " .. type_id .. " added")
		end

		box.space.tuples:auto_increment({ type_id, unpack(values) })

		return 0
	end,
}
