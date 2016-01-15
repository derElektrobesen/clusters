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

local json = require("json")
local log = require("log")

tuple_space = {
	-- All this functions takes a tuple, where each element is a pair { "elem_type", "value" }

	read_tuple = function (...)
		-- Get tuple from tuple space and return to client
		-- TODO
		log.info("Tuple came into read_tuple: " .. json.encode({...}))
		return {}
	end,

	get_tuple = function (...)
		-- Get tuple from tuple space, remove it from tuple space and return to client
		-- TODO
		log.info("Tuple came into get_tuple: " .. json.encode({...}))
		return {}
	end,

	add_tuple = function (...)
		-- Add tuple into tuple space (tuple may be duplicated)
		-- Return number of tuples in tuple space
		-- TODO
		log.info("Tuple came into add_tuple: " .. json.encode({...}))
		return 0
	end,
}
