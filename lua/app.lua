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
box.schema.user.grant('guest', 'read,write,execute', 'universe', nil, { if_not_exists = true })

local json = require("json")
local log = require("log")

function test_func(test)
	log.info(json.encode(test))
	return { 1, 2, 6, 9, 18 }
end
