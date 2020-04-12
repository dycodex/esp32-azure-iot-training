# Warning! This code was automatically generated for projects
# without default 'CMakeLists.txt' file.

set(app_sources
	"main.c"
	"iothub_devicemethod_mqtt.c"
)

idf_component_register(SRCS ${app_sources})
