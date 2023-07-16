d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), \
		coordinator.cc coordinatorserver.cc)

PROTOS += $(addprefix $(d), \
	    commit-proto.proto)

OBJS-coordinator := $(o)commit-proto.o $(o)coordinator.o \
					$(LIB-message) $(LIB-configuration)

$(d)coorserver:  $(LIB-udptransport) $(OBJS-coordinator) $(o)coordinatorserver.o

BINS += $(d)coorserver