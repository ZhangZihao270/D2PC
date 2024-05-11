d := $(dir $(lastword $(MAKEFILE_LIST)))

# SRCS += $(addprefix $(d), benchClient.cc retwisClient.cc terminalClient.cc, tpccTransaction.cc, tpccData.cc, tpccClient.cc)

# SRCS += $(addprefix $(d), benchClient.cc retwisClient.cc terminalClient.cc tpccData.cc)

SRCS += $(addprefix $(d), benchClient.cc retwisClient.cc terminalClient.cc tpccTransaction.cc tpccClient.cc tpccData.cc)

OBJS-all-clients := $(OBJS-strong-client) $(OBJS-weak-client) $(OBJS-tapir-client)
# $(OBJS-tpcc-client) 
# $(OBJS-tpcc-data)

$(d)benchClient: $(OBJS-all-clients) $(o)benchClient.o

$(d)retwisClient: $(OBJS-all-clients) $(o)retwisClient.o

$(d)terminalClient: $(OBJS-all-clients) $(o)terminalClient.o

$(d)tpccData: $(OBJS-all-clients) $(o)tpccData.o

$(d)tpccClient: $(OBJS-all-clients) $(o)tpccTransaction.o $(o)tpccClient.o

# BINS += $(d)benchClient $(d)retwisClient $(d)terminalClient $(d)tpccData $(d)tpccClient

# BINS += $(d)benchClient $(d)retwisClient $(d)terminalClient $(d)tpccData

BINS += $(d)benchClient $(d)retwisClient $(d)terminalClient $(d)tpccClient $(d)tpccData
