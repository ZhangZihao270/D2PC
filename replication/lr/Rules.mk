d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), \
		log.cc client.cc replica.cc)

PROTOS += $(addprefix $(d), \
	    lr-proto.proto)

OBJS-lr-client :=  $(o)lr-proto.o $(o)client.o \
                   $(LIB-message) \
                   $(LIB-configuration)

OBJS-lr-replica := $(o)log.o $(o)replica.o $(o)lr-proto.o\
                   $(OBJS-replica) $(LIB-message) $(OBJS-coordinator)\
                   $(LIB-configuration) $(LIB-persistent_register) $(LIB-store-common)