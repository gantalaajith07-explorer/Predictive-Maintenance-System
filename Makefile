ARTIFACT = pms_qnx_concepts
SUPERVISOR_ARTIFACT = pms_supervisor_child

PLATFORM ?= x86_64
BUILD_PROFILE ?= debug

CONFIG_NAME ?= $(PLATFORM)-$(BUILD_PROFILE)
OUTPUT_DIR = build/$(CONFIG_NAME)
TARGET = $(OUTPUT_DIR)/$(ARTIFACT)
SUPERVISOR_TARGET = $(OUTPUT_DIR)/$(SUPERVISOR_ARTIFACT)

CC = qcc -Vgcc_nto$(PLATFORM)
LD = $(CC)

INCLUDES += -Isrc
INCLUDES += -Isrc/core
INCLUDES += -Isrc/ipc
INCLUDES += -Isrc/tasks
INCLUDES += -Isrc/processes

LIBS += -lm
LIBS += -lsocket
LIBS += -lpthread

CCFLAGS_release += -O2
CCFLAGS_debug += -g -O0 -fno-builtin
CCFLAGS_coverage += -g -O0 -ftest-coverage -fprofile-arcs
LDFLAGS_coverage += -ftest-coverage -fprofile-arcs
CCFLAGS_profile += -g -O0 -finstrument-functions
LIBS_profile += -lprofilingS

CCFLAGS_all += -Wall -Wextra -fmessage-length=0
CCFLAGS_all += $(CCFLAGS_$(BUILD_PROFILE))
LDFLAGS_all += $(LDFLAGS_$(BUILD_PROFILE))
LIBS_all += $(LIBS_$(BUILD_PROFILE))
DEPS = -Wp,-MMD,$(@:%.o=%.d),-MT,$@

COMMON_SRCS = \
	src/core/pms_runtime.c \
	src/core/pms_model.c \
	src/core/pms_scheduler.c \
	src/ipc/pms_ipc_router.c \
	src/tasks/acquisition_task.c \
	src/tasks/notification_task.c \
	src/tasks/timing_task.c \
	src/processes/logger_process.c

APP_SRCS = src/app_boot.c $(COMMON_SRCS)
SUPERVISOR_SRCS = src/processes/supervisor_child.c

APP_OBJS = $(addprefix $(OUTPUT_DIR)/,$(addsuffix .o, $(basename $(APP_SRCS))))
SUPERVISOR_OBJS = $(addprefix $(OUTPUT_DIR)/,$(addsuffix .o, $(basename $(SUPERVISOR_SRCS))))

$(OUTPUT_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(DEPS) -o $@ $(INCLUDES) $(CCFLAGS_all) $(CCFLAGS) $<

$(TARGET): $(APP_OBJS)
	$(LD) -o $(TARGET) $(LDFLAGS_all) $(LDFLAGS) $(APP_OBJS) $(LIBS_all) $(LIBS)

$(SUPERVISOR_TARGET): $(SUPERVISOR_OBJS)
	$(LD) -o $(SUPERVISOR_TARGET) $(LDFLAGS_all) $(LDFLAGS) $(SUPERVISOR_OBJS) $(LIBS_all) $(LIBS)

all: $(TARGET) $(SUPERVISOR_TARGET)

clean:
	rm -fr $(OUTPUT_DIR)

rebuild: clean all

-include $(APP_OBJS:%.o=%.d)
-include $(SUPERVISOR_OBJS:%.o=%.d)
