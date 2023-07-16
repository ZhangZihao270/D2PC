d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), occstore.cc lockstore.cc server.cc \
					client.cc shardclient.cc)

PROTOS += $(addprefix $(d), strong-proto.proto)

LIB-strong-store := $(o)occstore.o $(o)lockstore.o

OBJS-strong-store := $(LIB-message) $(LIB-strong-store) $(LIB-store-common) \
	$(LIB-store-backend) $(o)strong-proto.o 

OBJS-strong-client := $(OBJS-lr-client) $(LIB-udptransport) $(LIB-store-frontend) \
	$(LIB-store-common) $(o)strong-proto.o $(o)shardclient.o $(o)client.o

$(d)server: $(LIB-udptransport) $(OBJS-lr-replica) $(OBJS-strong-store) $(o)server.o

BINS += $(d)server
