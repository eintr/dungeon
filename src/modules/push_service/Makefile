
MOD_NAME=push_service

sources=mod_skel.c push_api.c push_api_listener.c push_service.c push_service_listener.c protocol_def.pb-c.c

include ../module.inc

prepare:
	protoc-c --c_out=. protocol_def.proto

