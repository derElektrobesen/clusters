#!/usr/bin/env tarantool

local listen = "5000"
local instance_dir = "./tarantool_data"

box.cfg {
	pid_file = instance_dir .. "/tarantool.pid",
	wal_dir = instance_dir,
	snap_dir = instance_dir,
	sophia_dir = instance_dir,
	logger = instance_dir .. "/logs",
	username = "p.bereznoy",
	listen = listen,
}

-- Assume, that user hasn't `execute' permission by default
-- Tarantool version, installed on cluster (Centos-5) not support 'if_not_exists'
-- TODO: update tarantool version
-- box.schema.user.grant('guest', 'read,write,execute', 'universe', nil, { if_not_exists = true })
box.schema.user.revoke('guest', 'read,write,execute', 'universe')
box.schema.user.grant('guest', 'read,write,execute', 'universe')

local json = require("json")
local log = require("log")

tuple_space = {
	_rd = function ()
		-- Get tuple from tuple space and return to client
		-- TODO
		return {}
	end,

	_in = function ()
		-- Get tuple from tuple space, remove it from tuple space and return to client
		-- TODO
		return {}
	end,

	_out = function ()
		-- Add tuple into tuple space (tuple may be duplicated)
		-- Return number of tuples in tuple space
		return 0
	end,
}
