MIOT_ENABLE_ATCA ?= 1
MIOT_ENABLE_ATCA_SERVICE ?= 1
MIOT_ENABLE_CONSOLE ?= 1
MIOT_ENABLE_CONFIG_SERVICE ?= 1
MIOT_ENABLE_DNS_SD ?= 1
MIOT_ENABLE_FILESYSTEM_SERVICE ?= 1
MIOT_ENABLE_HTTP_SERVER ?=1
MIOT_ENABLE_I2C ?= 1
MIOT_ENABLE_I2C_GPIO ?= 0
MIOT_ENABLE_JS ?= 1
MIOT_ENABLE_MQTT ?= 1
MIOT_ENABLE_RPC ?= 1
MIOT_ENABLE_RPC_CHANNEL_HTTP ?= 1
MIOT_ENABLE_RPC_CHANNEL_UART ?= 1
MIOT_DEBUG_UART ?= 1
MIOT_ENABLE_UPDATER ?= 1
MIOT_ENABLE_UPDATER_POST ?= 1
MIOT_ENABLE_UPDATER_RPC ?= 1
MIOT_ENABLE_WIFI ?= 1

SYS_CONF_SCHEMA += $(MIOT_SRC_PATH)/miot_http_config.yaml

ifeq "$(MIOT_ENABLE_CONSOLE)" "1"
  SYS_CONF_SCHEMA += $(MIOT_SRC_PATH)/miot_console_config.yaml
  MIOT_SRCS += miot_console.c cs_frbuf.c
  MIOT_FEATURES += -DMIOT_ENABLE_CONSOLE=1
else
  MIOT_FEATURES += -DMIOT_ENABLE_CONSOLE=0
endif

ifeq "$(MIOT_ENABLE_ATCA)" "1"
  ATCA_PATH ?= $(MIOT_PATH)/third_party/cryptoauthlib
  ATCA_LIB = $(BUILD_DIR)/libatca.a

  MIOT_SRCS += miot_atca.c
  MIOT_FEATURES += -DMIOT_ENABLE_ATCA -I$(ATCA_PATH)/lib
  SYS_CONF_SCHEMA += $(MIOT_SRC_PATH)/miot_atca_config.yaml

  ifeq "$(MIOT_ENABLE_RPC)$(MIOT_ENABLE_ATCA_SERVICE)" "11"
    MIOT_SRCS += miot_atca_service.c
    MIOT_FEATURES += -DMIOT_ENABLE_ATCA_SERVICE
  else
    MIOT_FEATURES += -DMIOT_ENABLE_ATCA_SERVICE=0
  endif

$(BUILD_DIR)/atca/libatca.a:
	$(Q) make -C $(ATCA_PATH)/lib \
		CC=$(CC) AR=$(AR) BUILD_DIR=$(BUILD_DIR)/atca \
	  CFLAGS="$(CFLAGS)"

$(ATCA_LIB): $(BUILD_DIR)/atca/libatca.a
	$(Q) cp $< $@
	$(Q) $(OBJCOPY) --rename-section .rodata=.irom0.text $@
	$(Q) $(OBJCOPY) --rename-section .rodata.str1.1=.irom0.text $@
else
  ATCA_LIB =
  MIOT_FEATURES += -DMIOT_ENABLE_ATCA=0
endif

ifeq "$(MIOT_ENABLE_RPC)" "1"
  MIOT_SRCS += mg_rpc.c mg_rpc_channel_ws.c miot_rpc.c
  MIOT_FEATURES += -DMIOT_ENABLE_RPC -DMIOT_ENABLE_RPC_API
  SYS_CONF_SCHEMA += $(MIOT_SRC_PATH)/miot_rpc_config.yaml

ifeq "$(MIOT_ENABLE_CONFIG_SERVICE)" "1"
  MIOT_SRCS += miot_service_config.c miot_service_vars.c
  MIOT_FEATURES += -DMIOT_ENABLE_CONFIG_SERVICE
endif
ifeq "$(MIOT_ENABLE_FILESYSTEM_SERVICE)" "1"
  MIOT_SRCS += miot_service_filesystem.c
  MIOT_FEATURES += -DMIOT_ENABLE_FILESYSTEM_SERVICE
endif
ifeq "$(MIOT_ENABLE_RPC_CHANNEL_HTTP)" "1"
  MIOT_SRCS += mg_rpc_channel_http.c
  MIOT_FEATURES += -DMIOT_ENABLE_RPC_CHANNEL_HTTP
endif
ifeq "$(MIOT_ENABLE_RPC_CHANNEL_UART)" "1"
  MIOT_SRCS += miot_rpc_channel_uart.c
  MIOT_FEATURES += -DMIOT_ENABLE_RPC_CHANNEL_UART
  SYS_CONF_SCHEMA += $(MIOT_SRC_PATH)/miot_rpc_uart_config.yaml
endif

else
  MIOT_FEATURES += -DMIOT_ENABLE_RPC=0
endif # MIOT_ENABLE_RPC

ifeq "$(MIOT_ENABLE_DNS_SD)" "1"
  MIOT_SRCS += miot_mdns.c miot_dns_sd.c
  MIOT_FEATURES += -DMG_ENABLE_DNS -DMG_ENABLE_DNS_SERVER -DMIOT_ENABLE_MDNS -DMIOT_ENABLE_DNS_SD
  SYS_CONF_SCHEMA += $(MIOT_SRC_PATH)/miot_dns_sd_config.yaml
endif

ifeq "$(MIOT_ENABLE_I2C)" "1"
  MIOT_SRCS += miot_i2c.c
  MIOT_FEATURES += -DMIOT_ENABLE_I2C
  SYS_CONF_SCHEMA += $(MIOT_SRC_PATH)/miot_i2c_config.yaml
  ifeq "$(MIOT_ENABLE_I2C_GPIO)" "1"
    MIOT_SRCS += miot_i2c_gpio.c
    MIOT_FEATURES += -DMIOT_ENABLE_I2C_GPIO
    SYS_CONF_SCHEMA += $(MIOT_SRC_PATH)/miot_i2c_gpio_config.yaml
  endif
else
  MIOT_FEATURES += -DMIOT_ENABLE_I2C=0
endif

ifeq "$(MIOT_ENABLE_MQTT)" "1"
  MIOT_SRCS += miot_mqtt.c
  MIOT_FEATURES += -DMIOT_ENABLE_MQTT -DMG_ENABLE_MQTT
  SYS_CONF_SCHEMA += $(MIOT_SRC_PATH)/miot_mqtt_config.yaml
else
  MIOT_FEATURES += -DMIOT_ENABLE_MQTT=0 -DMG_ENABLE_MQTT=0
endif

ifneq "$(MIOT_ENABLE_UPDATER)$(MIOT_ENABLE_UPDATER_POST)$(MIOT_ENABLE_UPDATER_RPC)" "000"
  SYS_CONF_SCHEMA += $(MIOT_SRC_PATH)/miot_updater_config.yaml
  MIOT_SRCS += miot_updater_common.c miot_updater_http.c
  MIOT_FEATURES += -DMIOT_ENABLE_UPDATER=1
ifeq "$(MIOT_ENABLE_UPDATER_POST)" "1"
  MIOT_FEATURES += -DMIOT_ENABLE_UPDATER_POST=1
  SYS_CONF_SCHEMA += $(MIOT_SRC_PATH)/miot_updater_post.yaml
endif
ifeq "$(MIOT_ENABLE_UPDATER_RPC)" "1"
  MIOT_SRCS += miot_updater_rpc.c
  MIOT_FEATURES += -DMIOT_ENABLE_UPDATER_RPC=1
endif
endif

ifeq "$(MIOT_ENABLE_WIFI)" "1"
  SYS_CONF_SCHEMA += $(MIOT_SRC_PATH)/miot_wifi_config.yaml
  MIOT_SRCS += miot_wifi.c
  MIOT_FEATURES += -DMIOT_ENABLE_WIFI=1
else
  MIOT_FEATURES += -DMIOT_ENABLE_WIFI=0
endif

ifeq "$(MIOT_ENABLE_HTTP_SERVER)" "0"
  MIOT_FEATURES += -DMIOT_ENABLE_HTTP_SERVER=0
else
  MIOT_FEATURES += -DMIOT_ENABLE_HTTP_SERVER=1
endif

# Export all the feature switches.
# This is required for needed make invocations, such as when building POSIX MIOT
# for JS freeze operation.
export MIOT_ENABLE_ATCA
export MIOT_ENABLE_ATCA_SERVICE
export MIOT_ENABLE_CONFIG_SERVICE
export MIOT_ENABLE_CONSOLE
export MIOT_ENABLE_DNS_SD
export MIOT_ENABLE_FILESYSTEM_SERVICE
export MIOT_ENABLE_I2C
export MIOT_ENABLE_I2C_GPIO
export MIOT_ENABLE_JS
export MIOT_ENABLE_MQTT
export MIOT_ENABLE_RPC
export MIOT_ENABLE_RPC_CHANNEL_HTTP
export MIOT_ENABLE_RPC_CHANNEL_UART
export MIOT_ENABLE_UPDATER
export MIOT_ENABLE_UPDATER_POST
export MIOT_ENABLE_UPDATER_RPC
export MIOT_ENABLE_WIFI
export MIOT_ENABLE_HTTP_SERVER
